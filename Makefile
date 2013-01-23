ALL = \
	$(HOME)/bin/cpuinfo	\
	$(HOME)/bin/diskinfo	\
	$(HOME)/bin/meminfo	\
	$(HOME)/bin/netinfo

all: $(ALL)

$(HOME)/bin/%: %.c
	$(CC) -o $@ $?
