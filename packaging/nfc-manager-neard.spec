%bcond_with wayland	1
%bcond_with x

Name:       nfc-manager-neard
Summary:    NFC framework manager
Version:    0.1.7
Release:    0
Group:      Network & Connectivity/NFC
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    %{name}.service
Source1001: %{name}.manifest
Requires:   neard
Requires:   neardal
BuildRequires:  cmake
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(tapi)
BuildRequires:  pkgconfig(capi-network-wifi)
BuildRequires:  pkgconfig(capi-network-bluetooth)
BuildRequires:  pkgconfig(mm-sound)
BuildRequires:  pkgconfig(appsvc)
BuildRequires:  pkgconfig(feedback)
BuildRequires:  pkgconfig(capi-media-wav-player)
BuildRequires:  pkgconfig(libssl)
BuildRequires:  pkgconfig(libcrypto)
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(pkgmgr)
BuildRequires:  pkgconfig(pkgmgr-info)
%if %{with x}
BuildRequires: pkgconfig(ecore-x)
%endif
%if %{with wayland}
BuildRequires: pkgconfig(ecore-wayland)
%endif
BuildRequires:  pkgconfig(deviced)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  pkgconfig(neardal)
BuildRequires:  python
BuildRequires:  python-xml
BuildRequires:  gettext-tools
%ifarch %arm
BuildRequires:  pkgconfig(capi-network-wifi-direct)
%global ARM_DEF "-DARM_TARGET=Y"
%endif

Requires(post):   /sbin/ldconfig
Requires(post):   /usr/bin/vconftool
Requires(postun): /sbin/ldconfig
Requires:         nfc-client-lib-neard = %{version}


%description
Tizen NFC framework manager.


%package -n nfc-common-neard-devel
Summary:    NFC common library (devel)
Group:      Network & Connectivity/Development


%description -n nfc-common-neard-devel
NFC manager common header for internal development.


%package -n nfc-client-lib-neard
Summary:    NFC client library
Group:      Network & Connectivity/NFC


%description -n nfc-client-lib-neard
NFC manager Client library for NFC client applications.


%package -n nfc-client-lib-neard-devel
Summary:    NFC client library (devel)
Group:      Network & Connectivity/Development
Requires:   nfc-client-lib-neard = %{version}


%description -n nfc-client-lib-neard-devel
NFC manager Client library for developing NFC client applications.

#%%package -n nfc-client-test
#Summary:    NFC client test
#Group:      Network & Connectivity/NFC
#Requires:   %%{name} = %%{version}


#%%description -n nfc-client-test
#NFC client test (devel)

%prep
%setup -q
cp %{SOURCE1001} .

%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
%cmake . -DMAJORVER=${MAJORVER} -DFULLVER=%{version} %{?ARM_DEF} \
%if %{with wayland}
-DWAYLAND_SUPPORT=On \
%else
-DWAYLAND_SUPPORT=Off \
%endif
%if %{with x}
-DX11_SUPPORT=On
%else
-DX11_SUPPORT=Off
%endif

make %{?_smp_mflags}

%install
%make_install

install -d %{buildroot}%{_unitdir}
install -d %{buildroot}%{_unitdir}/multi-user.target.wants/
install -m 644 %{S:1} %{buildroot}%{_unitdir}/%{name}.service
ln -s ../%{name}.service %{buildroot}%{_unitdir}/multi-user.target.wants/%{name}.service

%post
/sbin/ldconfig

systemctl daemon-reload
if [ $1 == 1 ]; then
    systemctl restart %{name}.service
fi

%post -n nfc-client-lib-neard
/sbin/ldconfig

%postun
/sbin/ldconfig
if [ $1 == 0 ]; then
    systemctl stop %{name}.service
fi
systemctl daemon-reload

%postun -n nfc-client-lib-neard -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_bindir}/nfc-manager-daemon
#%%{_bindir}/ndef-tool
%{_unitdir}/%{name}.service
%{_unitdir}/multi-user.target.wants/%{name}.service
%{_datadir}/dbus-1/system-services/org.tizen.NetNfcService.service
%{_datadir}/packages/nfc-manager.xml
%{_datadir}/nfc-manager-daemon/sounds/*
%license LICENSE.APLv2


%files -n nfc-client-lib-neard
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/libnfc.so.*
%license LICENSE.APLv2


%files -n nfc-client-lib-neard-devel
%defattr(-,root,root,-)
%{_libdir}/libnfc.so
%{_libdir}/pkgconfig/nfc.pc
%{_includedir}/nfc/*.h


%files -n nfc-common-neard-devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/nfc-common.pc
%{_includedir}/nfc-common/*.h

#%%files -n nfc-client-test
#%%manifest nfc-client-test.manifest
#%%defattr(-,root,root,-)
#%%{_bindir}/nfc_client
#%%license LICENSE.APLv2
