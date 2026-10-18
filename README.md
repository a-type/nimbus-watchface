# Nimbus Watchface

A native Pebble C watchface for emery and gabbro.

Press the center Select button to switch between digital and analog time. The
selected display mode is remembered across launches.

## Building & running

```sh
pebble build                          # build for all targetPlatforms
pebble install --emulator emery       # install on the emery emulator
pebble install --phone <ip>           # install to a paired phone
```

## Target platforms

Alloy targets the modern Pebble hardware: **emery** (Pebble Time 2) and
**gabbro** (Pebble Round 2). Other platforms are currently not supported.

## Project layout

```
src/c/main.c                   Native app lifecycle and clock display
src/c/cloud.c                  Fractal geometry and scanline cloud renderer
src/c/cloud.h                  Cloud renderer interface
package.json                   Project metadata (UUID, platforms, resources)
wscript                        Build rules — usually no need to edit
```

## Documentation

Full SDK docs and tutorials: <https://developer.repebble.com>

## Cloud rendering

The cloud geometry is generated once in C and stored in a fixed-size lobe array.
Each frame is rasterized directly into five scanline buffers so a two-pixel
outline can be drawn without allocating a full-screen bitmap or using the XS
heap. Shading remains dithered. Build with `pnpm build` using the Docker SDK on
this host. Run `pnpm test` for JavaScript reference checks and a host-side C
renderer check.
