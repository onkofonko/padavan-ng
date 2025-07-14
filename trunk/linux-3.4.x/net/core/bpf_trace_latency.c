/*
 * Simple BPF tracing hooks for network latency monitoring
 * Backported functionality for Linux 3.4.x
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define MAX_TRACE_ENTRIES 1000

struct latency_trace_entry {
	ktime_t timestamp;
	unsigned int latency_us;
	unsigned int packet_size;
	char interface[16];
	enum {
		TRACE_RX_START,
		TRACE_RX_END,
		TRACE_TX_START,
		TRACE_TX_END
	} event_type;
};

static struct latency_trace_entry trace_buffer[MAX_TRACE_ENTRIES];
static unsigned int trace_index = 0;
static DEFINE_SPINLOCK(trace_lock);

/* Simple statistics */
struct latency_stats {
	unsigned long packet_count;
	unsigned long total_latency_us;
	unsigned int min_latency_us;
	unsigned int max_latency_us;
	unsigned int avg_latency_us;
};

static struct latency_stats rx_stats = {0};
static struct latency_stats tx_stats = {0};

static void update_stats(struct latency_stats *stats, unsigned int latency_us)
{
	stats->packet_count++;
	stats->total_latency_us += latency_us;
	
	if (stats->min_latency_us == 0 || latency_us < stats->min_latency_us)
		stats->min_latency_us = latency_us;
		
	if (latency_us > stats->max_latency_us)
		stats->max_latency_us = latency_us;
		
	stats->avg_latency_us = stats->total_latency_us / stats->packet_count;
}

void bpf_trace_netdev_rx(struct sk_buff *skb, const char *dev_name)
{
	struct latency_trace_entry *entry;
	unsigned long flags;
	ktime_t now = ktime_get();
	unsigned int latency_us;
	
	if (!skb || !dev_name)
		return;
		
	/* Calculate latency if timestamp is available */
	if (skb->tstamp.tv64) {
		latency_us = ktime_to_us(ktime_sub(now, skb->tstamp));
		update_stats(&rx_stats, latency_us);
	} else {
		latency_us = 0;
	}
	
	spin_lock_irqsave(&trace_lock, flags);
	
	entry = &trace_buffer[trace_index % MAX_TRACE_ENTRIES];
	entry->timestamp = now;
	entry->latency_us = latency_us;
	entry->packet_size = skb->len;
	entry->event_type = TRACE_RX_END;
	strncpy(entry->interface, dev_name, sizeof(entry->interface) - 1);
	entry->interface[sizeof(entry->interface) - 1] = '\0';
	
	trace_index++;
	
	spin_unlock_irqrestore(&trace_lock, flags);
}
EXPORT_SYMBOL(bpf_trace_netdev_rx);

void bpf_trace_netdev_tx(struct sk_buff *skb, const char *dev_name)
{
	struct latency_trace_entry *entry;
	unsigned long flags;
	ktime_t now = ktime_get();
	
	if (!skb || !dev_name)
		return;
		
	/* Set timestamp for latency measurement */
	skb->tstamp = now;
	
	spin_lock_irqsave(&trace_lock, flags);
	
	entry = &trace_buffer[trace_index % MAX_TRACE_ENTRIES];
	entry->timestamp = now;
	entry->latency_us = 0;  /* Will be calculated on RX */
	entry->packet_size = skb->len;
	entry->event_type = TRACE_TX_START;
	strncpy(entry->interface, dev_name, sizeof(entry->interface) - 1);
	entry->interface[sizeof(entry->interface) - 1] = '\0';
	
	trace_index++;
	
	spin_unlock_irqrestore(&trace_lock, flags);
}
EXPORT_SYMBOL(bpf_trace_netdev_tx);

static int latency_trace_proc_show(struct seq_file *m, void *v)
{
	unsigned int i, start_idx;
	unsigned long flags;
	struct latency_trace_entry entry;
	const char *event_names[] = {"RX_START", "RX_END", "TX_START", "TX_END"};
	
	seq_printf(m, "Network Latency Trace Buffer\n");
	seq_printf(m, "============================\n\n");
	
	seq_printf(m, "RX Statistics:\n");
	seq_printf(m, "  Packets: %lu\n", rx_stats.packet_count);
	seq_printf(m, "  Avg Latency: %u us\n", rx_stats.avg_latency_us);
	seq_printf(m, "  Min Latency: %u us\n", rx_stats.min_latency_us);
	seq_printf(m, "  Max Latency: %u us\n", rx_stats.max_latency_us);
	
	seq_printf(m, "\nTX Statistics:\n");
	seq_printf(m, "  Packets: %lu\n", tx_stats.packet_count);
	seq_printf(m, "  Avg Latency: %u us\n", tx_stats.avg_latency_us);
	seq_printf(m, "  Min Latency: %u us\n", tx_stats.min_latency_us);
	seq_printf(m, "  Max Latency: %u us\n", tx_stats.max_latency_us);
	
	seq_printf(m, "\nRecent Trace Entries:\n");
	seq_printf(m, "Timestamp(us)\tEvent\t\tInterface\tSize\tLatency(us)\n");
	seq_printf(m, "---------------------------------------------------------------\n");
	
	spin_lock_irqsave(&trace_lock, flags);
	
	start_idx = (trace_index > MAX_TRACE_ENTRIES) ? 
	            (trace_index - MAX_TRACE_ENTRIES) : 0;
	
	for (i = 0; i < min(trace_index, (unsigned int)MAX_TRACE_ENTRIES); i++) {
		unsigned int idx = (start_idx + i) % MAX_TRACE_ENTRIES;
		entry = trace_buffer[idx];
		
		seq_printf(m, "%llu\t%-12s\t%-12s\t%u\t%u\n",
			   ktime_to_us(entry.timestamp),
			   event_names[entry.event_type],
			   entry.interface,
			   entry.packet_size,
			   entry.latency_us);
	}
	
	spin_unlock_irqrestore(&trace_lock, flags);
	
	return 0;
}

static int latency_trace_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, latency_trace_proc_show, NULL);
}

static const struct file_operations latency_trace_proc_fops = {
	.open = latency_trace_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct proc_dir_entry *proc_entry;

static int __init bpf_trace_init(void)
{
	proc_entry = proc_create("latency_trace", 0444, NULL, &latency_trace_proc_fops);
	if (!proc_entry) {
		printk(KERN_ERR "Failed to create latency_trace proc entry\n");
		return -ENOMEM;
	}
	
	printk(KERN_INFO "BPF network latency tracing initialized\n");
	return 0;
}

static void __exit bpf_trace_exit(void)
{
	if (proc_entry)
		remove_proc_entry("latency_trace", NULL);
		
	printk(KERN_INFO "BPF network latency tracing unloaded\n");
}

module_init(bpf_trace_init);
module_exit(bpf_trace_exit);

MODULE_DESCRIPTION("Simple BPF-style network latency tracing for 3.4.x");
MODULE_AUTHOR("Padavan-NG Project");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");