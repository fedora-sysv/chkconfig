Summary: Updates and queries runlevel information for system services
Name: chkconfig
%define version 0.9.4
Version: %{version}
Release: 1
Copyright: GPL
Group: Utilities/System
Source: ftp://ftp.redhat.com/pub/redhat/code/chkconfig/chkconfig-%{version}.tar.gz
BuildRoot: /var/tmp/chkconfig.root

%package -n ntsysv
Summary: Full-screen interface for configurating runlevel information
Group: Utilities/System

%changelog

* Sun Aug 02 1998 Erik Troan <ewt@redhat.com>

- built against newt 0.30
- split ntsysv into a separate package

* Thu May 07 1998 Erik Troan <ewt@redhat.com>

- added numerous translations

* Mon Mar 23 1998 Erik Troan <ewt@redhat.com>

- added i18n support

* Sun Mar 22 1998 Erik Troan <ewt@redhat.com>

- added --back

%description
chkconfig provides a simple command-line  tool  for  maintaining  the
/etc/rc.d  directory  hierarchy by relieving system administrators of
directly manipulating the  numerous symbolic links in that directory.

%description -n ntsysv
ntsysv provides a full-screen tool for updating the /etc/rc.d directory
hierarchy, which controls the starting and stopping of system services.

%prep
%setup

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
%attr(0755, root, root) /sbin/chkconfig
%attr(0644, root, root) /usr/man/man8/chkconfig.8
%dir %attr(0755, root, root) /etc/rc.d
%dir %attr(0755, root, root) /etc/rc.d/*
%attr(0644, root, root) /usr/share/locale/*/LC_MESSAGES/chkconfig.mo

%files -n ntsysv
%attr(0755, root, root) /usr/sbin/ntsysv
%attr(0644, root, root) /usr/man/man8/ntsysv.8
