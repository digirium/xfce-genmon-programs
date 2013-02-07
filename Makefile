ALL = \
	$(HOME)/bin/cpuinfo	\
	$(HOME)/bin/diskinfo	\
	$(HOME)/bin/meminfo	\
	$(HOME)/bin/netinfo

all: $(ALL)
	cp nvidiainfo $(HOME)/bin/nvidiainfo
	chmod 755 $(HOME)/bin/nvidiainfo
	cp pacinfo $(HOME)/bin/pacinfo
	chmod 755 $(HOME)/bin/pacinfo
	cp ffpcsync $(HOME)/bin/ffpcsync
	chmod 755 $(HOME)/bin/ffpcsync

$(HOME)/bin/%: %.c
	$(CC) -o $@ $?
