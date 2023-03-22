Summary: A system tool for maintaining the /etc/rc*.d hierarchy
Name: chkconfig
Version: 1.21
Release: %autorelease
License: GPL-2.0-only
URL: https://github.com/fedora-sysv/chkconfig
Source: https://github.com/fedora-sysv/chkconfig/archive/%{version}.tar.gz#/%{name}-%{version}.tar.gz
BuildRequires: newt-devel gettext popt-devel libselinux-devel beakerlib gcc systemd-devel make
Conflicts: initscripts <= 5.30-1

Provides: /sbin/chkconfig

%description
Chkconfig is a basic system utility.  It updates and queries runlevel
information for system services.  Chkconfig manipulates the numerous
symbolic links in /etc/rc.d, to relieve system administrators of some 
of the drudgery of manually editing the symbolic links.

%package -n ntsysv
Summary: A tool to set the stop/start of system services in a runlevel
Requires: chkconfig = %{version}-%{release}

%description -n ntsysv
Ntsysv provides a simple interface for setting which system services
are started or stopped in various runlevels (instead of directly
manipulating the numerous symbolic links in /etc/rc.d). Unless you
specify a runlevel or runlevels on the command line (see the man
page), ntsysv configures the current runlevel (5 if you're using X).

%package -n alternatives
Summary: A tool to maintain symbolic links determining default commands

%description -n alternatives
alternatives creates, removes, maintains and displays information about the
symbolic links comprising the alternatives system. It is possible for several
programs fulfilling the same or similar functions to be installed on a single
system at the same time.

%prep
%setup -q

%build
%make_build RPM_OPT_FLAGS="$RPM_OPT_FLAGS" LDFLAGS="$RPM_LD_FLAGS"

%check
make check

%install
rm -rf $RPM_BUILD_ROOT
%make_install MANDIR=%{_mandir} SBINDIR=%{_sbindir}

mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
ln -s rc.d/init.d $RPM_BUILD_ROOT/etc/init.d
for n in 0 1 2 3 4 5 6; do
    mkdir -p $RPM_BUILD_ROOT/etc/rc.d/rc${n}.d
    ln -s rc.d/rc${n}.d $RPM_BUILD_ROOT/etc/rc${n}.d
done
mkdir -p $RPM_BUILD_ROOT/etc/chkconfig.d

%find_lang %{name}

%files -f %{name}.lang
%defattr(-,root,root)
%{!?_licensedir:%global license %%doc}
%license COPYING
%{_sbindir}/chkconfig
%{_sysconfdir}/chkconfig.d
%{_sysconfdir}/init.d
%{_sysconfdir}/rc.d
%{_sysconfdir}/rc.d/init.d
%{_sysconfdir}/rc[0-6].d
%{_sysconfdir}/rc.d/rc[0-6].d
%{_mandir}/*/chkconfig*
%{_prefix}/lib/systemd/systemd-sysv-install

%files -n ntsysv
%defattr(-,root,root)
%{_sbindir}/ntsysv
%{_mandir}/*/ntsysv.8*

%files -n alternatives
%license COPYING
%dir /etc/alternatives
%{_sbindir}/update-alternatives
%{_sbindir}/alternatives
%{_mandir}/*/update-alternatives*
%{_mandir}/*/alternatives*
%dir /var/lib/alternatives

%changelog
%autochangelog
