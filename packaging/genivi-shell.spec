Name:           genivi-shell
Version:        0.2.3
Release:        0
Summary:        GENIVI Shell Plugin-in
License:        Apache-2.0
Group:          Graphics & UI Framework/Wayland Window System
Url:            http://git.projects.genivi.org/wayland-ivi-extension.git
Source0:        %name-%version.tar.gz
Source1001:     genivi-shell.manifest
BuildRequires:  cmake
BuildRequires:  weston-ivi-shell-devel >= 0.1.7
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-egl)
BuildRequires:  pkgconfig(wayland-server)
BuildRequires:  pkgconfig(cairo)
BuildRequires:  pkgconfig(libffi)
BuildRequires:  pkgconfig(weston) >= 1.5
BuildRequires:  pkgconfig(xkbcommon)

%description
This package provides a weston plugin implementing the GENIVI layer
manager client interface.

%package devel
Summary: Development files for package %{name}
Group:   Graphics & UI Framework/Development
Requires: %{name} = %{version}-%{release}
%description devel
This package provides header files and other developer files needed for
creating GENIVI layer manager clients.

%prep
%setup -q
cp %{SOURCE1001} .

%cmake .

%build

make %{?_smp_mflags} V=1

%install
%make_install

install -d %{buildroot}/%{_datadir}/%{name}/protocol/

install -m 644 protocol/ivi-application.xml %{buildroot}/%{_datadir}/%{name}/protocol/
install -m 644 protocol/ivi-controller.xml %{buildroot}/%{_datadir}/%{name}/protocol/

install -m 644 protocol/ivi-application-server-protocol.h \
    %{buildroot}/%{_datadir}/%{name}/protocol/

install -m 644 protocol/ivi-application-protocol.c \
    %{buildroot}/%{_datadir}/%{name}/protocol/

install -m 644 protocol/ivi-controller-server-protocol.h \
    %{buildroot}/%{_datadir}/%{name}/protocol/

install -m 644 protocol/ivi-controller-protocol.c \
    %{buildroot}/%{_datadir}/%{name}/protocol/

%post   -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%defattr(-,root,root)
%{_bindir}/IVISurfaceCreator
%{_bindir}/LayerManagerControl
%{_bindir}/EGLWLMockNavigation
%{_libdir}/libilmClient.so.*
%{_libdir}/libilmCommon.so.*
%{_libdir}/libilmControl.so.*
%{_libdir}/weston/ivi-controller.so

%files devel
%defattr(-,root,root)
%{_includedir}/ilm/ilm_client.h
%{_includedir}/ilm/ilm_common.h
%{_includedir}/ilm/ilm_control.h
%{_includedir}/ilm/ilm_platform.h
%{_includedir}/ilm/ilm_types.h
%{_libdir}/libilmClient.so
%{_libdir}/libilmCommon.so
%{_libdir}/libilmControl.so
%{_datadir}/%{name}/protocol/*.xml
%{_datadir}/%{name}/protocol/*.h
%{_datadir}/%{name}/protocol/*.c

