"""A loaded Neuronte face. Draw calls go through libneuronte-sdk."""

from __future__ import annotations

import ctypes
from typing import Any

from ._native import (
    NEURONTE_ERR_ARG,
    FaceClosedError,
    NeuronteError,
    describe,
    sdk_library,
    uint32_pointer,
)

NET_NAMES = ("ascii", "draw", "symbol", "accent", "pixel", "braille")

_INT_MIN = -2147483648
_INT_MAX = 2147483647


def _c_int(value: int, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{label} must be an int")
    if value < _INT_MIN or value > _INT_MAX:
        raise ValueError(f"{label} is outside a C int")
    return value


def _u32(value: int, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{label} must be an int")
    if value < 0 or value > 0xFFFFFFFF:
        raise ValueError(f"{label} is outside uint32")
    return value


class Face:
    """One open weight file and the libneuronte handle that draws it."""

    def __init__(self, weights: str | None = None) -> None:
        if weights is not None and not isinstance(weights, str):
            raise TypeError("weights must be a str or None")
        self._lib = sdk_library()
        self._handle = ctypes.c_void_p()
        encoded = None if weights is None else weights.encode("utf-8")
        rc = self._lib.nsdk_open(encoded, ctypes.byref(self._handle))
        if rc != 0 or not self._handle:
            self._handle = None
            raise NeuronteError(rc, describe(rc))

    def close(self) -> None:
        if self._handle is not None:
            self._lib.nsdk_close(self._handle)
            self._handle = None

    def __enter__(self) -> Face:
        return self

    def __exit__(self, exc_type: object, exc: object, traceback: object) -> None:
        self.close()

    def _alive(self) -> ctypes.c_void_p:
        if self._handle is None:
            raise FaceClosedError()
        return self._handle

    def _check(self, rc: int) -> int:
        if rc < 0:
            raise NeuronteError(rc, describe(rc))
        return rc

    def nets(self) -> int:
        count = self._lib.nsdk_nets(self._alive())
        if count < 0:
            raise NeuronteError(count, describe(count))
        return int(count)

    def route(self, scalar: int) -> int:
        index = self._lib.nsdk_route(self._alive(), _u32(scalar, "scalar"))
        if index < 0:
            raise NeuronteError(index, describe(index))
        return int(index)

    def net_name(self, index: int) -> str:
        buf = ctypes.create_string_buffer(64)
        rc = self._lib.nsdk_net_name(
            self._alive(), _c_int(index, "index"), buf, ctypes.sizeof(buf)
        )
        self._check(rc)
        return buf.value.decode("ascii")

    def draw_scalar(
        self,
        pixels: Any,
        stride: int,
        width: int,
        height: int,
        x: int,
        y: int,
        cell_w: int,
        cell_h: int,
        scalar: int,
        color: int,
    ) -> None:
        pointer, owner = uint32_pointer(pixels)
        try:
            self._check(
                self._lib.nsdk_draw_scalar(
                    self._alive(),
                    pointer,
                    _c_int(stride, "stride"),
                    _c_int(width, "width"),
                    _c_int(height, "height"),
                    _c_int(x, "x"),
                    _c_int(y, "y"),
                    _c_int(cell_w, "cell_w"),
                    _c_int(cell_h, "cell_h"),
                    _u32(scalar, "scalar"),
                    _u32(color, "color"),
                )
            )
        finally:
            del owner

    def _grid_put(
        self,
        pixels: Any,
        stride: int,
        width: int,
        height: int,
        origin_x: int,
        origin_y: int,
        cell_w: int,
        cell_h: int,
        col: int,
        row: int,
        scalar: int,
        color: int,
    ) -> None:
        pointer, owner = uint32_pointer(pixels)
        try:
            self._check(
                self._lib.nsdk_grid_put(
                    self._alive(),
                    pointer,
                    stride,
                    width,
                    height,
                    origin_x,
                    origin_y,
                    cell_w,
                    cell_h,
                    col,
                    row,
                    scalar,
                    color,
                )
            )
        finally:
            del owner

    def _grid_text(
        self,
        pixels: Any,
        stride: int,
        width: int,
        height: int,
        origin_x: int,
        origin_y: int,
        cell_w: int,
        cell_h: int,
        col: int,
        row: int,
        utf8: bytes,
        color: int,
    ) -> int:
        pointer, owner = uint32_pointer(pixels)
        try:
            return self._check(
                self._lib.nsdk_grid_text(
                    self._alive(),
                    pointer,
                    stride,
                    width,
                    height,
                    origin_x,
                    origin_y,
                    cell_w,
                    cell_h,
                    col,
                    row,
                    utf8,
                    len(utf8),
                    color,
                )
            )
        finally:
            del owner

    def _grid_fill(
        self,
        pixels: Any,
        stride: int,
        width: int,
        height: int,
        origin_x: int,
        origin_y: int,
        cell_w: int,
        cell_h: int,
        col: int,
        row: int,
        cols: int,
        rows: int,
        scalar: int,
        color: int,
    ) -> None:
        pointer, owner = uint32_pointer(pixels)
        try:
            self._check(
                self._lib.nsdk_grid_fill(
                    self._alive(),
                    pointer,
                    stride,
                    width,
                    height,
                    origin_x,
                    origin_y,
                    cell_w,
                    cell_h,
                    col,
                    row,
                    cols,
                    rows,
                    scalar,
                    color,
                )
            )
        finally:
            del owner

    def _rgb(self, pixels: Any, width: int, height: int) -> bytes:
        count = width * height * 3
        out = bytearray(count)
        pointer, owner = uint32_pointer(pixels)
        holder = (ctypes.c_uint8 * count).from_buffer(out)
        try:
            self._check(self._lib.nsdk_rgb(pointer, width, height, holder, count))
        finally:
            del owner, holder
        return bytes(out)


def open(weights: str | None = None) -> Face:
    """Open a weight file. None prefers split-v5, falling back to split-v4."""
    if weights is not None and not isinstance(weights, str):
        raise TypeError("weights must be a str or None")
    if weights is not None and weights == "":
        raise NeuronteError(NEURONTE_ERR_ARG, describe(NEURONTE_ERR_ARG))
    return Face(weights)
