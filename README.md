### build:
```
git clone https://github.com/heather7283/frzscr
cd frzscr
meson setup build
meson compile -C build
```

### usage:
```shell
build/frzscr -h
build/frzscr -c sh -c 'grim -g "$(slurp)" - | wl-copy -t "image/png"'
```
