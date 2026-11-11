# neuronte-sdk Python binding

Ctypes binding for the neuronte-sdk C library. Drawing stays in
libneuronte. From a source checkout:

```sh
make -C .. 
PYTHONPATH=src python3 -c "import neuronte_sdk"
make check
```

The loader looks for `../build/libneuronte-sdk.so`. Set
`NEURONTE_SDK_LIBRARY` to use another build of this SDK. `NEURONTE_LIB`
and `NEURONTE_WEIGHTS` still select libneuronte and the weight file, and
are read by the C library.

```python
from neuronte_sdk import Grid, open

with open() as face:
    grid = Grid(face, 8, 16)
    grid.text(1, 1, "Hi", 0xFF66CCFF)
    rgb = grid.rgb()
```
