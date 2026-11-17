# neuronte-sdk

`neuronte-sdk` is a cell surface on top of libneuronte. The caller owns a
grid of Unicode scalars and `0xAARRGGBB` colours. The SDK draws each cell
with `neuronte_draw_scalar` into a caller-owned `0xAARRGGBB` buffer, the
same pixel layout as Neuronte and soft-raster. `nsdk_rgb()` packs that
buffer as RGB888 bytes for `kitty_frame_presenter.FramePresenter`.

It does not open a window, take a terminal, read the keyboard, or talk to
a model. It does not include weight files. The default face is
`neuronte-split-v5.nrt` when available, with split-v4 as a fallback. The default cell is 8 by 16, the Kilix cell.

## Build and test

```sh
make
make test
make example
./build/grid
make -C python check
```

`make example` builds `build/grid`. Running it draws `Hello`, a light box,
and a full block into an 8 by 16 cell grid and writes `grid.ppm`. The
program exits 0 when that PPM contains ink.

`make test` and `make -C python check` do not need a terminal. They open
the real weight file when it is visible and skip, with a reason, when the
library or the weight file is absent.

## Weight and library search

`nsdk_open(NULL)` and `neuronte_sdk.open()` use the first weight file that
exists:

1. `NEURONTE_WEIGHTS`, when it is set
2. `$NEURONTE_HOME/weights/neuronte-split-v5.nrt`, then split-v4 in that directory
3. `$NEURONTE_DIR/weights/neuronte-split-v5.nrt`, then split-v4 in that directory
4. The fallback recorded when this SDK was built

`NEURONTE_WEIGHTS`, when set, is the only weight path tried. The library
search is the same shape. An existing but invalid preferred bundle fails to
open; it does not silently select an older bundle.

Library search:

1. `NEURONTE_LIB`, when it is set
2. `$NEURONTE_HOME/build/libneuronte.so`
3. `$NEURONTE_DIR/build/libneuronte.so`
4. The fallback recorded when this SDK was built

The build records a fallback when it can see a `projects/neuronte` checkout
from a parent directory, which is how a build on the GPU host finds
Neuronte. A caller-supplied path does not fall through to that search.
Committed source does not name a machine path.

## C

```c
#include "neuronte_sdk.h"

nsdk *face = NULL;
uint32_t pixels[8 * 16];

if (nsdk_open(NULL, &face) != NEURONTE_OK)
    return 1;
nsdk_grid_text(face, pixels, 8, 8, 16, 0, 0, 8, 16, 0, 0, "A", 1, 0xFF66CCFFu);
nsdk_close(face);
```

Errors use Neuronte's codes (`NEURONTE_OK`, `NEURONTE_ERR_ARG`, and the
rest). A file with six draw nets names indexes 0..5 `ascii`, `draw`,
`symbol`, `accent`, `pixel`, and `braille`. Any other index is `netN`.
The router's gate is not a draw net and is not reported.

`nsdk_grid_text` decodes UTF-8 with `neuronte_utf8_decode`, one cell per
scalar, with no wrap and no wide-character doubling. Coverage blends the
caller's RGB. Binary cut stays off unless `nsdk_set_binary(face, 1)`.

## Canvas

`neuronte_sdk_canvas.h` is a pixel canvas for a program that draws with
Neuronte. It holds what such a program still does in plain code: make and
free the buffer, set a clip, clear the frame, fill a rectangle with one
colour, and hand the pixels on as RGBA bytes or a PPM file. It draws no
other shape.

```c
#include "neuronte_sdk_canvas.h"

nsdk_canvas canvas;

if (!nsdk_canvas_init(&canvas, 640, 400))
    return 1;
nsdk_canvas_clear(&canvas, 0x101820);
nsdk_canvas_fill_rect(&canvas, 16.0f, 16.5f, 200.0f, 48.0f, 0x2a9d8f, 0.9f);
/* canvas.px is 0xAARRGGBB with canvas.w as the stride: what
 * nsdk_draw_scalar() and Neuronte's own draw calls take. */
nsdk_canvas_write_ppm(&canvas, "frame.ppm");
nsdk_canvas_free(&canvas);
```

The canvas needs no weight file and no libneuronte. `src/neuronte_sdk_canvas.c`
includes nothing else of the SDK, so a program may compile that one file
with `-lm` in place of linking the library. The header may be included
beside `neuronte_sdk.h` or `neuronte.h`.

The source is adapted from soft-raster, and gives the same bytes: the
struct has `sr_canvas`'s fields in the same order, and each function keeps
the arithmetic of the soft-raster function named in the header. A program
with a raster build and a Neuronte build gets the same frame from both.
`make test` checks hand-worked results and sums pinned from soft-raster
0.5.0. `make canvas-reference SOFT_RASTER=/path/to/soft-raster` prints
those sums again. The canvas is not in the Python binding.

## Python

The package is `neuronte_sdk`, importable from `python/src` via
`PYTHONPATH`. It is a ctypes binding over this C library, not a second
draw implementation.

```python
from neuronte_sdk import Grid, open

face = open()
grid = Grid(face, cols=8, rows=16)
grid.text(0, 0, "Hello", 0xFFE7E5E4)
frame = grid.rgb()  # width * height * 3, for kitty-frame-presenter
for record in grid.records():
    print(record["col"], record["row"], record["net"])
face.close()
```

`Grid.records()` yields one dict per cell, including cells that were never
written. `route` and `net` come from the loaded library.

On a split-v4 file, `face.nets()` is 6 and `face.route()` follows that
file's router: space, `A`, and `#` are `ascii`; U+2500 and the other light
box and shade scalars covered by the draw net are `draw`; U+2192 is
`symbol`; U+00E9 is `accent`; U+2588 and U+2580 are `pixel`; U+2800 is
`braille`.
