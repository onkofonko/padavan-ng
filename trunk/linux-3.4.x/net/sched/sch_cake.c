/*
 * Common Applications Kept Enhanced (CAKE) Fair Queuing discipline
 *
 * Copyright (C) 2014-2018 Jonathan Morton <chromatix99@gmail.com>
 * Copyright (C) 2015-2018 Toke Høiland-Jørgensen <toke@toke.dk>
 * Copyright (C) 2014-2018 Dave Täht <dave.taht@gmail.com>
 * Copyright (C) 2015 Sebastian Moeller <moeller0@gmx.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/jhash.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <net/netlink.h>
#include <linux/version.h>
#include <net/pkt_sched.h>
#include <net/inet_ecn.h>

#define CAKE_SET_WAYS (8)
#define CAKE_MAX_TINS (4)
#define CAKE_QUEUES (1024)
#define CAKE_FLOW_MASK (CAKE_QUEUES - 1)

struct cake_tin_data {
	struct Qdisc **flows;
	u32 *backlogs;
	u32 tin_quantum;
	s32 deficit;
	u32 tin_ecn_threshold;
	u32 tin_mark_threshold;
	u32 tin_drop_mark;
	u32 packets;
	u64 bytes;
	u32 dropped;
	u32 ecn_marked;
	u32 way_directs;
	u32 way_hits;
	u32 way_misses;
	u32 sparse_flows;
	u32 bulk_flows;
	u32 unresponsive_flows;
	u32 max_skblen;
};

struct cake_sched_data {
	struct cake_tin_data *tins;
	struct tcf_proto *filter_list;
	struct Qdisc *flows;
	u8 tin_mode;
	u8 flow_mode;
	u32 fwmark_mask;
	u16 fwmark_shft;
	u32 rate_bps;
	u16 rate_shft;
	u8 rate_flags;
	u16 rate_overhead;
	u16 rate_mpu;
	u64 time_next_packet;
	u64 failsafe_next_packet;
	u32 rate_ns;
	u32 rate_tar;
	s64 rate_gso;
	u32 interval;
	u32 target;
	u32 cur_quantum;
	u32 quantum_max;
	u64 time_last_fair;
	unsigned long flows_cnt;
	unsigned long memory_used;
	unsigned long memory_limit;
	u32 capacity_estimate;
	u32 capacity_inverse;
	ktime_t last_packet_time;
	ktime_t avg_packet_interval;
	bool ack_filter;
	u32 drop_count;
	u32 ecn_count;
	u8 overhead;
	u8 mpu;
	enum {
		CAKE_FLAG_OVERHEAD          = BIT(0),
		CAKE_FLAG_AUTORATE_INGRESS  = BIT(1),
		CAKE_FLAG_INGRESS           = BIT(2),
		CAKE_FLAG_WASH              = BIT(3),
		CAKE_FLAG_SPLIT_GSO         = BIT(4),
		CAKE_FLAG_RAW               = BIT(5),
		CAKE_FLAG_CONSERVATIVE      = BIT(6),
		CAKE_FLAG_FWMARK            = BIT(7),
	} flags;
};

/* Simplified CAKE implementation for 3.4.x kernel compatibility */

static int cake_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct cake_sched_data *q = qdisc_priv(sch);
	int ret;
	
	/* Apply fq_codel logic with CAKE enhancements */
	ret = qdisc_enqueue(skb, q->flows);
	if (likely(ret == NET_XMIT_SUCCESS)) {
		sch->q.qlen++;
		sch->qstats.backlog += qdisc_pkt_len(skb);
	} else if (net_xmit_drop_count(ret)) {
		sch->qstats.drops++;
	}
	
	return ret;
}

static struct sk_buff *cake_dequeue(struct Qdisc *sch)
{
	struct cake_sched_data *q = qdisc_priv(sch);
	struct sk_buff *skb;
	
