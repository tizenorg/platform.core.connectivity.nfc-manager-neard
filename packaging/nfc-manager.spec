Name:       nfc-manager
Summary:    NFC framework manager
Version: 0.1.0
Release:    0
Group:      libs
License:    Flora Software License
Source0:    %{name}-%{version}.tar.gz
Source1:    libnfc-manager-0.init.in
Source2:    nfc-manager.service
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
BuildRequires: pkgconfig(pkgmgr)
BuildRequires: pkgconfig(pkgmgr-info)
BuildRequires: pkgconfig(ecore-x)
BuildRequires: pkgconfig(pmapi)
BuildRequires: python
BuildRequires: python-xml
BuildRequires: gettext-tools
Requires(post):   /sbin/ldconfig
Requires(post):   /usr/bin/vconftool
Requires(postun): /sbin/ldconfig
Requires:         nfc-common-lib = %{version}
Requires:         nfc-client-lib = %{version}


%description
NFC library Manager.


%prep
%setup -q
cp %{SOURCE1001} .

%package -n nfc-common-lib
Summary:    NFC common library
Group:      Development/Libraries


%description -n nfc-common-lib
NFC Common library.


%package -n nfc-common-lib-devel
Summary:    NFC common library (devel)
Group:      libs
Requires:   nfc-common-lib = %{version}


%description -n nfc-common-lib-devel
NFC common library (devel)


%package -n nfc-client-lib
Summary:    NFC client library
Group:      Development/Libraries
Requires:   nfc-common-lib = %{version}


%description -n nfc-client-lib
NFC Client library.


%package -n nfc-client-lib-devel
Summary:    NFC client library (devel)
Group:      libs
Requires:   nfc-client-lib = %{version}


%description -n nfc-client-lib-devel
NFC client library (devel)


#%package -n nfc-client-test
#Summary:    NFC client test
#Group:      Development/Libraries
#Requires:   %{name} = %{version}-%{release}


#%description -n nfc-client-test
#NFC client test (devel)


%build
export LDFLAGS+="-Wl,--rpath=%{_prefix}/lib -Wl,--as-needed"
LDFLAGS="$LDFLAGS" cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}


%install
%make_install

install -D -m 0755 %SOURCE1 %{buildroot}%{_sysconfdir}/init.d/libnfc-manager-0
mkdir -p %{buildroot}/opt/usr/share/nfc_debug

mkdir -p %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants
cp -af %{SOURCE2} %{buildroot}%{_libdir}/systemd/system/
ln -s ../%{name}.service %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants/%{name}.service

%post
/sbin/ldconfig
#ln -s /etc/init.d/libnfc-manager-0 /etc/rc.d/rc3.d/S81libnfc-manager-0 -f
#ln -s /etc/init.d/libnfc-manager-0 /etc/rc.d/rc5.d/S81libnfc-manager-0 -f


systemctl daemon-reload
if [ $1 == 1 ]; then
    systemctl restart %{name}.service
fi


%post -n nfc-client-lib
/sbin/ldconfig
vconftool set -t bool db/nfc/feature 0 -u 5000 -f
vconftool set -t int db/nfc/se_type 0 -u 5000 -f
vconftool set -t bool db/nfc/predefined_item_state 0 -u 5000 -f
vconftool set -t string db/nfc/predefined_item "None" -u 5000 -f
vconftool set -t bool db/nfc/enable 0 -u 5000 -f

%postun
/sbin/ldconfig
#mkdir -p /etc/rc.d/rc3.d
#mkdir -p /etc/rc.d/rc5.d
#rm -f /etc/rc.d/rc3.d/S81libnfc-manager-0
#rm -f /etc/rc.d/rc5.d/S81libnfc-manager-0

if [ $1 == 0 ]; then
    systemctl stop %{name}.service
fi
systemctl daemon-reload


%post -n nfc-common-lib -p /sbin/ldconfig


%postun -n nfc-common-lib -p /sbin/ldconfig


%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_bindir}/nfc-manager-daemon
#%{_bindir}/ndef-tool
%{_libdir}/systemd/system/%{name}.service
%{_libdir}/systemd/system/multi-user.target.wants/%{name}.service
%{_sysconfdir}/init.d/libnfc-manager-0
%{_datadir}/dbus-1/services/org.tizen.NetNfcService.service
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
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/libnfc.so
%{_libdir}/pkgconfig/nfc.pc
%{_includedir}/nfc/*.h


%files -n nfc-common-lib
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/libnfc-common-lib.so.*
%{_datadir}/nfc-manager-daemon/sounds/*
%license LICENSE.Flora


%files -n nfc-common-lib-devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/libnfc-common-lib.so
%{_libdir}/pkgconfig/nfc-common-lib.pc
%{_includedir}/nfc-common-lib/*.h


#%files -n nfc-client-test
#%manifest nfc-client-test.manifest
#%defattr(-,root,root,-)
#%{_bindir}/nfc_client
#%license LICENSE.Flora
