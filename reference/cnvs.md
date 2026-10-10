# Open an HTML canvas graphics device

Opens a graphics device that records subsequent plotting commands as
JavaScript `CanvasRenderingContext2D` calls, writing them to a
self-contained HTML file as they happen. Each new page of plotting (for
instance, each top-level
[`graphics::plot()`](https://rdrr.io/r/graphics/plot.default.html) call,
or each panel of a `par(mfrow = ...)` layout) starts a fresh `<canvas>`
element in the file, so a device left open across several plots produces
several canvases stacked down the page. Close the device with
[`grDevices::dev.off()`](https://rdrr.io/r/grDevices/dev.html) to finish
writing the file.

## Usage

``` r
cnvs(
  file = "cnvs.html",
  width = 480,
  height = 480,
  pointsize = 12,
  bg = "white"
)
```

## Arguments

- file:

  Path of the HTML file to create.

- width, height:

  Device size, in pixels (used directly as the `width`/`height`
  attributes of the `<canvas>` element).

- pointsize:

  The default font pointsize, in big points (1/72 inch), following the
  same convention as
  [`grDevices::pdf()`](https://rdrr.io/r/grDevices/pdf.html) and
  friends.

- bg:

  Default background colour, as a colour name or other value accepted by
  [`grDevices::col2rgb()`](https://rdrr.io/r/grDevices/col2rgb.html).

## Value

Invisibly, `NULL`. Called for the side effect of opening a new graphics
device.

## Examples

``` r
f <- tempfile(fileext = ".html")
cnvs(f)
plot(1:10, main = "cnvs demo")
dev.off()
#> agg_record_18f535ffe68 
#>                      2 
# browseURL(f) # to view the result in a browser
```
