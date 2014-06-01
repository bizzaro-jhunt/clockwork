Name:		clockwork
Version:	0.2.7
Release:	1%{?dist}

Summary:	Clockwork Configuration Management
License:	GPLv3
URL:		http://clockwork.niftylogic.com
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	augeas-devel readline-devel
Requires:	augeas-libs readline yum-utils

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
%{__install} -D -p -m 644 share/vim/cogd.vim %{buildroot}%{_datadir}/vim/vimfiles/syntax/cogd.vim
%{__install} -D -p -m 644 share/vim/clockwork.vim %{buildroot}%{_datadir}/vim/vimfiles/syntax/clockwork.vim
%{__install} -D -p -m 644 share/vim/clockd.vim %{buildroot}%{_datadir}/vim/vimfiles/syntax/clockd.vim
%{__install} -D -p -m 644 share/vim/ft-agent.vim %{buildroot}%{_datadir}/vim/vimfiles/ftdetect/cogd.vim
%{__install} -D -p -m 644 share/vim/ft-server.vim %{buildroot}%{_datadir}/vim/vimfiles/ftdetect/clockd.vim
# Init scripts
%{__install} -D -p -m 755 share/rpm/clockd.init %{buildroot}/etc/init.d/clockd
# Gatherer scripts
%{__install} -d %{buildroot}/var/lib/clockwork/gather.d
%{__install} -D -p -m 755 share/gather.d/* %{buildroot}/var/lib/clockwork/gather.d

%clean
rm -rf %{buildroot}


%post server
/sbin/chkconfig --add clockd

%preun server
/sbin/chkconfig --del clockd

%files agent
%defattr(755,root,root,-)
/sbin/cogd
/var/lib/clockwork
%defattr(644,root,root,-)
%config(noreplace) /etc/clockwork/cogd.conf
%doc /usr/share/man/man1/cogd.1.gz
%doc /usr/share/man/man5/cogd.conf.5.gz
%doc /usr/share/vim/vimfiles/ftdetect/cogd.vim
%doc /usr/share/vim/vimfiles/syntax/cogd.vim

%files server
%defattr(755,root,root,-)
/sbin/cwca
/sbin/clockd
/sbin/cwpol
/etc/init.d/clockd
/var/cache/clockwork
/var/lib/clockwork
%defattr(644,root,root,-)
%config(noreplace) /etc/clockwork/manifest.pol
%config(noreplace) /etc/clockwork/clockd.conf
%doc /usr/share/man/man1/clockd.1.gz
%doc /usr/share/man/man1/cwpol.1.gz
%doc /usr/share/man/man5/clockd.conf.5.gz
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
%doc /usr/share/vim/vimfiles/ftdetect/clockd.vim
%doc /usr/share/vim/vimfiles/syntax/clockd.vim
%doc /usr/share/vim/vimfiles/syntax/clockwork.vim


%changelog
* Sun Apr 22 2012 James Hunt <james@jameshunt.us> - 0.2.4-1
- New version

* Tue Apr 10 2012 James Hunt <james@jameshunt.us> - 0.2.3-7
- Initial RPM build
