Summary: A system tool for maintaining the /etc/rc*.d hierarchy.
Name: chkconfig
Version: 1.3.0
Release: 1
License: GPL
Group: System Environment/Base
Source: ftp://ftp.redhat.com/pub/redhat/code/chkconfig/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildPrereq: newt newt-devel
Conflicts: initscripts <= 5.30-1

%description
Chkconfig is a basic system utility.  It updates and queries runlevel
information for system services.  Chkconfig manipulates the numerous
symbolic links in /etc/rc.d, to relieve system administrators of some 
of the drudgery of manually editing the symbolic links.

%package -n ntsysv
Summary: A tool to set the stop/start of system services in a runlevel.
Group: System Environment/Base
Requires: chkconfig = %{version}

%description -n ntsysv
Ntsysv provides a simple interface for setting which system services
are started or stopped in various runlevels (instead of directly
manipulating the numerous symbolic links in /etc/rc.d). Unless you
specify a runlevel or runlevels on the command line (see the man
page), ntsysv configures the current runlevel (5 if you're using X).

%prep
%setup -q

%build

%ifarch sparc
LIBMHACK=-lm
%endif

make RPM_OPT_FLAGS="$RPM_OPT_FLAGS" LIBMHACK=$LIBMHACK

%install
rm -rf $RPM_BUILD_ROOT
make instroot=$RPM_BUILD_ROOT MANDIR=%{_mandir} install

mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
ln -s rc.d/init.d $RPM_BUILD_ROOT/etc/init.d
for n in 0 1 2 3 4 5 6; do
    mkdir -p $RPM_BUILD_ROOT/etc/rc.d/rc${n}.d
    ln -s rc.d/rc${n}.d $RPM_BUILD_ROOT/etc/rc${n}.d
done

%find_lang %{name}

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}.lang
%defattr(-,root,root)
%dir /etc/alternatives
/sbin/chkconfig
/usr/sbin/update-alternatives
/usr/sbin/alternatives
/etc/init.d
/etc/rc.d/init.d
/etc/rc[0-6].d
/etc/rc.d/rc[0-6].d
%dir /var/lib/alternatives
%{_mandir}/*/chkconfig*
%{_mandir}/*/alternatives*

