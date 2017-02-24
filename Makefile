VERSION=$(shell awk '/Version:/ { print $$2 }' chkconfig.spec)
TAG = $(VERSION)

CFLAGS=-g -Wall $(RPM_OPT_FLAGS) -D_GNU_SOURCE
LDFLAGS+=-g
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
	$(CC) $(LDFLAGS) -lselinux -lsepol -o chkconfig $(OBJS) -lpopt

alternatives: alternatives.o

ntsysv: $(NTOBJS)
	$(CC) $(LDFLAGS) -lselinux -lsepol -o ntsysv $(NTOBJS) -lnewt -lpopt $(LIBMHACK)

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
	[ -d $(DESTDIR)/usr/lib/systemd ] || mkdir -p -m 755 $(DESTDIR)/usr/lib/systemd

	install -m 755 $(PROG) $(DESTDIR)/$(BINDIR)/$(PROG)
	ln -s ../../../$(BINDIR)/$(PROG) $(DESTDIR)/usr/lib/systemd/systemd-sysv-install

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

tag:
	@git tag -a -m "Tag as $(TAG)" -f $(TAG)
	@echo "Tagged as $(TAG)"

check: alternatives
	./test-alternatives.sh
