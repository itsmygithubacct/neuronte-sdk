"""A grid of cells drawn by a Face into one pixel buffer."""

from __future__ import annotations

import ctypes
from typing import Iterator

from .face import Face, _c_int, _u32


class Grid:
    """cols by rows cells. The default cell is the Kilix 8 by 16."""

    def __init__(
        self,
        face: Face,
        cols: int,
        rows: int,
        cell_w: int = 8,
        cell_h: int = 16,
        bg: int = 0xFF101010,
    ) -> None:
        if not isinstance(face, Face):
            raise TypeError("face must be a neuronte_sdk.Face")
        self.face = face
        self.cols = _c_int(cols, "cols")
        self.rows = _c_int(rows, "rows")
        self.cell_w = _c_int(cell_w, "cell_w")
        self.cell_h = _c_int(cell_h, "cell_h")
        self.bg = _u32(bg, "bg")
        if self.cols <= 0 or self.rows <= 0 or self.cell_w <= 0 or self.cell_h <= 0:
            raise ValueError("grid dimensions must be positive")
        if self.cols > 2147483647 // self.cell_w or self.rows > 2147483647 // self.cell_h:
            raise ValueError("frame is too large")
        self.width = self.cols * self.cell_w
        self.height = self.rows * self.cell_h
        self._pixels = (ctypes.c_uint32 * (self.width * self.height))()
        for index in range(self.width * self.height):
            self._pixels[index] = self.bg
        self._cells = [(0, self.bg) for _ in range(self.cols * self.rows)]

    def _clear_cell(self, col: int, row: int) -> None:
        origin = row * self.cell_h * self.width + col * self.cell_w
        for y in range(self.cell_h):
            start = origin + y * self.width
            for x in range(self.cell_w):
                self._pixels[start + x] = self.bg

    def _inside(self, col: int, row: int) -> bool:
        return 0 <= col < self.cols and 0 <= row < self.rows

    def put(self, col: int, row: int, scalar: int, color: int) -> None:
        col = _c_int(col, "col")
        row = _c_int(row, "row")
        scalar = _u32(scalar, "scalar")
        color = _u32(color, "color")
        if self._inside(col, row):
            self._clear_cell(col, row)
        self.face._grid_put(
            self._pixels,
            self.width,
            self.width,
            self.height,
            0,
            0,
            self.cell_w,
            self.cell_h,
            col,
            row,
            scalar,
            color,
        )
        if self._inside(col, row):
            self._cells[row * self.cols + col] = (scalar, color)

    def text(self, col: int, row: int, s: str, color: int) -> None:
        if not isinstance(s, str):
            raise TypeError("text must be a str")
        col = _c_int(col, "col")
        row = _c_int(row, "row")
        color = _u32(color, "color")
        written: list[tuple[int, int]] = []
        cursor = col
        for char in s:
            if self._inside(cursor, row):
                self._clear_cell(cursor, row)
                written.append((cursor, ord(char)))
            if cursor == 2147483647:
                break
            cursor += 1
        self.face._grid_text(
            self._pixels,
            self.width,
            self.width,
            self.height,
            0,
            0,
            self.cell_w,
            self.cell_h,
            col,
            row,
            s.encode("utf-8"),
            color,
        )
        for cursor, scalar in written:
            self._cells[row * self.cols + cursor] = (scalar, color)

    def fill(
        self, col: int, row: int, cols: int, rows: int, scalar: int, color: int
    ) -> None:
        col = _c_int(col, "col")
        row = _c_int(row, "row")
        cols = _c_int(cols, "cols")
        rows = _c_int(rows, "rows")
        scalar = _u32(scalar, "scalar")
        color = _u32(color, "color")
        if cols < 0 or rows < 0:
            raise ValueError("fill size must be non-negative")
        painted: list[tuple[int, int]] = []
        for y in range(rows):
            for x in range(cols):
                cc = col + x
                rr = row + y
                if self._inside(cc, rr):
                    self._clear_cell(cc, rr)
                    painted.append((cc, rr))
        self.face._grid_fill(
            self._pixels,
            self.width,
            self.width,
            self.height,
            0,
            0,
            self.cell_w,
            self.cell_h,
            col,
            row,
            cols,
            rows,
            scalar,
            color,
        )
        for cc, rr in painted:
            self._cells[rr * self.cols + cc] = (scalar, color)

    def pixels(self) -> memoryview:
        # ctypes exposes '<I', which memoryview will not index. A byte cast
        # back to I yields one 0xAARRGGBB value per pixel.
        return memoryview(self._pixels).cast("B").cast("I")

    def rgb(self) -> bytes:
        """Width times height times 3 bytes, for kitty-frame-presenter."""
        return self.face._rgb(self._pixels, self.width, self.height)

    def records(self) -> Iterator[dict[str, int | str]]:
        """Yield col, row, scalar, route, net, and color for every cell.

        route and net come from the loaded library, including cells that
        were never written.
        """
        for row in range(self.rows):
            for col in range(self.cols):
                scalar, color = self._cells[row * self.cols + col]
                route = self.face.route(scalar)
                yield {
                    "col": col,
                    "row": row,
                    "scalar": scalar,
                    "route": route,
                    "net": self.face.net_name(route),
                    "color": color,
                }
