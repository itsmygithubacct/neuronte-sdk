# neuronte-sdk

`neuronte-sdk` is a cell surface on top of libneuronte. The caller owns a
grid of Unicode scalars and `0xAARRGGBB` colours. The SDK draws each cell
with `neuronte_draw_scalar` into a caller-owned `0xAARRGGBB` buffer, the
same pixel layout as Neuronte and soft-raster. `nsdk_rgb()` packs that
buffer as RGB888 bytes for `kitty_frame_presenter.FramePresenter`.

It does not open a window, take a terminal, read the keyboard, or talk to
a model. It does not include weight files. The default face is
`neuronte-split-v4.nrt`. The default cell is 8 by 16, the Kilix cell.

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
2. `$NEURONTE_HOME/weights/neuronte-split-v4.nrt`
3. `$NEURONTE_DIR/weights/neuronte-split-v4.nrt`
4. The fallback recorded when this SDK was built

`NEURONTE_WEIGHTS`, when set, is the only weight path tried. The library
search is the same shape:

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

## Embedded cell masks

Programs that link Neuronte directly can compile `src/neuronte_sdk_mask.c`
with the SDK and Neuronte include directories. `nsdk_cell_mask()` accepts
the caller's Neuronte handle, style flags, and scratch buffers. It renders
8-bit coverage for one or two terminal cells, so the caller can cache the
mask and composite its own foreground and background colors. This source
uses the caller's linked Neuronte library and requires no dynamic loader.
