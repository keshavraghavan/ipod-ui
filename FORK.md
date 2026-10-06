# Rockbox fork: iPod Video (5G/5.5G)

A personal fork of [Rockbox](https://www.rockbox.org) targeting the iPod Video, with a custom
home screen added to the core firmware.

## What's changed from upstream

| File | Change |
|---|---|
| `apps/gui/home_screen.c/.h` | New hand-drawn home screen: header (clock, battery), launcher list with highlight and chevrons, and a "now playing" strip with a progress rail when audio is playing. Draws straight through the screen API, bypassing the skin engine. |
| `apps/root_menu.h` | `GO_TO_HOME` screen ID (added at the end of the enum so saved settings stay valid). |
| `apps/root_menu.c` | Registers the screen, adds a "Home" entry to the stock root list, and makes Home the real root: anything that would return to the root list lands on Home instead. Menu, or "Rockbox Menu" on Home, opens the stock list. |
| `apps/menu.c` | Back/Left on the stock root list returns to Home. |
| `apps/SOURCES` | Compiles `home_screen.c` on colour-LCD targets. |

Everything is behind `#ifdef HAVE_LCD_COLOR`, so mono targets still build unchanged.

## Navigation

```
boot → Home ──Select──▶ Music / Files / Playlists / Now Playing / Settings
        ▲  │                         │ (back / Menu from WPS)
        │  └─Menu / "Rockbox Menu"──▶ stock root list ──Left──┐
        └─────────────────────────────────────────────────────┘
```

## Where the UI lives

| You want to change… | Look in |
|---|---|
| What screen boots, how screens hand off | `apps/root_menu.c` (`root_menu()`, `items[]`, `menu_table[]`) |
| Settings menus, their structure | `apps/menus/*.c` (macros in `apps/menu.h`) |
| List drawing (every scrolling list) | `apps/gui/list.c`, `apps/gui/bitmap/list.c` |
| Now Playing (WPS) behaviour | `apps/gui/wps.c` |
| Skin engine (themes, `.wps/.sbs` parsing and rendering) | `apps/gui/skin_engine/` |
| Button → action mapping for the iPod | `apps/keymaps/keymap-ipod.c` |
| Hardware config (LCD 320×240 RGB565, features) | `firmware/export/config/ipodvideo.h` |
| Default theme | `wps/cabbiev2.*` |

## Build: simulator (iterate here)

```sh
sudo apt-get install build-essential libsdl2-dev   # Linux; macOS: brew install sdl2
mkdir build-sim && cd build-sim
../tools/configure --target=ipodvideo --ram=64 --type=s
make -j"$(nproc)" && make install     # installs into ./simdisk
./rockboxui                           # arrows = wheel, Enter = select, Esc = Menu
```

Put music in `build-sim/simdisk/`. For headless screenshots (CI or SSH):
`tools/fork/simshot.sh build-sim /tmp/ui shot Down Down shot Return sleep1 shot`

## Build: real firmware

Use the official toolchain (gcc 9.5.0 with Rockbox patches) for anything you flash:

```sh
tools/rockboxdev.sh --target=a --prefix="$HOME/rbdev"   # one-time, ~20-30 min
export PATH="$HOME/rbdev/bin:$PATH"
mkdir build-ipodvideo && cd build-ipodvideo
../tools/configure --target=ipodvideo --ram=64 --type=n
make -j"$(nproc)" && make zip        # → rockbox.zip
```

`.github/workflows/build-ipodvideo.yml` does the same on every push and uploads `rockbox.zip`
as an artifact, caching the toolchain after the first run.

## Install on the iPod

1. Make sure the iPod is **FAT32** ("Windows-formatted"). If it was set up on a Mac, restore it
   in Windows format first.
2. Install stock Rockbox once with **Rockbox Utility**. This installs the bootloader.
3. Unzip your `rockbox.zip` into the iPod's root, overwriting files in `/.rockbox`. The zip
   doesn't contain `config.cfg` or the database, so your settings and library are kept.
4. Eject and reboot. To boot Apple's firmware, hold **Menu** during the boot.

## Keeping up with upstream

```sh
git remote add upstream https://github.com/Rockbox/rockbox.git
git fetch upstream && git rebase upstream/master
```

Rockbox is GPLv2+; this fork is too.
