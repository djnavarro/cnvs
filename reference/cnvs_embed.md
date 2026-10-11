# Embed a plot as HTML in a knitted Quarto or R Markdown document

A convenience wrapper around
[`cnvs()`](https://cnvs.djnavarro.net/reference/cnvs.md) for embedding
plots directly in a knitted document that already produces HTML output.
Rather than opening the device yourself and remembering to call
[`grDevices::dev.off()`](https://rdrr.io/r/grDevices/dev.html) and
`standalone = FALSE`, `cnvs_embed()` does all of that around a single
plotting expression and hands back an object that `knitr` will drop into
the document verbatim (via
[`knitr::asis_output()`](https://rdrr.io/pkg/knitr/man/asis_output.html))
– no `results: "asis"` chunk option needed.

## Usage

``` r
cnvs_embed(expr, width = 480, height = 480, pointsize = 12, bg = "white")
```

## Arguments

- expr:

  A plotting expression, e.g. `plot(1:10)` or
  `print(ggplot2::ggplot(...))`. Evaluated with the
  [`cnvs()`](https://cnvs.djnavarro.net/reference/cnvs.md) device
  active, so ordinary base or grid plotting calls work as they would in
  any other chunk.

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

An object of class `"knit_asis"` (as produced by
[`knitr::asis_output()`](https://rdrr.io/pkg/knitr/man/asis_output.html))
wrapping the `<canvas>`/`<script>` HTML for the plot. Auto-printing this
at the top level of an R chunk is enough for knitr to insert the raw
HTML into the rendered document.

## Details

Internally, this writes to a temporary file exactly the way
[`cnvs()`](https://cnvs.djnavarro.net/reference/cnvs.md) always has
(there is no in-memory buffering mode – see the discussion of this
tradeoff in the package's design notes); the file is read back in and
deleted once its contents have been captured. Each call gets its own
[`cnvs()`](https://cnvs.djnavarro.net/reference/cnvs.md) `id_prefix`,
derived from the temporary filename R already guarantees is unique, so
calling `cnvs_embed()` from several chunks of the same document never
produces clashing `<canvas>` ids.

## Examples

``` r
if (requireNamespace("knitr", quietly = TRUE)) {
  out <- cnvs_embed(plot(1:10, main = "cnvs_embed() demo"))
  class(out)
}
#> [1] "knit_asis"
```
