Name:     push-agent
Summary:  oFono push agent
Version:  1.1.0
Release:  1
Group:    Communications/Telephony and IM
License:  GPLv2
URL:      https://github.com/nemomobile/push-agent
Source0:  %{name}-%{version}.tar.bz2
Requires: dbus
Requires: systemd

Requires(preun):  systemd
Requires(post):   systemd
Requires(postun): systemd

BuildRequires: python
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(libwspcodec) >= 2.0

%description
oFono push agent

%prep
%setup -q -n %{name}-%{version}

%build
make release

%install
rm -rf %{buildroot}
mkdir -p  %{buildroot}/%{_sbindir}
mkdir -p %{buildroot}/%{_sysconfdir}/push-agent
mkdir -p %{buildroot}/%{_lib}/systemd/system/
mkdir -p %{buildroot}/%{_lib}/systemd/system/network.target.wants
cp build/release/push-agent %{buildroot}/%{_sbindir}
cp push-agent.service %{buildroot}/%{_lib}/systemd/system/
ln -s ../push-agent.service %{buildroot}/%{_lib}/systemd/system/network.target.wants/

%preun
systemctl stop push-agent.service

%post
systemctl daemon-reload
systemctl start push-agent.service

%postun
systemctl daemon-reload

%files
%defattr(-,root,root,-)
%dir %{_sysconfdir}/push-agent
%{_sbindir}/push-agent
/%{_lib}/systemd/system/push-agent.service
/%{_lib}/systemd/system/network.target.wants/push-agent.service
