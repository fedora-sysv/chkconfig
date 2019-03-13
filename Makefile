VERSION=$(shell awk '/Version:/ { print $$2 }' chkconfig.spec)
TAG = $(VERSION)

CFLAGS=-g -Wall $(RPM_OPT_FLAGS) -D_GNU_SOURCE
LDFLAGS+=-g
MAN=chkconfig.8 ntsysv.8
PROG=chkconfig
BINDIR = /sbin
SBINDIR = /usr/sbin
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
	$(CC) $(LDFLAGS) -o chkconfig $(OBJS) -lpopt -lselinux -lsepol

ntsysv: $(NTOBJS)
	$(CC) $(LDFLAGS) -o ntsysv $(NTOBJS) -lnewt -lpopt $(LIBMHACK) -lselinux -lsepol

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
	[ -d $(DESTDIR)/$(BINDIR) ] || mkdir -p $(DESTDIR)/$(BINDIR)
	[ -d $(DESTDIR)/$(SBINDIR) ] || mkdir -p $(DESTDIR)/$(SBINDIR)
	[ -d $(DESTDIR)/$(MANDIR) ] || mkdir -p $(DESTDIR)/$(MANDIR)
	[ -d $(DESTDIR)/$(MANDIR)/man8 ] || mkdir -p $(DESTDIR)/$(MANDIR)/man8
	[ -d $(DESTDIR)/usr/lib/systemd ] || mkdir -p -m 755 $(DESTDIR)/usr/lib/systemd

	install -m 755 $(PROG) $(DESTDIR)/$(BINDIR)/$(PROG)
	ln -s ../../../$(BINDIR)/$(PROG) $(DESTDIR)/usr/lib/systemd/systemd-sysv-install

	install -m 755 ntsysv $(DESTDIR)/$(SBINDIR)/ntsysv

	for i in $(MAN); do \
		install -m 644 $$i $(DESTDIR)/$(MANDIR)/man`echo $$i | sed "s/.*\.//"`/$$i ; \
	done

	for d in $(SUBDIRS); do \
	(cd $$d; $(MAKE) install) \
	    || case "$(MFLAGS)" in *k*) fail=yes;; *) exit 1;; esac;\
	done && test -z "$$fail"

tag:
	@git tag -a -m "Tag as $(TAG)" -f $(TAG)
	@echo "Tagged as $(TAG)"
