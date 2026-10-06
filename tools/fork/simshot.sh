#!/usr/bin/env bash
# Headless screenshots of the Rockbox UI simulator.
#
#   tools/fork/simshot.sh <sim-build-dir> <out-prefix> [steps...]
#
# Steps run in order:
#   shot       save <out-prefix>-N.png (LCD only, 320x240 for ipodvideo)
#   sleepN     wait N seconds
#   <key>      press an X key name. iPod mapping in the sim:
#              Up/Down = wheel, Return = select, Left/Right = prev/next,
#              KP_Decimal = Menu, space = play/pause
#
# Example: tools/fork/simshot.sh build-sim /tmp/ui shot Down Down shot Return sleep1 shot
#
# Needs: Xvfb, xdotool, ImageMagick. The window is cropped to the LCD using
# LCD_X/LCD_Y/LCD_W/LCD_H (defaults match the ipodvideo sim window).
set -u
BUILD=$(cd "$1" && pwd); OUT=$2; shift 2
LCD_X=${LCD_X:-15} LCD_Y=${LCD_Y:-12} LCD_W=${LCD_W:-320} LCD_H=${LCD_H:-240}
DISP=${SIMSHOT_DISPLAY:-:99}
export DISPLAY=$DISP SDL_AUDIODRIVER=dummy

Xvfb "$DISP" -screen 0 1024x768x24 >/dev/null 2>&1 &
XPID=$!
trap 'kill $SPID $XPID 2>/dev/null' EXIT
sleep 1

(cd "$BUILD" && exec ./rockboxui) > "$OUT.sim.log" 2>&1 &
SPID=$!
sleep 4
WID=$(xdotool search --pid "$SPID" 2>/dev/null | tail -1)
[ -n "$WID" ] || { echo "simulator window not found; see $OUT.sim.log" >&2; exit 1; }
xdotool windowactivate --sync "$WID" 2>/dev/null

n=0
for step in "$@"; do
  case "$step" in
    shot)
      n=$((n + 1))
      import -window "$WID" png:- \
        | convert - +repage -crop "${LCD_W}x${LCD_H}+${LCD_X}+${LCD_Y}" +repage "$OUT-$n.png"
      echo "$OUT-$n.png" ;;
    sleep*)
      sleep "${step#sleep}" ;;
    *)
      # real press/release so actions bound to BUTTON_REL fire
      xdotool keydown "$step"; sleep 0.15; xdotool keyup "$step"; sleep 0.8 ;;
  esac
done
