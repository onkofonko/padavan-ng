PROG = mt7621_cpufreq

$(PROG): mt7621_cpufreq.c
	$(CC) -o $@ $^ $(CFLAGS) $(DEFINES) $(LIBS)

strip: $(PROG)
	$(STRIP) -s $(PROG)

clean:
	rm -f *.o $(PROG)

romfs:
	$(ROMFSINST) /sbin/$(PROG)
