/*
 * Low Latency IRQ Affinity Management for Ralink/MTK SoCs
 * 
 * This module provides automatic IRQ affinity optimization for network
 * interfaces to minimize latency in multi-core MIPS systems.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/cpumask.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#define LATENCY_IRQ_NAME "latency_irq"
#define PROC_ENTRY_NAME "latency_irq_affinity"

static struct proc_dir_entry *proc_entry;
static struct timer_list balance_timer;
static struct workqueue_struct *irq_wq;

/* Configuration parameters */
static int enable_affinity = 1;
static int balance_interval = 1000; /* milliseconds */
static int network_irq_cpu = 1; /* dedicate CPU 1 for network IRQs */

module_param(enable_affinity, int, 0644);
MODULE_PARM_DESC(enable_affinity, "Enable IRQ affinity optimization (default: 1)");

module_param(balance_interval, int, 0644);
MODULE_PARM_DESC(balance_interval, "IRQ balance interval in ms (default: 1000)");

module_param(network_irq_cpu, int, 0644);
MODULE_PARM_DESC(network_irq_cpu, "CPU for network IRQs (default: 1)");

/* IRQ information structure */
struct irq_info {
	unsigned int irq;
	const char *name;
	unsigned long count;
	int assigned_cpu;
};

static struct irq_info tracked_irqs[16];
static int tracked_irq_count = 0;

static void optimize_irq_affinity(void)
{
	int i;
	cpumask_t network_mask, other_mask;
	
	if (!enable_affinity)
		return;
		
	/* Set up CPU masks */
	cpumask_clear(&network_mask);
	cpumask_clear(&other_mask);
	
	/* Assign network CPU */
	if (network_irq_cpu < num_online_cpus())
		cpumask_set_cpu(network_irq_cpu, &network_mask);
	else
		cpumask_set_cpu(0, &network_mask);
		
	/* Other CPUs for general IRQs */
	for (i = 0; i < num_online_cpus(); i++) {
		if (i != network_irq_cpu)
			cpumask_set_cpu(i, &other_mask);
	}
	
	/* Apply affinity settings */
	for (i = 0; i < tracked_irq_count; i++) {
		struct irq_desc *desc = irq_to_desc(tracked_irqs[i].irq);
		if (!desc)
			continue;
			
		/* Network-related IRQs get dedicated CPU */
		if (strstr(tracked_irqs[i].name, "eth") ||
		    strstr(tracked_irqs[i].name, "raeth") ||
		    strstr(tracked_irqs[i].name, "fe")) {
			irq_set_affinity(tracked_irqs[i].irq, &network_mask);
			tracked_irqs[i].assigned_cpu = network_irq_cpu;
		} else {
			/* Other IRQs distributed across remaining CPUs */
			irq_set_affinity(tracked_irqs[i].irq, &other_mask);
			tracked_irqs[i].assigned_cpu = cpumask_first(&other_mask);
		}
	}
}

static void irq_balance_work(struct work_struct *work)
{
	optimize_irq_affinity();
}

static DECLARE_WORK(irq_balance_task, irq_balance_work);

static void balance_timer_func(unsigned long data)
{
	if (enable_affinity) {
		queue_work(irq_wq, &irq_balance_task);
		mod_timer(&balance_timer, jiffies + msecs_to_jiffies(balance_interval));
	}
}

static void scan_and_track_irqs(void)
{
	int irq;
	struct irq_desc *desc;
	
	tracked_irq_count = 0;
	
	for_each_irq_desc(irq, desc) {
		if (desc && desc->action && desc->action->name) {
			if (tracked_irq_count < ARRAY_SIZE(tracked_irqs)) {
				tracked_irqs[tracked_irq_count].irq = irq;
				tracked_irqs[tracked_irq_count].name = desc->action->name;
				tracked_irqs[tracked_irq_count].count = kstat_irqs(irq);
				tracked_irq_count++;
			}
		}
	}
}

static int latency_irq_proc_show(struct seq_file *m, void *v)
{
	int i;
	
	seq_printf(m, "Low Latency IRQ Affinity Manager\n");
	seq_printf(m, "Enabled: %d\n", enable_affinity);
	seq_printf(m, "Balance Interval: %d ms\n", balance_interval);
	seq_printf(m, "Network IRQ CPU: %d\n", network_irq_cpu);
	seq_printf(m, "Online CPUs: %d\n", num_online_cpus());
	seq_printf(m, "\nTracked IRQs:\n");
	seq_printf(m, "IRQ\tName\t\tCount\t\tCPU\n");
	
	for (i = 0; i < tracked_irq_count; i++) {
		seq_printf(m, "%d\t%-16s\t%lu\t\t%d\n",
			   tracked_irqs[i].irq,
			   tracked_irqs[i].name,
			   tracked_irqs[i].count,
			   tracked_irqs[i].assigned_cpu);
	}
	
	return 0;
}

static int latency_irq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, latency_irq_proc_show, NULL);
}

static const struct file_operations latency_irq_proc_fops = {
	.open = latency_irq_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init latency_irq_init(void)
{
	printk(KERN_INFO "Low Latency IRQ Affinity Manager initialized\n");
	
	/* Create workqueue */
	irq_wq = create_singlethread_workqueue("latency_irq_wq");
	if (!irq_wq) {
		printk(KERN_ERR "Failed to create workqueue\n");
		return -ENOMEM;
	}
	
	/* Create proc entry */
	proc_entry = proc_create(PROC_ENTRY_NAME, 0644, NULL, &latency_irq_proc_fops);
	if (!proc_entry) {
		destroy_workqueue(irq_wq);
		printk(KERN_ERR "Failed to create proc entry\n");
		return -ENOMEM;
	}
	
	/* Scan existing IRQs */
	scan_and_track_irqs();
	
	/* Setup timer for periodic rebalancing */
	setup_timer(&balance_timer, balance_timer_func, 0);
	mod_timer(&balance_timer, jiffies + msecs_to_jiffies(balance_interval));
	
	/* Initial optimization */
	optimize_irq_affinity();
	
	return 0;
}

static void __exit latency_irq_exit(void)
{
	del_timer_sync(&balance_timer);
	
	if (irq_wq) {
		flush_workqueue(irq_wq);
		destroy_workqueue(irq_wq);
	}
	
	if (proc_entry)
		remove_proc_entry(PROC_ENTRY_NAME, NULL);
		
	printk(KERN_INFO "Low Latency IRQ Affinity Manager unloaded\n");
}

module_init(latency_irq_init);
module_exit(latency_irq_exit);

MODULE_DESCRIPTION("Low Latency IRQ Affinity Manager for Ralink/MTK SoCs");
MODULE_AUTHOR("Padavan-NG Project");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");