# Third-party code

## soft-raster

`src/neuronte_sdk_canvas.c` is adapted from
[soft-raster](https://github.com/itsmygithubacct/soft-raster): its pixel
buffer, clip, clear, flat rectangle fill, RGBA packing and PPM writer, kept
to the same arithmetic. No other part of soft-raster is here: no fonts, no
lines, no curves, no image scaling. The file carries soft-raster's
copyright lines and MIT notice in full, and `make install` puts this
document beside the libraries, which hold that code. The notice:

```
MIT License

Copyright (c) 2026 soft-raster contributors
Copyright (c) 2026 terminal-lander contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The tests build against soft-raster only when asked to
(`make canvas-reference SOFT_RASTER=...`), to print the sums they pin.
Nothing of soft-raster is linked into the library.
