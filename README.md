# AnomalySpotter

Anomaly-hunting helper for *Observation Duty*-style camera games. Built first for **Dead Signal**, but it compares screenshots instead of reading game memory, so it works with any game of the genre. It memorizes room/camera snapshots and every 200 ms shows how much the current screen differs from each of them; hold F6 to highlight the differences on top of the game.

## Platforms

Developed and used on **Arch Linux / KDE Wayland**; also runs on **Windows**. X11 should work
as well, just untested.
The Windows build is cross-compiled from Linux and smoke-tested under wine (it launches, renders
its window and finds all its DLLs/plugins); the screen capture itself is only verifiable on real
Windows, so run the game in **borderless windowed** mode and check the first F5 snapshot there.

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

## Usage

1. Pick the monitor the game runs on and click "Start capture". On Wayland the system shows a screen picker dialog — choose the same monitor.
2. On the first launch KDE asks to allow the global F5/F6 hotkeys — confirm. Without confirmation the hotkeys work only while the trainer window is focused. On Windows the hotkeys are registered automatically.
3. **F5** — save the current frame to memory (room reference).
4. The table refreshes every 200 ms: difference percentage between the current screen and each reference; the best (most similar) one is shown in bold. While capture is running, a small HUD in the top-right corner of the game screen shows the difference % against the best match (toggle with the "Show difference % on screen" checkbox); the HUD corner is excluded from comparison so it never inflates its own percentage. The HUD digits are rendered at double size for readability and turn red at ≥0.5 %, then blink furiously once the difference has stayed ≥2 % for a full second. Above 20 % the whole HUD goes gray — that means no snapshot matches the current view and you probably need to take one here.
5. **F6** — shows the differences against the most similar reference as a red overlay: filled highlight plus outlined boxes for large zones. Comparison is paused while the overlay is up, so the highlight never feeds back into the capture. The "Overlay key mode" selector picks how F6 behaves:
   - **Toggle** (default) — press to show, press again to hide.
   - **Blink** — same as Toggle, but the red highlight flashes 5 times a second, alternating with the game's own pixels. The overlay window itself stays up the whole time (only its content repaints), so compositor window-open/close effects are not retriggered by the blinking.
   - **Hold** — the overlay is visible only while F6 is held down.

   The "Differences" button toggles the overlay with a click in any mode. The mode handling is shared code, so Linux and Windows behave identically.
6. Use the "Delete last" / "Delete selected" buttons to drop a snapshot taken by mistake. "Delete last" also has an optional global hotkey: it is unbound by default — on Wayland assign it in the desktop's keyboard-shortcuts settings ("Delete last snapshot" under AnomalySpotter), on Windows set `deleteLastKey` in the config.
7. The sensitivity threshold suppresses noise from the video codec, the cursor, and small animations.

### Changing the hotkeys

The default keys are **F5** (snapshot) and **F6** (overlay). How to rebind depends on the platform,
because on Wayland the compositor owns the global-shortcut binding, not the app:

- **Wayland:** the global shortcuts are registered with the compositor on first run (approve the
  portal prompt). Rebind them in the desktop's keyboard-shortcuts settings — the in-window
  **"Open shortcut settings…"** button opens it (KDE `systemsettings kcm_keys`, GNOME control
  center). They are listed under the app id **`anomaly-spotter`** (shown as **AnomalySpotter**),
  regardless of how the app was started — see *Wayland app identity* below. The buttons in the
  window show whatever is actually bound (the compositor may pick different keys if F5/F6 are taken).
- **Windows:** set `snapshotKey` / `overlayKey` / `deleteLastKey` (a function-key number,
  1–12; `deleteLastKey` 0 = disabled, the default) in
  `%APPDATA%\anomaly-spotter\anomaly-spotter.ini` — the in-window **"Edit config…"** button
  opens it in your editor — and relaunch. The same keys in
  `~/.config/anomaly-spotter/anomaly-spotter.conf` apply to the X11 backend.

### Wayland app identity

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

## Notes

- Pressing F5 while the overlay is visible first hides the overlay and takes the snapshot 350 ms later, so the highlight never lands in a reference.
- References live only in RAM and are gone on exit.
- The overlay is transparent to mouse input and never steals focus from the game.
- The HUD and the diff overlay appear on the screen that is actually being captured: it is detected by matching the frame size against screen resolutions, with the screen selected in the UI as a fallback.
- On Wayland the buttons show the hotkeys actually bound by the portal (KDE may assign different keys if F5/F6 are taken) and update live when the keys are rebound in the system settings.
- On Windows the overlay cannot draw above exclusive-fullscreen games — run the game in borderless/windowed mode.

## License

MIT — see [LICENSE](LICENSE).
