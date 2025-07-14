PROG = mt7621_cpufreq
PROG_ENHANCED = mt7621_cpufreq_enhanced

all: $(PROG) $(PROG_ENHANCED)

$(PROG): mt7621_cpufreq.c
	$(CC) -o $@ $^ $(CFLAGS) $(DEFINES) $(LIBS)

$(PROG_ENHANCED): mt7621_cpufreq_enhanced.c
	$(CC) -o $@ $^ $(CFLAGS) $(DEFINES) $(LIBS)

strip: $(PROG) $(PROG_ENHANCED)
	$(STRIP) -s $(PROG)
	$(STRIP) -s $(PROG_ENHANCED)

clean:
	rm -f *.o $(PROG) $(PROG_ENHANCED)

romfs:
	$(ROMFSINST) /sbin/$(PROG)
	$(ROMFSINST) /sbin/$(PROG_ENHANCED)