%files -n ntsysv
%defattr(-,root,root)
/usr/sbin/ntsysv
%{_mandir}/*/ntsysv.8*

%define date    %(echo `LC_ALL="C" date +"%a %b %d %Y"`)

%changelog
* Fri Mar  8 2002 Bill Nottingham <notting@redhat.com>
- alternatives: handle initscripts too; --initscript command-line option
- chkconfig/ntsysv (and serviceconf, indirectly): services with
   *no* links in /etc/rc*.d are no longer displayed with --list, or
   available for configuration except via chkconfig command-line options

* Tue Mar  5 2002 Bill Nottingham <notting@redhat.com>
- alternatives: handle things with different numbers of slave links

* Mon Mar  4 2002 Bill Nottingham <notting@redhat.com>
- minor alternatives tweaks: don't install the same thing multiple times

* Wed Jan 30 2002 Bill Nottingham <notting@redhat.com>
- actually, put the alternatives stuff back in /usr/sbin
- ship /etc/alternatives dir
- random alternatives fixes

* Sun Jan 27 2002 Erik Troan <ewt@redhat.com>
- reimplemented update-alternatives as just alternatives

* Thu Jan 25 2002 Bill Nottingham <notting@redhat.com>
- add in update-alternatives stuff (perl ATM)

* Mon Aug 27 2001 Trond Eivind Glomsrød <teg@redhat.com>
- Update translations

* Tue Jun 12 2001 Bill Nottingham <notting@redhat.com>
- don't segfault on files that are exactly the length of a page size
  (#44199, <kmori@redhat.com>)

* Sun Mar  4 2001 Bill Nottingham <notting@redhat.com>
- don't show xinetd services in ntsysv if xinetd doesn't appear to be
  installed (#30565)

* Wed Feb 14 2001 Preston Brown <pbrown@redhat.com>
- final translation update.

* Tue Feb 13 2001 Preston Brown <pbrown@redhat.com>
- warn in ntsysv if not running as root.

* Fri Feb  2 2001 Preston Brown <pbrown@redhat.com>
- use lang finder script

* Fri Feb  2 2001 Bill Nottingham <notting@redhat.com>
- finally fix the bug Nalin keeps complaining about :)

* Wed Jan 24 2001 Preston Brown <pbrown@redhat.com>
- final i18n update before Beta.

* Wed Oct 18 2000 Bill Nottingham <notting@redhat.com>
- ignore .rpmnew files (#18915)
- fix typo in error message (#17575)

* Wed Aug 30 2000 Nalin Dahyabhai <nalin@redhat.com>
- make xinetd config files mode 0644, not 644

* Thu Aug 24 2000 Erik Troan <ewt@redhat.com>
- updated it and es translations

* Sun Aug 20 2000 Bill Nottingham <notting@redhat.com>
- get man pages in proper packages

* Sun Aug 20 2000 Matt Wilson <msw@redhat.com>
- new translations

* Tue Aug 16 2000 Nalin Dahyabhai <nalin@redhat.com>
- don't worry about extra whitespace on chkconfig: lines (#16150)

* Wed Aug 10 2000 Trond Eivind Glomsrød <teg@redhat.com>
- i18n merge

* Wed Jul 26 2000 Matt Wilson <msw@redhat.com>
- new translations for de fr it es

* Tue Jul 25 2000 Bill Nottingham <notting@redhat.com>
- change prereqs

* Sun Jul 23 2000 Bill Nottingham <notting@redhat.com>
- fix ntsysv's handling of xinetd/init files with the same name

* Fri Jul 21 2000 Bill Nottingham <notting@redhat.com>
- fix segv when reading malformed files

* Wed Jul 19 2000 Bill Nottingham <notting@redhat.com>
- put links, rc[0-6].d dirs back, those are necessary

* Tue Jul 18 2000 Bill Nottingham <notting@redhat.com>
- add quick hack support for reading descriptions from xinetd files

* Mon Jul 17 2000 Bernhard Rosenkraenzer <bero@redhat.com>
- don't own the /etc/rc[0-6].d symlinks; they're owned by initscripts

* Sat Jul 15 2000 Matt Wilson <msw@redhat.com>
- move back to old file layout

* Thu Jul 13 2000 Preston Brown <pbrown@redhat.com>
- bump copyright date

* Tue Jul 11 2000 Bill Nottingham <notting@redhat.com>
- no %pre today. Maybe tomorrow.

* Thu Jul  6 2000 Bill Nottingham <notting@redhat.com>
- put initscripts %pre here too

* Mon Jul  3 2000 Bill Nottingham <notting@redhat.com>
- oops, if we don't prereq initscripts, we *need* to own /etc/rc[0-6].d

* Sun Jul  2 2000 Bill Nottingham <notting@redhat.com>
- add xinetd support

* Tue Jun 27 2000 Matt Wilson <msw@redhat.com>
- changed Prereq: initscripts >= 5.18 to Conflicts: initscripts < 5.18
- fixed sumary and description where a global string replace nuked them

* Mon Jun 26 2000 Matt Wilson <msw@redhat.com>
- what Bill said, but actually build this version

* Thu Jun 15 2000 Bill Nottingham <notting@redhat.com>
- don't own /etc/rc.*

* Fri Feb 11 2000 Bill Nottingham <notting@redhat.com>
- typo in man page

* Wed Feb 02 2000 Cristian Gafton <gafton@redhat.com>
- fix description

* Wed Jan 12 2000 Bill Nottingham <notting@redhat.com>
- link chkconfig statically against popt

* Mon Oct 18 1999 Bill Nottingham <notting@redhat.com>
- fix querying alternate levels

* Mon Aug 23 1999 Jeff Johnson <jbj@redhat.com>
- don't use strchr to skip unwanted files, look at extension instead (#4166).

* Thu Aug  5 1999 Bill Nottingham <notting@redhat.com>
- fix --help, --verson

* Mon Aug  2 1999 Matt Wilson <msw@redhat.com>
- rebuilt ntsysv against newt 0.50

* Mon Aug  2 1999 Jeff Johnson <jbj@redhat.com>
- fix i18n problem in usage message (#4233).
- add --help and --version.

* Mon Apr 19 1999 Cristian Gafton <gafton@redhat.com>
- release for Red Hat 6.0

* Thu Apr  8 1999 Matt Wilson <msw@redhat.com>
- added support for a "hide: true" tag in initscripts that will make
  services not appear in ntsysv when run with the "--hide" flag

* Thu Apr  1 1999 Matt Wilson <msw@redhat.com>
- added --hide flag for ntsysv that allows you to hide a service from the
  user.

* Mon Mar 22 1999 Bill Nottingham <notting@redhat.com>
- fix glob, once and for all. Really. We mean it.

* Thu Mar 18 1999 Bill Nottingham <notting@redhat.com>
- revert fix for services@levels, it's broken
- change default to only edit the current runlevel

* Mon Mar 15 1999 Bill Nottingham <notting@redhat.com>
- don't remove scripts that don't support chkconfig

* Tue Mar 09 1999 Erik Troan <ewt@redhat.com>
- made glob a bit more specific so xinetd and inetd don't cause improper matches

* Thu Feb 18 1999 Matt Wilson <msw@redhat.com>
- removed debugging output when starting ntsysv

* Thu Feb 18 1999 Preston Brown <pbrown@redhat.com>
- fixed globbing error
- fixed ntsysv running services not at their specified levels.

* Tue Feb 16 1999 Matt Wilson <msw@redhat.com>
- print the value of errno on glob failures.

* Sun Jan 10 1999 Matt Wilson <msw@redhat.com>
- rebuilt for newt 0.40 (ntsysv)

* Tue Dec 15 1998 Jeff Johnson <jbj@redhat.com>
- add ru.po.

* Thu Oct 22 1998 Bill Nottingham <notting@redhat.com>
- build for Raw Hide (slang-1.2.2)

* Wed Oct 14 1998 Cristian Gafton <gafton@redhat.com>
- translation updates

* Thu Oct 08 1998 Cristian Gafton <gafton@redhat.com>
- updated czech translation (and use cs instead of cz)

* Tue Sep 22 1998 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
- added pt_BR translations
- added more translatable strings
- support for i18n init.d scripts description

* Sun Aug 02 1998 Erik Troan <ewt@redhat.com>
- built against newt 0.30
- split ntsysv into a separate package

* Thu May 07 1998 Erik Troan <ewt@redhat.com>
- added numerous translations

* Mon Mar 23 1998 Erik Troan <ewt@redhat.com>
- added i18n support

* Sun Mar 22 1998 Erik Troan <ewt@redhat.com>
- added --back
