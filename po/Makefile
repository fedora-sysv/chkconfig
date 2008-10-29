INSTALL= /usr/bin/install -c
INSTALL_PROGRAM= ${INSTALL}
INSTALL_DATA= ${INSTALL} -m 644
INSTALLNLSDIR=$(DESTDIR)/usr/share/locale

MSGMERGE = msgmerge

NLSPACKAGE = chkconfig

POFILES = $(shell ls *.po)
CATALOGS = $(shell ls *.po | sed 's/\.po/\.mo/')

POTFILES  = ../chkconfig.c ../ntsysv.c ../leveldb.c ../alternatives.c

all: $(NLSPACKAGE).pot $(CATALOGS)

$(NLSPACKAGE).pot: $(POTFILES)
	xgettext --default-domain=$(NLSPACKAGE) \
	       	  --add-comments --keyword=_ --keyword=N_ $(POTFILES)
	if cmp -s $(NLSPACKAGE).po $(NLSPACKAGE).pot; then \
	    rm -f $(NLSPACKAGE).po; \
	else \
	    mv $(NLSPACKAGE).po $(NLSPACKAGE).pot; \
	fi


update-po: Makefile
	$(MAKE) $(NLSPACKAGE).pot
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
		lang=`basename $$cat .mo`; \
		if msgmerge $$lang.po $(NLSPACKAGE).pot > $$lang.pox ; then \
			echo "msgmerge of $$lang succeeded" ; \
			mv $$lang.pox $$lang.po ; \
		else \
			echo "msgmerge of $$lang failed" ; \
			rm -f $$lang.pox ; \
		fi \
	done

refresh-po: Makefile
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
		lang=`basename $$cat .mo`; \
		if msgmerge $$lang.po $(NLSPACKAGE).pot > $$lang.pox ; then \
			echo "msgmerge of $$lang succeeded" ; \
			mv $$lang.pox $$lang.po ; \
		else \
			echo "msgmerge of $$lang failed" ; \
			rm -f $$lang.pox ; \
		fi \
	done

clean:
	rm -f *mo

distclean: clean
	rm -f .depend Makefile

depend:

install:
	mkdir -p $(INSTALLNLSDIR)
	for n in $(CATALOGS); do \
	    l=`basename $$n .mo`; \
	    $(INSTALL) -m 755 -d $(INSTALLNLSDIR)/$$l; \
	    $(INSTALL) -m 755 -d $(INSTALLNLSDIR)/$$l/LC_MESSAGES; \
	    if [ -f $$n ]; then \
	        $(INSTALL) -m 644 $$n $(INSTALLNLSDIR)/$$l/LC_MESSAGES/$(NLSPACKAGE).mo; \
	    fi \
	done

%.mo: %.po
	msgfmt -o $@ $<

report: 
	@for lang in $(POFILES); do \
                echo -n "$$lang: "; \
                msgfmt -v --statistics -o /dev/null $$lang; \
        done