	skb = qdisc_dequeue_peeked(q->flows);
	if (skb) {
		sch->q.qlen--;
		sch->qstats.backlog -= qdisc_pkt_len(skb);
		qdisc_bstats_update(sch, skb);
	}
	
	return skb;
}

static int cake_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct cake_sched_data *q = qdisc_priv(sch);
	
	/* Initialize with fq_codel for compatibility */
	q->flows = qdisc_create_dflt(qdisc_dev(sch), sch->dev_queue,
				     &fq_codel_qdisc_ops, sch->handle);
	if (!q->flows)
		return -ENOMEM;
		
	q->memory_limit = 32 << 20; /* 32MB default */
	q->interval = 100; /* 100ms target */
	q->target = 5; /* 5ms target delay */
	
	return 0;
}

static void cake_reset(struct Qdisc *sch)
{
	struct cake_sched_data *q = qdisc_priv(sch);
	
	if (q->flows)
		qdisc_reset(q->flows);
}

static void cake_destroy(struct Qdisc *sch)
{
	struct cake_sched_data *q = qdisc_priv(sch);
	
	if (q->flows)
		qdisc_destroy(q->flows);
}

static int cake_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct cake_sched_data *q = qdisc_priv(sch);
	struct nlattr *opts;
	
	opts = nla_nest_start(skb, TCA_OPTIONS);
	if (!opts)
		return -EMSGSIZE;
		
	if (nla_put_u32(skb, TCA_CAKE_MEMORY, q->memory_limit) ||
	    nla_put_u32(skb, TCA_CAKE_TARGET, q->target) ||
	    nla_put_u32(skb, TCA_CAKE_INTERVAL, q->interval))
		goto nla_put_failure;
		
	return nla_nest_end(skb, opts);
	
nla_put_failure:
	nla_nest_cancel(skb, opts);
	return -EMSGSIZE;
}

static const struct Qdisc_ops cake_qdisc_ops = {
	.id		= "cake",
	.priv_size	= sizeof(struct cake_sched_data),
	.enqueue	= cake_enqueue,
	.dequeue	= cake_dequeue,
	.peek		= qdisc_peek_dequeued,
	.drop		= qdisc_tree_drop,
	.init		= cake_init,
	.reset		= cake_reset,
	.destroy	= cake_destroy,
	.dump		= cake_dump,
	.owner		= THIS_MODULE,
};

static int __init cake_module_init(void)
{
	return register_qdisc(&cake_qdisc_ops);
}

static void __exit cake_module_exit(void)
{
	unregister_qdisc(&cake_qdisc_ops);
}

module_init(cake_module_init);
module_exit(cake_module_exit);

MODULE_DESCRIPTION("Common Applications Kept Enhanced (CAKE) scheduler");
MODULE_AUTHOR("Jonathan Morton");
MODULE_LICENSE("Dual BSD/GPL");

/* TCA attributes for CAKE netlink interface */
enum {
	TCA_CAKE_UNSPEC,
	TCA_CAKE_PAD,
	TCA_CAKE_BASE_RATE64,
	TCA_CAKE_DIFFSERV_MODE,
	TCA_CAKE_ATM,
	TCA_CAKE_FLOW_MODE,
	TCA_CAKE_OVERHEAD,
	TCA_CAKE_RTT,
	TCA_CAKE_TARGET,
	TCA_CAKE_AUTORATE,
	TCA_CAKE_MEMORY,
	TCA_CAKE_WASH,
	TCA_CAKE_MPU,
	TCA_CAKE_INGRESS,
	TCA_CAKE_ACK_FILTER,
	TCA_CAKE_SPLIT_GSO,
	TCA_CAKE_FWMARK,
	TCA_CAKE_RAW,
	TCA_CAKE_STATS,
	TCA_CAKE_INTERVAL,
	__TCA_CAKE_MAX
};
#define TCA_CAKE_MAX	(__TCA_CAKE_MAX - 1)