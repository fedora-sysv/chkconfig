VERSION=0.1
CVSTAG = r$(subst .,-,$(VERSION))

CFLAGS=-g -Wall $(RPM_OPT_FLAGS)
LDFLAGS=-g
LIBS=-lpopt
MAN=chkconfig.8
PROG=chkconfig
BINDIR = /sbin
MANDIR = /usr/man

OBJS=chkconfig.o leveldb.o

all: $(PROG)

chkconfig: $(OBJS)
	$(CC) $(LDFLAGS) -o chkconfig $(OBJS) $(LIBS)

chkconfig.o: chkconfig.c leveldb.h
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c chkconfig.c

leveldb.o: leveldb.c leveldb.h

clean:
	rm -f chkconfig $(OBJS)

install:
	[ -d $(instroot)/$(BINDIR) ] || mkdir -p $(instroot)/$(BINDIR)
	[ -d $(instroot)/$(MANDIR) ] || mkdir -p $(instroot)/$(MANDIR)
	[ -d $(instroot)/$(MANDIR)/man8 ] || mkdir -p $(instroot)/$(MANDIR)/man8

	install -s -m 755 $(PROG) $(instroot)/$(BINDIR)
	install -m 644 $(MAN) $(instroot)/$(MANDIR)/man`echo $(MAN) | sed "s/.*\.//"`/$(MAN)

archive:
	cvs tag -F $(CVSTAG) .
	@rm -rf /tmp/chkconfig-$(VERSION)
	@cd /tmp; cvs export -r$(CVSTAG) -d /tmp/chkconfig-$(VERSION) chkconfig
	tar cvzf chkconfig-$(VERSION).tar.gz /tmp/chkconfig-$(VERSION)
	rm -f /tmp/chkconfig-$(VERSION)
	echo "The archive is in chkconfig-$(VERSION)"
