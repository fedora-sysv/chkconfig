Summary: Updates and queries runlevel information for system services
Name: chkconfig
%define version 0.1
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

%prep
%setup

%build
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
make instroot=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%attr(0755, root, root) /sbin/chkconfig
%attr(0644, root, root) /usr/man/man8/chkconfig.8
%attr(0644, root, root) /usr/man/man5/chkconfigtab.5
%ghost /var/lib/chkconfigtab
