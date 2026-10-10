#' Open an HTML canvas graphics device
#'
#' Opens a graphics device that records subsequent plotting commands as
#' JavaScript `CanvasRenderingContext2D` calls, writing them to a
#' self-contained HTML file as they happen. Each new page of plotting
#' (for instance, each top-level [graphics::plot()] call, or each panel of
#' a `par(mfrow = ...)` layout) starts a fresh `<canvas>` element in the
#' file, so a device left open across several plots produces several
#' canvases stacked down the page. Close the device with
#' [grDevices::dev.off()] to finish writing the file.
#'
#' @param file Path of the HTML file to create.
#' @param width,height Device size, in pixels (used directly as the
#'   `width`/`height` attributes of the `<canvas>` element).
#' @param pointsize The default font pointsize, in big points (1/72 inch),
#'   following the same convention as [grDevices::pdf()] and friends.
#' @param bg Default background colour, as a colour name or other value
#'   accepted by [grDevices::col2rgb()].
#'
#' @return Invisibly, `NULL`. Called for the side effect of opening a new
#'   graphics device.
#'
#' @examples
#' f <- tempfile(fileext = ".html")
#' cnvs(f)
#' plot(1:10, main = "cnvs demo")
#' dev.off()
#' # browseURL(f) # to view the result in a browser
#'
#' @export
cnvs <- function(file = "cnvs.html", 
                 width = 480, 
                 height = 480,
                 pointsize = 12, 
                 bg = "white") {
  stopifnot(
    is.character(file), length(file) == 1, !is.na(file),
    is.numeric(width), length(width) == 1, width > 0,
    is.numeric(height), length(height) == 1, height > 0,
    is.numeric(pointsize), length(pointsize) == 1, pointsize > 0
  )

  # The real work happens in C, in src/device.c: this call builds the
  # device description struct and registers it with R's graphics engine.
  # Everything after this point -- every line(), rect(), text() call that
  # happens before dev.off() -- is routed by R straight to our C callbacks,
  # never back through this R function.
  invisible(.Call(
    "cnvs_device_",
    file,
    as.integer(width),
    as.integer(height),
    as.double(pointsize),
    bg
  ))
}
