# cnvs: an HTML canvas graphics device

`cnvs` implements a small, heavily commented R graphics device that
writes plots out as a self-contained HTML file: a `<canvas>` element
plus a `<script>` block containing the JavaScript
`CanvasRenderingContext2D` calls (`moveTo()`, `lineTo()`, `fillRect()`,
...) that R's graphics engine generated while a plot was drawn.

## Details

This package exists to answer the question "how does an R graphics
device actually work?" by building one from scratch against R's public
graphics device API (see `R_ext/GraphicsDevice.h` and
`R_ext/GraphicsEngine.h` in the R source tree, or
`vignette("grDevices")`-adjacent material). It is not meant to compete
with mature canvas-based devices such as the archived `canvas` package
or `webr::canvas()` – it deliberately keeps things simple (no raster
image support, approximate text metrics, a single rectangular clip
region at a time) so the source stays easy to read end to end.

The package's C source, in `src/`, is organised by responsibility:

- `device.c` – the device lifecycle: creating it, closing it, starting
  new pages. This is the best starting point for understanding how a
  device registers itself with R.

- `draw.c` – the drawing callbacks: lines, rectangles, circles,
  polygons, text, clipping.

- `colour.c` – translating R's packed colour integers, line styles, and
  font descriptions into CSS/JS.

- `write.c` – the HTML/JS text templates everything else writes into.

See [`cnvs()`](reference/cnvs.md) to open a device.

## Author

**Maintainer**: First Last <first.last@example.com>

Authors:

- First Last <first.last@example.com>
