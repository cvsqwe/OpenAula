<p align="center">
  <img src="web/assets/aula-f75-black.png" alt="Aula F75" width="640">
</p>

<h1 align="center">OpenAULA</h1>

<p align="center">
  Open source lighting and macro driver for the Aula F75 on Linux.<br>
  Everything is controlled from a web page, there's no desktop app.
</p>

<p align="center">
  <img alt="platform" src="https://img.shields.io/badge/platform-Linux-informational">
  <img alt="language" src="https://img.shields.io/badge/language-C%2B%2B17-blue">
  <img alt="ui" src="https://img.shields.io/badge/UI-browser-purple">
</p>

## Screenshots

Lighting: live preview of the board, paint tools on top, layers on the right.

<p align="center"><img src="docs/screenshots/lighting.png" alt="Lighting tab" width="800"></p>

Effects are picked per layer from one list.

<p align="center"><img src="docs/screenshots/effects.png" alt="Effect list" width="800"></p>

Remap: remap keys, disable them or bind macros.

<p align="center"><img src="docs/screenshots/macros-remap.png" alt="Remap tab" width="800"></p>

Settings: daemon and calibration.

<p align="center"><img src="docs/screenshots/settings.png" alt="Settings tab" width="800"></p>

## Install

```sh
git clone https://github.com/cvsqwe/OpenAula
cd OpenAula
./install.sh
```

This builds everything and installs `openaula-daemon` and `openaula-webd` as systemd user
services, so they start on login. At the end it prints the address to open, usually
`http://aula.settings/` or `http://localhost:8787/`.

It asks for sudo to install the udev rule and (optionally) to let the web server use
port 80. The services themselves run as your user. You can run it again after a
`git pull`, nothing gets duplicated.

What it does, step by step:

1. Builds with CMake into `build/`.
2. Copies `daemon/60-openaula.rules` to `/etc/udev/rules.d/`. It gives the logged in user
   access to the keyboard (`uaccess`), so nothing has to run as root.
3. Installs and enables `openaula-daemon` (`~/.config/systemd/user/`).
4. Installs and enables `openaula-webd` and copies `web/` to `~/.local/share/openaula/web`.
5. Tries to set up `http://aula.settings/`: `setcap cap_net_bind_service` on the webd
   binary, a `127.0.0.1 aula.settings` line in `/etc/hosts`, port 80. If that fails it
   just stays on 8787.
6. Installs `openaula-remapd` if libevdev was found, but doesn't start it. Turn it on
   in the Remap tab (read the remap section below first).

The parts can also be installed separately with `daemon/install.sh`, `bridge/install.sh`
and `daemon/install-remap.sh`. Note that `bridge/install.sh` copies the binary without
the port 80 capability, so run `./install.sh` again afterwards if you use `aula.settings`.

## Running without installing

```sh
cmake -S . -B build && cmake --build build -j
./build/openaula-daemon &
OPENAULA_WEB_PORT=8787 ./build/openaula-webd &
```

Then open `http://localhost:8787/`. webd finds `web/` next to `build/` on its own, or set
`OPENAULA_WEB_ROOT`.

Settings are stored as plain text in `~/.config/openaula/` (`state.conf`,
`profiles.conf`, `remap.conf`).

## How it works

```
 browser  <--HTTP-->  openaula-webd  <--files + SIGUSR1-->  openaula-daemon  <--hidraw-->  keyboard
  (web/)                (bridge)                             openaula-remapd  <--evdev/uinput--> keyboard (optional)
```

- `core/` - protocol, layout, effects, config files. No dependencies, used by everything else.
- `daemon/` - `openaula-daemon` owns the hidraw connection and drives the backlight.
  `openaula-remapd` (remaps/macros) is built from here too.
- `bridge/` - `openaula-webd`, serves `web/` and a small JSON API. It never opens the
  keyboard, it writes the config files and sends `SIGUSR1` to the daemon.
- `web/` - the UI.

Linux only (hidraw, evdev, uinput).

## Lighting

Lighting is a stack of layers, drawn from the bottom up. Each layer has an effect,
colour, speed, opacity, a set of keys and a blend mode:

- Cover: replaces what's below
- Add: adds light
- Lighten: brighter colour wins
- Tint: multiplies what's below (a white Breath over a Canvas makes the painted keys breathe)

So you can run different effects on different keys, or stack several on the same keys.

Above the keyboard there are two tools:

- Paint: click or drag over keys to colour them, Alt+click picks a key's colour. Paint goes
  on the top Canvas layer (created the first time you paint), Erase takes keys off it.
- Layer keys: choose which keys the selected layer covers, by hand or with presets
  (letters, numbers, F-row, WASD, arrows, ...).

Effects:

| Group | Effects |
|---|---|
| Still | Canvas, Horizon, Blackout |
| Ambient | Breath, Aurora, Starfield, Pulse, Chroma, Prism, Vortex, Ember |
| Motion | Tide, Pendulum, Echo, Rain, Cascade, Meteor, Serpent, Wipe, Checker, Flash, Bloom, Confetti |
| Reactive | Afterglow, Splash, Lock Light |
| System | Processor, Memory, Thermal, Traffic, Clock |

Reactive effects react to key presses. The daemon reads them from the keyboard's input
device without grabbing it, so typing works as usual. System effects use CPU load,
memory, CPU temperature, network traffic and the time. Both work with the page closed.
The preview in the browser reacts to keys typed while the page is focused.

Configs from before layers existed get converted on load.

## Remaps and macros

In the Remap tab every key can be:

- Passthrough (normal, default)
- Disabled
- Remapped to another key
- A macro: a list of press/release steps with a delay after each. Plays once per press,
  holding the key doesn't repeat it.

Edit on a binding loads it back into the form, Save binding overwrites the binding for
the selected key.

Before enabling remaps: `openaula-remapd` grabs the keyboard's input device and sends
every key press through a virtual device (same approach as keyd or
interception-tools). That's how it works in every app, but if it breaks, typing breaks
too. That's why it's a separate process from the lighting daemon and isn't started by
default. It also doesn't grab anything until it's enabled and has at least one binding.

If the keyboard stops typing:

```sh
systemctl --user stop openaula-remapd
# or from another terminal / TTY:
pkill -x openaula-remapd
```

The grab is released as soon as the process exits.

## Known issues

- Reactive effects don't see key presses while remaps are on (remapd has the keyboard
  grabbed).
- Without calibration (Settings) per-key colours can end up on the wrong keys.
- `aula.settings` is just a line in `/etc/hosts`, so it only works on the machine you
  installed on.

## Uninstall

```sh
./uninstall.sh
```

or by hand:

```sh
systemctl --user disable --now openaula-daemon openaula-webd openaula-remapd
rm -f ~/.local/bin/openaula-daemon ~/.local/bin/openaula-webd ~/.local/bin/openaula-remapd
rm -f ~/.config/systemd/user/openaula-{daemon,webd,remapd}.service
rm -rf ~/.local/share/openaula
systemctl --user daemon-reload

sudo rm -f /etc/udev/rules.d/60-openaula.rules
sudo udevadm control --reload-rules

# if you used aula.settings
sudo sed -i '/aula\.settings/d' /etc/hosts
```

`~/.config/openaula/` is kept, delete it yourself if you want (or `./uninstall.sh --purge`).

## Requirements

- Linux, systemd for autostart (not needed to run it by hand)
- CMake 3.16+, C++17 compiler, hidapi (hidraw), pthreads
- libevdev (dev package) for `openaula-remapd`, optional
- a browser
