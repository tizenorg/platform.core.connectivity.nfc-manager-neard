Name:       nfc-manager
Summary:    NFC framework manager
Version: 	0.1.6
Release:    0
Group:      Network & Connectivity/NFC
License:    Flora
Source0:    %{name}-%{version}.tar.gz
Source1:    %{name}.service
Source1001: 	%{name}.manifest
BuildRequires: cmake
BuildRequires: pkgconfig(aul)
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(gobject-2.0)
BuildRequires: pkgconfig(security-server)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(tapi)
BuildRequires: pkgconfig(bluetooth-api)
BuildRequires: pkgconfig(capi-network-wifi)
BuildRequires: pkgconfig(mm-sound)
BuildRequires: pkgconfig(appsvc)
BuildRequires: pkgconfig(svi)
BuildRequires: pkgconfig(capi-media-wav-player)
BuildRequires: pkgconfig(libssl)
BuildRequires: pkgconfig(libcurl)
BuildRequires: pkgconfig(pkgmgr)
BuildRequires: pkgconfig(pkgmgr-info)
BuildRequires: pkgconfig(ecore-x)
BuildRequires: pkgconfig(pmapi)
BuildRequires: python
BuildRequires: python-xml
BuildRequires: gettext-tools
%ifarch %arm
BuildRequires: pkgconfig(wifi-direct)
%global ARM_DEF "-DARM_TARGET=Y"
%endif

Requires(post):   /sbin/ldconfig
Requires(post):   /usr/bin/vconftool
Requires(postun): /sbin/ldconfig
Requires:         nfc-client-lib = %{version}


%description
Tizen NFC framework manager.


%prep
%setup -q
cp %{SOURCE1001} .


%package -n nfc-common-devel
Summary:    NFC common library (devel)
Group:      Network & Connectivity/Development


%description -n nfc-common-devel
NFC manager common header for internal development.


%package -n nfc-client-lib
Summary:    NFC client library
Group:      Network & Connectivity/NFC


%description -n nfc-client-lib
NFC manager Client library for NFC client applications.


%package -n nfc-client-lib-devel
Summary:    NFC client library (devel)
Group:      Network & Connectivity/Development
Requires:   nfc-client-lib = %{version}


%description -n nfc-client-lib-devel
NFC manager Client library for developing NFC client applications.



#%%package -n nfc-client-test
#Summary:    NFC client test
#Group:      Network & Connectivity/NFC
#Requires:   %%{name} = %%{version}


#%%description -n nfc-client-test
#NFC client test (devel)


%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
%cmake . -DMAJORVER=${MAJORVER} -DFULLVER=%{version} %{?ARM_DEF}


%install
%make_install

mkdir -p %{buildroot}/opt/usr/share/nfc_debug

mkdir -p %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants
cp -af %{SOURCE1} %{buildroot}%{_libdir}/systemd/system/
ln -s ../%{name}.service %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants/%{name}.service

%post
/sbin/ldconfig

systemctl daemon-reload
if [ $1 == 1 ]; then
    systemctl restart %{name}.service
fi


%post -n nfc-client-lib
/sbin/ldconfig
vconftool set -t bool db/nfc/feature 0 -u 5000 -f
vconftool set -t bool db/nfc/predefined_item_state 0 -u 5000 -f
vconftool set -t string db/nfc/predefined_item "None" -u 5000 -f
vconftool set -t bool db/nfc/enable 0 -u 5000 -f
vconftool set -t int db/nfc/se_type 0 -u 5000 -f

%postun
/sbin/ldconfig
if [ $1 == 0 ]; then
    systemctl stop %{name}.service
fi
systemctl daemon-reload


%postun -n nfc-client-lib -p /sbin/ldconfig


%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_bindir}/nfc-manager-daemon
#%%{_bindir}/ndef-tool
%{_libdir}/systemd/system/%{name}.service
%{_libdir}/systemd/system/multi-user.target.wants/%{name}.service
%{_datadir}/dbus-1/system-services/org.tizen.NetNfcService.service
%{_datadir}/packages/%{name}.xml
%{_datadir}/nfc-manager-daemon/sounds/*
%attr(0775,-,5000) %dir /opt/usr/share/nfc_debug
%license LICENSE.Flora


%files -n nfc-client-lib
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/libnfc.so.*
%license LICENSE.Flora


%files -n nfc-client-lib-devel
%defattr(-,root,root,-)
%{_libdir}/libnfc.so
%{_libdir}/pkgconfig/nfc.pc
%{_includedir}/nfc/*.h


%files -n nfc-common-devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/nfc-common.pc
%{_includedir}/nfc-common/*.h


#%%files -n nfc-client-test
#%%manifest nfc-client-test.manifest
#%%defattr(-,root,root,-)
#%%{_bindir}/nfc_client
#%%license LICENSE.Flora
