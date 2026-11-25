# Atari Gamestation Go

Build for the console as Linux ARMv7 hard-float with NEON, using its matching
`arm-linux-gnueabihf` SDK/sysroot. The desktop OpenGL renderer is disabled;
EDuke32 uses the software renderer with SDL2 input/audio.

```sh
SYSROOT=/path/to/gamestation-go-sysroot \
  CROSS=arm-linux-gnueabihf- \
  SDLCONFIG=arm-linux-gnueabihf-sdl2-config \
  RUNTIME_LIBDIR=/path/to/target-runtime-libs \
  bash platform/gamestation-go-build.sh
```

The result is `package/gamestation-go/eduke32`. Deploy it with
`gamecontrollerdb.txt` and the optional `lib/` directory.

Configure the GameStation controller in EDuke32's **Controller Setup** menu.

Platform-specific input handling (all guarded by `EDUKE32_GAMESTATION_GO`):

- The **Home** button (controller `misc1`) opens EDuke32's quit confirmation
  only while the top-level main menu is on screen, and exits gracefully to the
  GameStation launcher. It is inert during gameplay and in submenus.
- The **trackball** ("XC Track Ball") reports ball motion on REL_Z/REL_RX,
  which stock SDL never maps to a mouse; a small evdev forwarder claims the
  device before SDL initializes and injects absolute mouse motion and buttons,
  enabling the menu cursor.
- Menu D-pad repeat uses a 125 ms step delay (upstream's 60 ms is too twitchy
  here), and menu direction input ignores gamefunction-bound buttons, which
  otherwise double-step menu movement.
- The integrated **volume buttons are ignored**. `evtest` shows them on the
  `zadc-keys` evdev device (`/dev/input/event0`) as `KEY_VOLUMEUP` (115) and
  `KEY_VOLUMEDOWN` (114). The direct `agpio-keys` keypad reader skips those
  two key codes, and the SDL keyboard handler drops
  `SDL_SCANCODE_VOLUMEUP`/`VOLUMEDOWN` (and the matching keycodes).
  In game mode the firmware also remaps the buttons to ordinary game inputs
  (volume down -> controller A, volume up -> D-pad down/Down arrow, and on some
  configs other controls). The real 114/115 events are still read passively
  from `zadc-keys`; while one is held, and for 250 ms after, new presses (SDL
  controller/raw buttons, non-number SDL key-downs and large axis pushes) are
  dropped. Releases always pass, so nothing can stick. The number keys
  (SDL `1`-`0` and keypad `1`-`0`) are exempt, since the driver never remaps
  them, and the direct keypad reader is not filtered by the window at all.
  Home, the D-pad and the trackball behave normally otherwise, and
  joystick enumeration and the controller database are unchanged. Set the
  `gsg-debug` file (below) to log `GSGDBG dropped phantom ...` lines.
  On-device behavior must be verified on the hardware.
- The **wireless gamepad's keypad** (digits 0-9, `*`, `#`) is read passively
  from its "LiteStar PC-GamePad" evdev node by the same direct keypad reader
  as the console's built-in keypad (digits become keyboard 1-9, 0; `*`/`#`
  become the stick clicks). SETTINGS/CREDIT remain console-keypad only.

Diagnostics: create `/sdcard/bin/eduke32/gsg-debug` to capture an input/perf
log (`gsg-input.log`) on the next launch; delete the file to disable.

## HDMI output

The package uses a hybrid stack: the bundled SDK SDL2 (2.28.5) provides input
and audio (its controller handling is known-good on this device), while video
is presented by a GameStation-only direct-DRM path in EDuke32 that borrows
SDL's DRM fd, picks the connected external connector itself, renders into GEM
dumb buffers, and page-flips them — bypassing SDL's KMSDRM presentation
entirely. Neither stock SDL2 can present acceptably on this device: the SDK
build misroutes the HDMI CRTC to the panel, and the vendor 2.0.22 build funnels
every frame through a ~130 ms write-combined Mali copy (its window-surface
path also fails with "Failed to set DRM cursor").

At startup, the GameStation build still enumerates and logs SDL displays
(driver, name, mode) and auto-selects an external connector if one is ever
exposed separately; with the vendor SDL2 the selection is display 0 in both
configurations. `EDUKE32_GSG_DISPLAY_INDEX` remains as an explicit override.

The launcher cycles a connected external DRM connector through `off`, `on`,
and `detect`, and routes audio to HDMI (`AUDIODEV=hdmi`, `alsactl restore -f
/sdcard/asound.state`, `ALSA_CARD=0`, `ALSA_DEVICE=1`), matching the vendor
RetroArch launcher. Launch EDuke32 from the GameStation menu so the launcher
UI yields the display. HDMI must be connected before launch; cable changes
while running do not migrate the window — exit and relaunch.

```sh
/sdcard/bin/eduke32/run-eduke32.sh -j "/sdcard/Games/Build Engine/gamedata/duke3d"
```

To choose a display explicitly, set `EDUKE32_GSG_DISPLAY_INDEX`:

```sh
EDUKE32_GSG_DISPLAY_INDEX=1 \
  /sdcard/bin/eduke32/run-eduke32.sh -j "/sdcard/Games/Build Engine/gamedata/duke3d"
```

For a one-launch override, use `--launcher --display <index> --`; use
`--launcher --builtin --` to force the built-in display.
