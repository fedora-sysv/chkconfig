VERSION=0.3
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
	[ -d $(instroot)/$(MANDIR)/man5 ] || mkdir -p $(instroot)/$(MANDIR)/man5

	install -s -m 755 $(PROG) $(instroot)/$(BINDIR)
	for i in $(MAN); do \
		install -m 644 $$i $(instroot)/$(MANDIR)/man`echo $$i | sed "s/.*\.//"`/$$i ; \
	done

archive:
	cvs tag -F $(CVSTAG) .
	@rm -rf /tmp/chkconfig-$(VERSION)
	@cd /tmp; cvs export -r$(CVSTAG) -d /tmp/chkconfig-$(VERSION) chkconfig
	@dir=$$PWD; cd /tmp; tar cvzf $$dir/chkconfig-$(VERSION).tar.gz chkconfig-$(VERSION)
	@rm -rf /tmp/chkconfig-$(VERSION)
	@echo "The archive is in chkconfig-$(VERSION)"
