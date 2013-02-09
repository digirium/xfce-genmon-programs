ALL = \
	$(HOME)/bin/cpuinfo	\
	$(HOME)/bin/diskinfo	\
	$(HOME)/bin/meminfo	\
	$(HOME)/bin/netinfo	\
	$(HOME)/bin/nvidiainfo	\
	$(HOME)/bin/pacinfo	\
	$(HOME)/bin/ffpcsync

all: $(ALL)

$(HOME)/bin/nvidiainfo: nvidiainfo
	cp nvidiainfo $(HOME)/bin/nvidiainfo
	chmod 755 $(HOME)/bin/nvidiainfo

$(HOME)/bin/pacinfo: pacinfo
	cp pacinfo $(HOME)/bin/pacinfo
	chmod 755 $(HOME)/bin/pacinfo

$(HOME)/bin/ffpcsync: ffpcsync
	cp ffpcsync $(HOME)/bin/ffpcsync
	chmod 755 $(HOME)/bin/ffpcsync

$(HOME)/bin/%: %.c
	$(CC) -o $@ $?
