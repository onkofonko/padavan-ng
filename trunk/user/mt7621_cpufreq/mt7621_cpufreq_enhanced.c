/*
 * Enhanced MT7621 CPU Frequency Controller with Latency Optimization
 * 
 * Based on original mt7621_cpufreq.c but with enhanced features for
 * low-latency networking applications.
 */

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define RALINK_SYSCTL_BASE		0x1E000000
#define RALINK_MEMCTRL_BASE		0x1E005000

#define CPU_MIN_FREQ 600
#define CPU_MAX_FREQ 1400
#define CPU_LATENCY_FREQ 1200  /* Optimized frequency for low latency */

/* Performance profiles */
enum cpu_profile {
	PROFILE_POWERSAVE = 0,
	PROFILE_BALANCED = 1,
	PROFILE_PERFORMANCE = 2,
	PROFILE_LATENCY = 3,  /* New latency-optimized profile */
};

static const char *profile_names[] = {
	"powersave",
	"balanced", 
	"performance",
	"latency"
};

static const int profile_freqs[] = {
	600,   /* powersave */
	900,   /* balanced */
	1400,  /* performance */
	1200   /* latency - optimal for network responsiveness */
};

struct cpu_state {
	unsigned int current_freq;
	enum cpu_profile current_profile;
	int auto_scaling;
	time_t last_update;
};

static struct cpu_state cpu_state = {0};

int ralink_asic_rev_id;

static void *map_memory(unsigned long base, size_t size)
{
	int mem;
	void *ptr;
	
	if ((mem = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		handle_error("Cannot open /dev/mem");
	}
	
	ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem, base);
	if (ptr == MAP_FAILED) {
		close(mem);
		handle_error("mmap failed");
	}
	
	close(mem);
	return ptr;
}

static int set_cpu_frequency(unsigned int freq_mhz)
{
	void *ptr, *ptr2;
	unsigned int reg, surfboard_sysclk;
	char clk_sel, clk_sel2;
	int cpu_fdiv = 0, cpu_ffrac = 0, fbdiv = 0;
	int xtal = 40;
	
	if (freq_mhz < CPU_MIN_FREQ || freq_mhz > CPU_MAX_FREQ) {
		fprintf(stderr, "Frequency %d MHz out of range [%d-%d]\n", 
			freq_mhz, CPU_MIN_FREQ, CPU_MAX_FREQ);
		return -1;
	}
	
	ptr = map_memory(RALINK_SYSCTL_BASE, 0x100);
	ptr2 = map_memory(RALINK_MEMCTRL_BASE, 0x1000);
	
	/* Read current configuration */
	reg = (*((volatile u32 *)(ptr + 0x10)));
	clk_sel = (reg >> 6) & 0x7;
	clk_sel2 = (reg >> 4) & 0x3;
	
	/* Calculate dividers for target frequency */
	if (freq_mhz <= 600) {
		cpu_fdiv = 1;
		cpu_ffrac = 0;
		fbdiv = freq_mhz / 25 - 1;
	} else if (freq_mhz <= 900) {
		cpu_fdiv = 1;
		cpu_ffrac = 1;
		fbdiv = freq_mhz / 25 - 1;
	} else {
		cpu_fdiv = 0;
		cpu_ffrac = 1;
		fbdiv = freq_mhz / 25 - 1;
	}
	
	/* Apply frequency settings with optimizations for latency profile */
	if (cpu_state.current_profile == PROFILE_LATENCY) {
		/* Enable additional optimizations for latency */
		reg |= (1 << 31);  /* Enable turbo mode if available */
		reg &= ~(1 << 30); /* Disable power saving features */
	}
	
	/* Update frequency */
	reg &= ~(0x1F << 11);
	reg |= (fbdiv << 11);
	reg &= ~(0x3 << 9);
	reg |= (cpu_fdiv << 9);
	reg &= ~(0x1 << 8);
	reg |= (cpu_ffrac << 8);
	
	(*((volatile u32 *)(ptr + 0x10))) = reg;
	
	/* Wait for stabilization */
	usleep(50000);  /* 50ms */
	
	munmap(ptr, 0x100);
	munmap(ptr2, 0x1000);
	
	cpu_state.current_freq = freq_mhz;
	cpu_state.last_update = time(NULL);
	
	printf("CPU frequency set to %d MHz\n", freq_mhz);
	return 0;
}

static int set_cpu_profile(enum cpu_profile profile)
{
	if (profile >= sizeof(profile_freqs) / sizeof(profile_freqs[0])) {
		fprintf(stderr, "Invalid profile %d\n", profile);
		return -1;
	}
	
	cpu_state.current_profile = profile;
	
	printf("Setting CPU profile to %s (%d MHz)\n", 
		profile_names[profile], profile_freqs[profile]);
		
	return set_cpu_frequency(profile_freqs[profile]);
}

static void print_usage(const char *progname)
{
	printf("Usage: %s [options]\n", progname);
	printf("Options:\n");
	printf("  <freq>           Set CPU frequency in MHz (%d-%d)\n", CPU_MIN_FREQ, CPU_MAX_FREQ);
	printf("  -p <profile>     Set performance profile:\n");
	printf("                   powersave, balanced, performance, latency\n");
	printf("  -s               Show current status\n");
	printf("  -h               Show this help\n");
	printf("\nLatency profile is optimized for network responsiveness\n");
}

static void print_status(void)
{
	printf("Current CPU Status:\n");
	printf("  Frequency: %d MHz\n", cpu_state.current_freq);
	printf("  Profile: %s\n", profile_names[cpu_state.current_profile]);
	printf("  Last update: %s", ctime(&cpu_state.last_update));
}

int main(int argc, char *argv[])
{
	int opt;
	unsigned int freq = 0;
	enum cpu_profile profile = -1;
	int show_status = 0;
	
	if (argc == 1) {
		print_usage(argv[0]);
		return 1;
	}
	
	/* Parse command line arguments */
	while ((opt = getopt(argc, argv, "p:sh")) != -1) {
		switch (opt) {
		case 'p':
			if (strcmp(optarg, "powersave") == 0)
				profile = PROFILE_POWERSAVE;
			else if (strcmp(optarg, "balanced") == 0)
				profile = PROFILE_BALANCED;
			else if (strcmp(optarg, "performance") == 0)
				profile = PROFILE_PERFORMANCE;
			else if (strcmp(optarg, "latency") == 0)
				profile = PROFILE_LATENCY;
			else {
				fprintf(stderr, "Unknown profile: %s\n", optarg);
				return 1;
			}
			break;
		case 's':
			show_status = 1;
			break;
		case 'h':
		default:
			print_usage(argv[0]);
			return 1;
		}
	}
	
	/* Handle frequency argument */
	if (optind < argc) {
		freq = atoi(argv[optind]);
	}
	
	/* Initialize CPU state */
	cpu_state.current_freq = 880;  /* Default */
	cpu_state.current_profile = PROFILE_BALANCED;
	cpu_state.auto_scaling = 0;
	cpu_state.last_update = time(NULL);
	
	if (show_status) {
		print_status();
		return 0;
	}
	
	if (profile != -1) {
		return set_cpu_profile(profile);
	}
	
	if (freq > 0) {
		return set_cpu_frequency(freq);
	}
	
	print_usage(argv[0]);
	return 1;
}