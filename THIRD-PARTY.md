# Third-party code

## soft-raster

`src/neuronte_sdk_canvas.c` is adapted from
[soft-raster](https://github.com/itsmygithubacct/soft-raster): its pixel
buffer, clip, clear, flat rectangle fill, RGBA packing and PPM writer, kept
to the same arithmetic. No other part of soft-raster is here: no fonts, no
lines, no curves, no image scaling. The file carries soft-raster's
copyright lines and MIT notice in full:

```
MIT License

Copyright (c) 2026 soft-raster contributors
Copyright (c) 2026 terminal-lander contributors
```

The tests build against soft-raster only when asked to
(`make canvas-reference SOFT_RASTER=...`), to print the sums they pin.
Nothing of soft-raster is linked into the library.
