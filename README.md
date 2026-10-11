# app

A Pebble Alloy project — embedded JavaScript on the watch, powered by Moddable
XS, alongside C.

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
src/c/mdbl.c                   C glue around the Moddable runtime
src/embeddedjs/main.js         JavaScript that runs on the watch
src/embeddedjs/manifest.json   Moddable manifest
src/pkjs/index.js              PebbleKit JS (phone-side) code
package.json                   Project metadata (UUID, platforms, resources)
wscript                        Build rules — usually no need to edit
```

## Documentation

Full SDK docs and tutorials: <https://developer.repebble.com>

## Cloud renderer

`src/embeddedjs/cloud.js` adapts the reference fractal cloud to Poco using
packed pixel masks, dithered shading, and scanline rectangle fills. The complete
cloud is uniformly scaled to fit the requested area with a small margin.

Prepare once, then draw inside an existing Poco `begin()` / `end()` frame:

```js
import { createCloud } from "cloud";

const cloud = createCloud(render.width, render.height, { seed: 42 });
render.begin();
cloud.draw(render);
render.end();
```

`drawCloud(render, options)` prepares and draws in one call. Its optional
`x`, `y`, `width`, and `height` select a rectangular region within the renderer.
`createCloud(width, height, options)` returns a reusable image with `draw(render,
x, y)` and fitted `left`, `top`, `right`, and `bottom` bounds. Transparent pixels
leave the existing background untouched. Neither API starts or ends a frame.

Options include `seed`, `sizeMultiplier`, `floor`, and `rootCenterX` for source
geometry, plus `debug` for lobe outlines. `cloudFill`, `cloudOutline`,
`shadowColor`, and `innerOutline` accept Poco colors from `render.makeColor()`,
not CSS strings. The defaults are white, black, gray, and dark gray.

The cached image uses half a byte per pixel; preparation also allocates two
one-bit masks and temporary lobe geometry. Prepare outside repeated draw events.
Run the renderer tests with `node --test test/cloud.test.mjs` and build with
`pnpm build` (uses the Docker SDK on this host).
