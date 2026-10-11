#' Embed a plot as HTML in a knitted Quarto or R Markdown document
#'
#' A convenience wrapper around [cnvs()] for embedding plots directly in a
#' knitted document that already produces HTML output. Rather than opening
#' the device yourself and remembering to call [grDevices::dev.off()] and
#' `standalone = FALSE`, `cnvs_embed()` does all of that around a single
#' plotting expression and hands back an object that `knitr` will drop into
#' the document verbatim (via [knitr::asis_output()]) -- no
#' `results: "asis"` chunk option needed.
#'
#' Internally, this writes to a temporary file exactly the way [cnvs()]
#' always has (there is no in-memory buffering mode -- see the discussion
#' of this tradeoff in the package's design notes); the file is read back
#' in and deleted once its contents have been captured. Each call gets its
#' own [cnvs()] `id_prefix`, derived from the temporary filename R already
#' guarantees is unique, so calling `cnvs_embed()` from several chunks of
#' the same document never produces clashing `<canvas>` ids.
#'
#' @param expr A plotting expression, e.g. `plot(1:10)` or
#'   `print(ggplot2::ggplot(...))`. Evaluated with the `cnvs()` device
#'   active, so ordinary base or grid plotting calls work as they would in
#'   any other chunk.
#' @inheritParams cnvs
#'
#' @return An object of class `"knit_asis"` (as produced by
#'   [knitr::asis_output()]) wrapping the `<canvas>`/`<script>` HTML for the
#'   plot. Auto-printing this at the top level of an R chunk is enough for
#'   knitr to insert the raw HTML into the rendered document.
#'
#' @examples
#' if (requireNamespace("knitr", quietly = TRUE)) {
#'   out <- cnvs_embed(plot(1:10, main = "cnvs_embed() demo"))
#'   class(out)
#' }
#'
#' @export
cnvs_embed <- function(expr,
                        width = 480,
                        height = 480,
                        pointsize = 12,
                        bg = "white") {
  if (!requireNamespace("knitr", quietly = TRUE)) {
    stop("cnvs_embed() requires the knitr package; install it with ",
         "install.packages(\"knitr\")", call. = FALSE)
  }

  file <- tempfile(fileext = ".html")

  # tempfile() hands back a name that's already guaranteed unique within
  # this R session (it won't repeat across calls, even to cnvs_embed()
  # itself), so reusing its basename as the id_prefix is enough to keep
  # every embedded plot's <canvas> id distinct once several of them are
  # pasted into one rendered document. The "cnvs-" prefix is only there
  # because HTML/CSS ids shouldn't start with a digit, which a bare
  # tempfile() basename sometimes does.
  id_prefix <- paste0("cnvs-", tools::file_path_sans_ext(basename(file)))

  cnvs(
    file = file,
    width = width,
    height = height,
    pointsize = pointsize,
    bg = bg,
    standalone = FALSE,
    id_prefix = id_prefix
  )
  dev <- grDevices::dev.cur()

  # Belt-and-braces cleanup: if `expr` errors partway through plotting, we
  # still want the (partially drawn) device closed and the temp file gone
  # rather than leaking an open device or a stray file. If everything
  # succeeds, the device is already closed and the file already read by
  # the time this runs, so both checks below are no-ops.
  on.exit({
    if (dev %in% grDevices::dev.list()) grDevices::dev.off(dev)
    unlink(file)
  })

  # `expr` is an unevaluated promise at this point (standard R lazy
  # evaluation of function arguments); forcing it here, with our device
  # already current, is what makes a plain `plot(...)` call passed in by
  # the caller draw onto *this* device rather than whatever was active
  # before.
  force(expr)

  # Must close the device (which flushes and closes the file) before
  # reading it back -- cnvs() only finishes writing valid HTML/JS once
  # dev.off() runs the close callback.
  grDevices::dev.off(dev)
  html <- paste(readLines(file, warn = FALSE), collapse = "\n")

  knitr::asis_output(html)
}
