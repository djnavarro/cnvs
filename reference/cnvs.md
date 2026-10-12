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
  bg = "white",
  standalone = TRUE,
  id_prefix = "cnvs-page"
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

- standalone:

  If `TRUE` (the default), `file` is written as a complete,
  self-contained HTML document – `<html>`/`<head>`/`<body>` and all –
  that can be opened directly in a browser. If `FALSE`, only the
  `<canvas>`/`<script>` block for each page is written, with no
  surrounding document; this is meant for embedding the output as a
  fragment inside a larger HTML page you already have (for instance, a
  knitted Quarto or R Markdown document), not for opening on its own.

- id_prefix:

  Base string used to build each page's `<canvas>` element `id` (page
  *n* gets `id = paste0(id_prefix, "-", n)`). The default,
  `"cnvs-page"`, is fine for a single device writing its own file.
  Supply a distinct value per device when embedding more than one
  `cnvs()` fragment (`standalone = FALSE`) into the same host HTML
  document – e.g. one call per code chunk in a Quarto/R Markdown
  document – so their `<canvas>` ids don't collide.

## Value

Invisibly, `NULL`. Called for the side effect of opening a new graphics
device.

## Examples

``` r
f <- tempfile(fileext = ".html")
cnvs(f)
plot(1:10, main = "cnvs demo")
dev.off()
#> agg_record_1964134d98f0 
#>                       2 
# browseURL(f) # to view the result in a browser
```
