# frzscr - screen freezing program for wayland
## Overview
At the time of writing there's still no sane way to freeze screen on wayland (for screenshots for example) and I saw people use combination of [hyprpicker](https://github.com/hyprwm/hyprpicker) and [slurp](https://github.com/emersion/slurp) which just feels like a crutch so I decided to write this abomination.

## How it works
Simple, it just takes a screenshot and then displays this screenshot over your screen to create an illusion of frozen screen. It uses [wlr layer shell](https://wayland.app/protocols/wlr-layer-shell-unstable-v1) and [wlr screencopy](https://wayland.app/protocols/wlr-screencopy-unstable-v1) so make sure your composter supports them.

## Build
You need meson, wayland-scanner and libwayland-client to compile frzscr
```sh
git clone 'https://github.com/heather7283/frzscr'
cd frzscr
meson setup build
meson compile -C build
```

## Usage
See help for overview of available options:
```sh
build/frzscr -h
```
Launching frzscr without arguments will just freeze your screen until you kill frzscr so don't do that. You can use -t (timeout) option to exit after n seconds or -c option to fork a child and exit when child exits. You can also combine both options, in which case it will kill child and exit when timeout expires.
```sh
# freeze screen for 5 seconds
build/frzscr -t 5
# this is an example on how frzscr can be used for screenshots,
# note double -c flag, first is for frzscr itself, second is for sh
build/frzscr -c sh -c 'grim -g "$(slurp)" - | wl-copy -t "image/png"'
```

## References
- [hello-wayland](https://github.com/wmww/hello-wayland) - layer shell example
- [grim](https://git.sr.ht/~emersion/grim) - screenshot example
- [1](https://gaultier.github.io/blog/wayland_from_scratch.html#shared-memory-the-frame-buffer), [2](https://jan.newmarch.name/Wayland/SharedMemory/) - shm examples
- [wayland explorer](https://wayland.app) - protocol docs

