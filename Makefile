VERSION=$(shell awk '/Version:/ { print $$2 }' chkconfig.spec)
CVSTAG = r$(subst .,-,$(VERSION))

CFLAGS=-g -Wall $(RPM_OPT_FLAGS)
LDFLAGS=-g
MAN=chkconfig.8 ntsysv.8
PROG=chkconfig
BINDIR = /sbin
USRSBINDIR = /usr/sbin
MANDIR = /usr/man
SUBDIRS = po

OBJS=chkconfig.o leveldb.o
NTOBJS=ntsysv.o leveldb.o

all: subdirs $(PROG) ntsysv

subdirs:
	for d in $(SUBDIRS); do \
	(cd $$d; $(MAKE)) \
	|| case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac;\
	done && test -z "$$fail"

chkconfig: $(OBJS)
	$(CC) $(LDFLAGS) -o chkconfig $(OBJS) /usr/lib/libpopt.a

ntsysv: $(NTOBJS)
	$(CC) $(LDFLAGS) -o ntsysv $(NTOBJS) -lnewt -lpopt $(LIBMHACK)

chkconfig.o: chkconfig.c leveldb.h
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c chkconfig.c

ntsysv.o: ntsysv.c leveldb.h
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c ntsysv.c

leveldb.o: leveldb.c leveldb.h

clean:
	rm -f chkconfig ntsysv $(OBJS) $(NTOBJS)
	make -C po clean
	rm -f chkconfig-*.tar.gz *~ *.old

install:
	[ -d $(instroot)/$(BINDIR) ] || mkdir -p $(instroot)/$(BINDIR)
	[ -d $(instroot)/$(USRSBINDIR) ] || mkdir -p $(instroot)/$(USRSBINDIR)
	[ -d $(instroot)/$(MANDIR) ] || mkdir -p $(instroot)/$(MANDIR)
	[ -d $(instroot)/$(MANDIR)/man8 ] || mkdir -p $(instroot)/$(MANDIR)/man8
	[ -d $(instroot)/$(MANDIR)/man5 ] || mkdir -p $(instroot)/$(MANDIR)/man5

	install -s -m 755 $(PROG) $(instroot)/$(BINDIR)/$(PROG)
	install -s -m 755 ntsysv $(instroot)/$(USRSBINDIR)/ntsysv
	for i in $(MAN); do \
		install -m 644 $$i $(instroot)/$(MANDIR)/man`echo $$i | sed "s/.*\.//"`/$$i ; \
	done
	for d in $(SUBDIRS); do \
	(cd $$d; $(MAKE) install) \
	    || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac;\
	done && test -z "$$fail"

archive:
	cvs tag -F $(CVSTAG) .
	@rm -rf /tmp/chkconfig-$(VERSION) /tmp/chkconfig
	@cd /tmp; cvs export -r$(CVSTAG) chkconfig
	@mv /tmp/chkconfig /tmp/chkconfig-$(VERSION)
	@dir=$$PWD; cd /tmp; tar cvIf $$dir/chkconfig-$(VERSION).tar.bz2 chkconfig-$(VERSION)
	@rm -rf /tmp/chkconfig-$(VERSION)
	@echo "The archive is in chkconfig-$(VERSION).tar.bz2"
