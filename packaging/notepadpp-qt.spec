Name:           notepadpp-qt
Version:        0.1.0
Release:        1%{?dist}
Summary:        Native Linux Qt6 port of Notepad++ (foundation)

License:        GPLv3+
URL:            https://github.com/clip52/notepad-fedora
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  uchardet-devel
BuildRequires:  pkgconfig
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

Requires:       qt6-qtbase
Requires:       uchardet
Requires:       hicolor-icon-theme

%description
notepadpp-qt is a foundational native Linux port of the well-known Notepad++
text editor, rebuilt on top of Qt6 and the bundled Scintilla/Lexilla editing
components. This is the first milestone (M1) of the port: it establishes the
core editor, build system and Fedora packaging baseline.

Plugin compatibility with the original Notepad++ Win32 plugin ABI is
intentionally dropped — Win32-specific plugins will not load. A future Qt-native
plugin model is being designed; see docs/PORTING.md in the source tree for
details on the porting strategy and current limitations.

The bundled Scintilla editing component is distributed under the HPND license;
the Notepad++ application code remains under GPLv3 or later.

%prep
%autosetup -n %{name}-%{version}

%build
cd PowerEditor-qt
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
cd PowerEditor-qt
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/%{name}.metainfo.xml

%files
%license LICENSE
%doc README.md docs/
%{_bindir}/notepadpp-qt
%{_datadir}/applications/notepadpp-qt.desktop
%{_datadir}/metainfo/notepadpp-qt.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/notepadpp-qt.svg

%changelog
* Mon May 05 2026 Lord Clip <provisionamento@gmail.com> - 0.1.0-1
- Initial Fedora package: foundational Qt6 port milestone (M1).
