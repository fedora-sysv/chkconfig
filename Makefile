VERSION=$(shell awk '/Version:/ { print $$2 }' chkconfig.spec)
RELEASE=$(shell awk '/Release:/ { print $$2 }' chkconfig.spec)
CVSTAG = r$(subst .,-,$(VERSION)-$(RELEASE))
CVSROOT = $(shell cat CVS/Root 2>/dev/null || :)

CFLAGS=-g -Wall $(RPM_OPT_FLAGS) -D_GNU_SOURCE
LDFLAGS=-g
MAN=chkconfig.8 ntsysv.8 alternatives.8
PROG=chkconfig
BINDIR = /sbin
SBINDIR = /usr/sbin
MANDIR = /usr/man
ALTDIR = /var/lib/alternatives
ALTDATADIR = /etc/alternatives
SUBDIRS = po

OBJS=chkconfig.o leveldb.o
NTOBJS=ntsysv.o leveldb.o

all: subdirs $(PROG) ntsysv alternatives

subdirs:
	for d in $(SUBDIRS); do \
	(cd $$d; $(MAKE)) \
	|| case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac;\
	done && test -z "$$fail"

chkconfig: $(OBJS)
	$(CC) $(LDFLAGS) -o chkconfig $(OBJS) -Wl,-Bstatic -lpopt -Wl,-Bdynamic

alternativs: alternatives.o

ntsysv: $(NTOBJS)
	$(CC) $(LDFLAGS) -o ntsysv $(NTOBJS) -lnewt -lpopt $(LIBMHACK)

chkconfig.o: chkconfig.c leveldb.h
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c chkconfig.c

alternatives.o: alternatives.c
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c alternatives.c

ntsysv.o: ntsysv.c leveldb.h
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c ntsysv.c

leveldb.o: leveldb.c leveldb.h

clean:
	rm -f chkconfig ntsysv $(OBJS) $(NTOBJS)
	rm -f alternatives alternatives.o
	make -C po clean
	rm -f chkconfig-*.tar.gz *~ *.old

install:
	[ -d $(DESTDIR)/$(BINDIR) ] || mkdir -p $(DESTDIR)/$(BINDIR)
	[ -d $(DESTDIR)/$(SBINDIR) ] || mkdir -p $(DESTDIR)/$(SBINDIR)
	[ -d $(DESTDIR)/$(MANDIR) ] || mkdir -p $(DESTDIR)/$(MANDIR)
	[ -d $(DESTDIR)/$(MANDIR)/man8 ] || mkdir -p $(DESTDIR)/$(MANDIR)/man8
	[ -d $(DESTDIR)/$(MANDIR)/man5 ] || mkdir -p $(DESTDIR)/$(MANDIR)/man5
	[ -d $(DESTDIR)/$(ALTDIR) ] || mkdir -p -m 755 $(DESTDIR)/$(ALTDIR)
	[ -d $(DESTDIR)/$(ALTDATADIR) ] || mkdir -p -m 755 $(DESTDIR)/$(ALTDATADIR)

	install -m 755 $(PROG) $(DESTDIR)/$(BINDIR)/$(PROG)
	install -m 755 ntsysv $(DESTDIR)/$(SBINDIR)/ntsysv
	install -m 755 alternatives $(DESTDIR)/$(SBINDIR)/alternatives
	ln -s alternatives $(DESTDIR)/$(SBINDIR)/update-alternatives
	
	for i in $(MAN); do \
		install -m 644 $$i $(DESTDIR)/$(MANDIR)/man`echo $$i | sed "s/.*\.//"`/$$i ; \
	done

	ln -s alternatives.8 $(DESTDIR)/$(MANDIR)/man8/update-alternatives.8

	for d in $(SUBDIRS); do \
	(cd $$d; $(MAKE) install) \
	    || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac;\
	done && test -z "$$fail"

archive:
	cvs tag -F $(CVSTAG) .
	@rm -rf /tmp/chkconfig-$(VERSION) /tmp/chkconfig
	@cd /tmp; cvs -d $(CVSROOT) export -r$(CVSTAG) chkconfig
	@mv /tmp/chkconfig /tmp/chkconfig-$(VERSION)
	@dir=$$PWD; cd /tmp; tar cvzf $$dir/chkconfig-$(VERSION).tar.gz chkconfig-$(VERSION)
	@rm -rf /tmp/chkconfig-$(VERSION)
	@echo "The archive is in chkconfig-$(VERSION).tar.gz"
