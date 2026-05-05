# Building notepadpp-qt on Linux

This document covers building `notepadpp-qt` from source on Fedora and
producing an installable RPM. Other Linux distributions should work with
the equivalent package names from their own repositories.

## 1. Supported distributions

- **Fedora 43+** — primary target. The build is developed and tested
  against Fedora 43 with Qt 6.10.
- Other distributions (RHEL/CentOS Stream 10+, openSUSE Tumbleweed,
  Debian/Ubuntu with Qt 6.5+, Arch) are expected to work once the
  equivalent `-devel` packages are installed. Package names will differ;
  the dependencies themselves do not.

The hard requirements are:

- Qt 6.5 or newer (Qt Base + Qt Tools).
- CMake 3.20 or newer.
- A C++17-capable compiler. gcc-c++ 15+ is what the project is developed
  against; clang 16+ also works.
- `uchardet` (development headers) for encoding detection.
- `pkg-config`.
- Python 3 (only at build time, to regenerate `ScintillaEdit.cpp` /
  `ScintillaEdit.h` from `WidgetGen.py`).

## 2. Build dependencies (Fedora)

```bash
sudo dnf install \
    qt6-qtbase-devel \
    qt6-qttools-devel \
    cmake \
    gcc-c++ \
    uchardet-devel \
    pkgconfig \
    python3
```

For RPM packaging you additionally need:

```bash
sudo dnf install rpm-build rpmdevtools
rpmdev-setuptree   # creates ~/rpmbuild/{SOURCES,SPECS,...} once
```

## 3. Building from source

```bash
git clone https://github.com/clip52/notepad-fedora.git
cd notepad-fedora

# Generate ScintillaEdit.cpp / .h from the Scintilla interface description.
# This step is required once after cloning and again whenever Scintilla.iface
# changes. Skipping it leaves PowerEditor-qt without ScintillaEdit symbols.
cd scintilla/qt/ScintillaEdit && python3 WidgetGen.py && cd -

# Configure and build the Qt port.
cmake -B build -S PowerEditor-qt -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run from the build directory without installing.
./build/notepadpp-qt
```

To install system-wide after a build:

```bash
sudo cmake --install build
```

The install prefix defaults to `/usr/local`. Pass `-DCMAKE_INSTALL_PREFIX=/usr`
at configure time to match distro conventions, or just install via the RPM
(next section) which handles paths, the desktop file, and MIME entries
correctly.

## 4. Building the RPM

From a clean checkout:

```bash
# Create a source tarball matching the Version: in the spec.
tar czf ~/rpmbuild/SOURCES/notepadpp-qt-0.1.0.tar.gz \
    --transform 's,^,notepadpp-qt-0.1.0/,' .

# Build both source and binary RPMs.
rpmbuild -ba packaging/notepadpp-qt.spec
```

Outputs land in:

- `~/rpmbuild/SRPMS/notepadpp-qt-0.1.0-1.fc43.src.rpm`
- `~/rpmbuild/RPMS/x86_64/notepadpp-qt-0.1.0-1.fc43.x86_64.rpm`

Install with:

```bash
sudo dnf install ~/rpmbuild/RPMS/x86_64/notepadpp-qt-0.1.0-1.fc43.x86_64.rpm
```

The package installs the binary, a `.desktop` launcher, an SVG icon, and
MIME associations for common text types. After install, `notepadpp-qt`
appears in the desktop menu under Accessories / Development.

## 5. Running

```bash
notepadpp-qt                # empty editor
notepadpp-qt file.txt       # open a single file
notepadpp-qt *.cpp *.h      # open several files in tabs
```

A desktop launcher is installed by the RPM, so the editor also shows up
under "Text Editor / Notepad++ (Qt)" in GNOME, KDE, or any XDG-compliant
desktop. `xdg-mime default notepadpp-qt.desktop text/plain` makes it the
default handler for plain text if you want that.

Settings live under `~/.config/notepadpp-qt/` (per `QSettings`
defaults). The directory is created on first run.

## 6. Troubleshooting

**`fatal error: ScintillaEdit.cpp: No such file or directory`** — you
skipped the `WidgetGen.py` step. Run it from
`scintilla/qt/ScintillaEdit/` and reconfigure:

```bash
cd scintilla/qt/ScintillaEdit && python3 WidgetGen.py && cd -
cmake --build build -j$(nproc)
```

**`Could NOT find Qt6 (missing: Qt6_DIR)`** — Qt6 development packages
aren't installed, or CMake is finding Qt5 first. On Fedora the package is
`qt6-qtbase-devel` (note the `6`). Verify with:

```bash
qmake6 -query QT_VERSION
```

If `qmake6` is missing, the package isn't installed. If a different Qt
version is found first, hint CMake explicitly:

```bash
cmake -B build -S PowerEditor-qt -DCMAKE_PREFIX_PATH=/usr/lib64/qt6
```

**`undefined reference to uchardet_*`** — `uchardet-devel` is missing, or
the linker isn't finding `libuchardet.so`. Install the package and
reconfigure (CMake caches the previous failed lookup):

```bash
sudo dnf install uchardet-devel
rm -rf build
cmake -B build -S PowerEditor-qt -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Build is slow / runs out of memory.** Drop the parallel job count:
`cmake --build build -j2`. Scintilla and Lexilla each have a few large
translation units that peak at ~1 GB of RAM per process.

**Wayland / HiDPI scaling looks off.** Qt picks up `QT_SCALE_FACTOR` and
`QT_AUTO_SCREEN_SCALE_FACTOR` from the environment. For mixed-DPI
multi-monitor setups, `QT_ENABLE_HIGHDPI_SCALING=1` is the modern Qt 6
default and usually does the right thing.

Last updated: 2026-05-05
