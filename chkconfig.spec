Summary: Updates and queries runlevel information for system services
Name: chkconfig
%define version 1.0
Version: %{version}
Release: 1
Copyright: GPL
Group: Utilities/System
Source: ftp://ftp.redhat.com/pub/redhat/code/chkconfig/chkconfig-%{version}.tar.gz
BuildRoot: /var/tmp/chkconfig.root

%description
chkconfig provides a simple command-line  tool  for  maintaining  the
/etc/rc.d  directory  hierarchy by relieving system administrators of
directly manipulating the  numerous symbolic links in that directory.

%package -n ntsysv
Summary: Full-screen interface for configurating runlevel information
Group: Utilities/System

%description -n ntsysv
ntsysv provides a full-screen tool for updating the /etc/rc.d directory
hierarchy, which controls the starting and stopping of system services.

%prep
%setup -q

%build

%ifarch sparc
LIBMHACK=-lm
%endif

make RPM_OPT_FLAGS="$RPM_OPT_FLAGS" LIBMHACK=$LIBMHACK

%install
rm -rf $RPM_BUILD_ROOT
make instroot=$RPM_BUILD_ROOT install

mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
for n in 0 1 2 3 4 5 6; do
    mkdir -p $RPM_BUILD_ROOT/etc/rc.d/rc${n}.d
done

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/sbin/chkconfig
/usr/man/man8/chkconfig.8
%dir /etc/rc.d
%dir /etc/rc.d/*
/usr/share/locale/*/LC_MESSAGES/chkconfig.mo

%files -n ntsysv
%defattr(-,root,root)
/usr/sbin/ntsysv
/usr/man/man8/ntsysv.8

%changelog
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
