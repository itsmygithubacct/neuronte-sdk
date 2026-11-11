"""Load libneuronte-sdk and declare the C API. Drawing stays in that library."""

from __future__ import annotations

import ctypes
import ctypes.util
import os
from pathlib import Path

NEURONTE_OK = 0
NEURONTE_ERR_ARG = -1
NEURONTE_ERR_FORMAT = -2
NEURONTE_ERR_SHAPE = -3
NEURONTE_ERR_NOMEM = -4
NEURONTE_ERR_IO = -5

_MESSAGES = {
    NEURONTE_ERR_ARG: "invalid argument",
    NEURONTE_ERR_FORMAT: "weight file is not a Neuronte file, or is damaged",
    NEURONTE_ERR_SHAPE: "weight file is for a different net shape",
    NEURONTE_ERR_NOMEM: "out of memory",
    NEURONTE_ERR_IO: (
        "could not open libneuronte or terminal bundle (split-v5/v4); "
        "set NEURONTE_LIB and NEURONTE_WEIGHTS, or NEURONTE_HOME or NEURONTE_DIR"
    ),
}


class NeuronteError(Exception):
    """A Neuronte status code other than NEURONTE_OK."""

    def __init__(self, code: int, message: str | None = None) -> None:
        self.code = code
        super().__init__(message or _MESSAGES.get(code, f"neuronte error {code}"))


class LibraryNotFoundError(NeuronteError):
    """libneuronte-sdk could not be loaded."""

    def __init__(self, message: str) -> None:
        super().__init__(NEURONTE_ERR_IO, message)


class IncompatibleLibraryError(NeuronteError):
    """The loaded SDK library does not export the cell-surface API."""

    def __init__(self, message: str) -> None:
        super().__init__(NEURONTE_ERR_IO, message)


class FaceClosedError(NeuronteError):
    """A method was called on a face that has been closed."""

    def __init__(self, message: str = "face is closed") -> None:
        super().__init__(NEURONTE_ERR_ARG, message)


def describe(code: int) -> str:
    return _MESSAGES.get(code, f"neuronte error {code}")


def _dedupe(values: list[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for value in values:
        if not value or value in seen:
            continue
        seen.add(value)
        result.append(value)
    return result


def _candidates() -> list[str]:
    forced = os.environ.get("NEURONTE_SDK_LIBRARY", "")
    if forced:
        return [forced]
    package = Path(__file__).resolve().parent
    root = package.parents[2]
    names = ("libneuronte-sdk.so",)
    found: list[str] = [str(root / "build" / name) for name in names]
    located = ctypes.util.find_library("neuronte-sdk")
    if located:
        found.append(located)
    found.extend(names)
    return _dedupe(found)


def _bind(library: ctypes.CDLL, name: str, argtypes: list[object], restype: object) -> None:
    try:
        function = getattr(library, name)
    except AttributeError as exc:
        raise IncompatibleLibraryError(
            f"libneuronte-sdk does not export {name}"
        ) from exc
    function.argtypes = argtypes
    function.restype = restype


def _declare(library: ctypes.CDLL) -> None:
    integer = ctypes.c_int
    u32 = ctypes.c_uint32
    handle = ctypes.c_void_p
    pixels = ctypes.POINTER(u32)
    _bind(library, "nsdk_open", [ctypes.c_char_p, ctypes.POINTER(handle)], integer)
    _bind(library, "nsdk_close", [handle], None)
    _bind(library, "nsdk_nets", [handle], integer)
    _bind(library, "nsdk_route", [handle, u32], integer)
    _bind(
        library,
        "nsdk_net_name",
        [handle, integer, ctypes.POINTER(ctypes.c_char), ctypes.c_size_t],
        integer,
    )
    _bind(library, "nsdk_set_binary", [handle, integer], integer)
    draw = [
        handle,
        pixels,
        integer,
        integer,
        integer,
        integer,
        integer,
        integer,
        integer,
        u32,
        u32,
    ]
    _bind(library, "nsdk_draw_scalar", draw, integer)
    _bind(
        library,
        "nsdk_grid_put",
        draw[:9] + [integer, integer, u32, u32],
        integer,
    )
    _bind(
        library,
        "nsdk_grid_text",
        draw[:9] + [integer, integer, ctypes.c_char_p, ctypes.c_size_t, u32],
        integer,
    )
    _bind(
        library,
        "nsdk_grid_fill",
        draw[:9] + [integer, integer, integer, integer, u32, u32],
        integer,
    )
    _bind(
        library,
        "nsdk_rgb",
        [pixels, integer, integer, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t],
        integer,
    )


def load_library() -> ctypes.CDLL:
    errors: list[str] = []
    for candidate in _candidates():
        try:
            library = ctypes.CDLL(candidate)
        except OSError as exc:
            errors.append(f"{candidate}: {exc}")
            continue
        _declare(library)
        return library
    detail = "; ".join(errors) if errors else "no candidates"
    raise LibraryNotFoundError(
        "could not load libneuronte-sdk "
        f"({detail}). Build it with make, or set NEURONTE_SDK_LIBRARY."
    )


_LIBRARY: ctypes.CDLL | None = None


def sdk_library() -> ctypes.CDLL:
    global _LIBRARY
    if _LIBRARY is None:
        _LIBRARY = load_library()
    return _LIBRARY


def uint32_pointer(pixels: object) -> tuple[ctypes._Pointer, object]:
    """Return a uint32 pointer and an owner that must outlive the C call."""
    if isinstance(pixels, ctypes.Array):
        return ctypes.cast(pixels, ctypes.POINTER(ctypes.c_uint32)), pixels
    try:
        view = memoryview(pixels)
    except TypeError as exc:
        raise TypeError("pixels must be a contiguous uint32 buffer") from exc
    if view.ndim != 1 or not view.c_contiguous or view.readonly:
        raise TypeError("pixels must be a writable contiguous uint32 buffer")
    if view.format == "B":
        if len(view) % 4 != 0:
            raise ValueError("pixel buffer length is not a multiple of 4")
    elif view.itemsize != 4 or view.format not in {"I", "L"}:
        raise TypeError("pixels must be a uint32 buffer")
    holder = (ctypes.c_uint8 * view.nbytes).from_buffer(view)
    pointer = ctypes.cast(holder, ctypes.POINTER(ctypes.c_uint32))
    return pointer, (view, holder)
