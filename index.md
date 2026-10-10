# cnvs

cnvs is a small, heavily commented R graphics device, written to learn
how R’s graphics devices actually work. It renders plots as JavaScript
`CanvasRenderingContext2D` calls, embedded in a standalone HTML file –
in the same spirit as the archived `canvas` package or `webr::canvas()`,
but intentionally simple so the C source (in `src/`) can be read end to
end by someone unfamiliar with R’s graphics engine internals.

This is **not** meant to be a polished, general-purpose plotting
backend: there’s no raster image support, text metrics are an
approximation (there’s no portable way to get exact font metrics without
an actual rendering engine), and only one rectangular clip region is
tracked at a time. See the comments in `src/device.c` and `src/draw.c`
for what’s implemented and why.

## Installation

``` r

# install.packages("pak")
pak::pak("djnavarro/cnvs")
```

## Example

``` r

library(cnvs)

f <- tempfile(fileext = ".html")
cnvs(f, width = 480, height = 400)
plot(mpg ~ wt, data = mtcars, col = "steelblue", pch = 19,
     main = "mtcars: mpg vs weight")
abline(lm(mpg ~ wt, data = mtcars), col = "firebrick", lwd = 2)
invisible(dev.off())
```

The result is a standalone HTML file: open it in a browser, and the plot
is drawn live by the browser’s own canvas engine from the JS commands
R’s graphics engine produced. You can inspect the generated commands
directly:

``` r

cat(readLines(f)[8:18], sep = "\n")
#> <canvas id="cnvs-page-1" width="480" height="400" style="border: 1px solid #ccc; margin-bottom: 1em;"></canvas>
#> <script>
#> (function() {
#>   var canvas = document.getElementById("cnvs-page-1");
#>   var ctx = canvas.getContext("2d");
#>   ctx.fillStyle = "rgba(255, 255, 255, 1.000)";
#>   ctx.fillRect(0, 0, 480, 400);
#>   ctx.save();
#>   ctx.beginPath();
#>   ctx.rect(78.72, 78.72, 360.96, 223.36);
#>   ctx.clip();
```

## How it’s organised

A custom graphics device has to be implemented in C, because it works by
registering a table of callback functions (one per drawing primitive)
with R’s graphics engine – there’s no pure-R way to intercept
[`line()`](https://rdrr.io/r/stats/line.html),
[`rect()`](https://rdrr.io/r/graphics/rect.html),
[`text()`](https://rdrr.io/r/graphics/text.html) and friends. The C
source is split up by responsibility:

- `src/device.c` – the device *lifecycle*: building the `DevDesc`
  struct, registering it with the graphics engine, opening/closing
  pages. Start here.
- `src/draw.c` – the drawing callbacks: lines, rectangles, circles,
  polygons, text, clipping.
- `src/colour.c` – translating R’s packed colour integers, line styles,
  and font descriptions into the equivalent CSS/JS.
- `src/write.c` – the HTML/JS text templates everything else writes
  into.

`src/cnvs.h` ties them together and is a reasonable map of the whole
device. R’s own `R_ext/GraphicsDevice.h` and `R_ext/GraphicsEngine.h`
headers (shipped with every R installation) are the authoritative
reference for what every field and callback means.
