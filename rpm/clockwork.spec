Name:		clockwork
Version:	0.2.4
Release:	1%{?dist}

Summary:	Clockwork Configuration Management
License:	GPLv3
URL:		http://clockwork.niftylogic.com
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	augeas-devel sqlite-devel libgear readline-devel ctest
Requires:	augeas-libs sqlite libgear readline yum-utils

%description

Clockwork

%package server
Summary:	Clockwork Server
Group:		System Environment/Daemons

%description server

Clockwork Server

%package agent
Summary:	Clockwork Agent
Group:		System Environment/Daemons

%description agent

Clockwork Agent

%prep
%setup -q

%pre
getent group clockwork >/dev/null || groupadd -r clockwork
getent passwd clockwork >/dev/null || useradd -Mr -c "Clockwork" -g clockwork -d /var/lib/clockwork clockwork
exit 0


%build
./configure --user clockwork --group clockwork --userhome /var/lib/clockwork
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
# VIM stuff
%{__install} -D -p -m 644 share/vim/cwa.vim %{buildroot}%{_datadir}/vim/vimfiles/syntax/cwa.vim
%{__install} -D -p -m 644 share/vim/pol.vim %{buildroot}%{_datadir}/vim/vimfiles/syntax/pol.vim
%{__install} -D -p -m 644 share/vim/policyd.vim %{buildroot}%{_datadir}/vim/vimfiles/syntax/policyd.vim
%{__install} -D -p -m 644 share/vim/ft-agent.vim %{buildroot}%{_datadir}/vim/vimfiles/ftdetect/cwa.vim
%{__install} -D -p -m 644 share/vim/ft-server.vim %{buildroot}%{_datadir}/vim/vimfiles/ftdetect/policyd.vim
# Init scripts
%{__install} -D -p -m 755 share/rpm/policyd.init %{buildroot}/etc/init.d/policyd
# Gatherer scripts
%{__install} -d -o root -g root %{buildroot}/var/lib/clockwork/gather.d
%{__install} -D -p -m 755 share/gather.d/* %{buildroot}/var/lib/clockwork/gather.d

%clean
rm -rf %{buildroot}


%post server
/sbin/chkconfig --add policyd

%preun server
/sbin/chkconfig --del policyd

%files agent
%defattr(755,root,root,-)
/sbin/cwa
/sbin/cwcert
/etc/clockwork/ssl
/var/lib/clockwork
%defattr(644,root,root,-)
%config(noreplace) /etc/clockwork/cwa.conf
%doc /usr/share/man/man1/cwa.1.gz
%doc /usr/share/man/man5/cwa.conf.5.gz
%doc /usr/share/vim/vimfiles/ftdetect/cwa.vim
%doc /usr/share/vim/vimfiles/syntax/cwa.vim
%doc /usr/share/clockwork/agent.sql

%files server
%defattr(755,root,root,-)
/sbin/cwca
/sbin/policyd
/sbin/cwpol
/etc/init.d/policyd
/etc/clockwork/ssl
/var/cache/clockwork
/var/lib/clockwork
%defattr(644,root,root,-)
%config(noreplace) /etc/clockwork/manifest.pol
%config(noreplace) /etc/clockwork/policyd.conf
%doc /usr/share/man/man1/policyd.1.gz
%doc /usr/share/man/man1/cwpol.1.gz
%doc /usr/share/man/man5/policyd.conf.5.gz
%doc /usr/share/man/man5/res_dir.5.gz
%doc /usr/share/man/man5/res_exec.5.gz
%doc /usr/share/man/man5/res_file.5.gz
%doc /usr/share/man/man5/res_group.5.gz
%doc /usr/share/man/man5/res_host.5.gz
%doc /usr/share/man/man5/res_package.5.gz
%doc /usr/share/man/man5/res_service.5.gz
%doc /usr/share/man/man5/res_sysctl.5.gz
%doc /usr/share/man/man5/res_user.5.gz
%doc /usr/share/man/man7/clockwork.7.gz
%doc /usr/share/clockwork/help/*
%doc /usr/share/clockwork/master.sql
%doc /usr/share/vim/vimfiles/ftdetect/policyd.vim
%doc /usr/share/vim/vimfiles/syntax/policyd.vim
%doc /usr/share/vim/vimfiles/syntax/pol.vim


%changelog
* Sun Apr 22 2012 James Hunt <james@niftylogic.com> - 0.2.4-1
- New version

* Tue Apr 10 2012 James Hunt <james@niftylogic.com> - 0.2.3-7
- Initial RPM build
