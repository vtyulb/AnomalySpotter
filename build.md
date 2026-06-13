# Building & internals

Developer-facing notes for AnomalySpotter. For what the app does and how to use it, see
[README.md](README.md).

## Building

### Linux

Needs Qt 6 (Widgets + Multimedia), plus optionally layer-shell-qt for the overlay above
fullscreen windows on Wayland. On Arch:

```bash
sudo pacman -S cmake ninja gcc qt6-base qt6-multimedia layer-shell-qt
cmake --preset release && cmake --build --preset release
./build/anomaly-spotter
```

The hotkey backend is picked automatically: the Wayland portal (`GlobalShortcuts` over DBus),
or X11 (`XGrabKey` — should work as well, just untested).
On Wayland the shortcuts need **xdg-desktop-portal >= 1.20** (its host app `Registry` is how the
app identifies itself, see *Wayland app identity* below); on older portals they may get filed
under the terminal the app was started from.

### Windows (native)

Install Qt 6 (online installer) with the **Multimedia** module and an MSVC or MinGW kit, then:

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
cmake --build build --config Release
windeployqt build\Release\anomaly-spotter.exe
```

`windeployqt` gathers the Qt DLLs and plugins next to the exe so it can be zipped and shipped.

### Windows ZIP, cross-built from Linux

This produces a self-contained `anomaly-spotter-windows-x64.zip` that runs on a clean Windows
machine. The toolchain and the target Qt must use the **same** UCRT llvm-mingw — Arch's MSVCRT
`mingw-w64-gcc` is ABI-incompatible with the official UCRT Qt, so use llvm-mingw instead.

Prerequisites:
- `llvm-mingw` **20231128** (the exact build Qt 6.11.1 `win64_llvm_mingw` was made with), the
  Linux-hosted tarball from github.com/mstorsjo/llvm-mingw — newer llvm-mingw has libc++ header
  bugs and mismatched ABI.
- Qt 6.11.1 `win64_llvm_mingw` (qtbase + qtmultimedia) extracted into a prefix, e.g. `~/.qt-cross`.
  The FFmpeg DLLs and the multimedia/platform plugins must be present in it.
- A host Qt 6.11.1 (system `/usr`) for the build tools (moc/rcc) via `QT_HOST_PATH`.

```bash
export LLVM_MINGW_ROOT=$HOME/toolchains/llvm-mingw-2023
cmake --preset windows && cmake --build --preset windows
tools/package-windows.sh build-win "$HOME/.qt-cross" anomaly-spotter-windows-x64.zip
```

The Qt prefix (`~/.qt-cross`), `QT_HOST_PATH` and the toolchain file are wired into the
`windows` preset in `CMakePresets.json` — edit it there if your paths differ.

`package-windows.sh` stages the exe, the Qt/FFmpeg/runtime DLLs and the needed plugins
(`platforms`, `styles`, `multimedia`, `tls`, `networkinformation`) with a `qt.conf`, plus the
license paperwork (`LICENSE.txt`, `THIRD-PARTY-NOTICES.txt` and the LGPL/LLVM license texts
from `legal/` — required for redistributing the Qt and FFmpeg DLLs), and zips them.
The result was smoke-tested under wine (launches, renders, finds all DLLs/plugins); the screen
capture itself can only be verified on real Windows.

### Tests

Headless (offscreen platform, no X11/Wayland needed) — run locally or in CI, see `.gitlab-ci.yml`:

```bash
ctest --test-dir build --output-on-failure
```

## Wayland app identity

By default `xdg-desktop-portal` derives an app's identity (and thus where its global shortcuts
are filed) from the systemd scope the process runs in. A binary started straight from a terminal
inherits the terminal's scope, so its shortcuts would be attributed to the terminal (e.g.
polluting `org.kde.yakuake` in `kglobalshortcutsrc`).

The app instead registers itself explicitly via the host app Registry
(`org.freedesktop.host.portal.Registry.Register`, **xdg-desktop-portal >= 1.20**). Two things
make that call work:

- Registration is per D-Bus connection and must be the *first* portal call on it, but Qt already
  talks to the Settings portal on the shared session-bus connection during startup. So the hotkey
  code opens its own private bus connection, registers `anomaly-spotter` there, and runs the whole
  `GlobalShortcuts` session over it.
- The registered app id must match the basename of an existing `.desktop` file, so on Wayland
  start the app writes `~/.local/share/applications/anomaly-spotter.desktop` (pointing `Exec` at
  its own binary; skipped under `AS_CONTROL_FILE`, and a packaged install ships the same file
  anyway).

With that, the shortcuts land under **AnomalySpotter** no matter how the binary was launched. On
portals older than 1.20 the `Register` call fails harmlessly and identification falls back to the
systemd scope of whatever launched the app.

## Development and headless testing

`test-scene` is a controllable fake room: it shows a window, switches scenes from a control file, and publishes its frame to a PNG.

```bash
./build/test-scene /tmp/scene.txt /tmp/frame.png
echo 1 > /tmp/scene.txt
```

The trainer supports two environment hooks for automated testing:

- `AS_FAKE_CAPTURE_FILE=<png>` — read frames from a PNG instead of real screen capture (no portal dialogs).
- `AS_CONTROL_FILE=<file>` — poll the file for commands (`capture-on`, `capture-off`, `snapshot`, `overlay`, `delete-last`), one per line; the file is truncated after reading. Current state is dumped as JSON to `<file>.state`. Global hotkey registration is skipped in this mode.

```bash
AS_FAKE_CAPTURE_FILE=/tmp/frame.png AS_CONTROL_FILE=/tmp/ctl ./build/anomaly-spotter
echo capture-on >> /tmp/ctl
echo snapshot >> /tmp/ctl
cat /tmp/ctl.state
```
