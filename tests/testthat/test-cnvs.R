test_that("cnvs() writes a well-formed HTML shell even with nothing drawn", {
  f <- tempfile(fileext = ".html")
  cnvs(f, width = 200, height = 150)
  dev.off()

  expect_true(file.exists(f))
  txt <- readLines(f)
  expect_true(any(grepl("<!DOCTYPE html>", txt)))
  expect_true(any(grepl("</html>", txt)))
  # no plotting happened, so newPage was never called -> no <canvas>
  expect_false(any(grepl("<canvas", txt)))
})

test_that("drawing a basic plot produces one canvas with expected primitives", {
  f <- tempfile(fileext = ".html")
  cnvs(f, width = 300, height = 200)
  plot(1:5, (1:5)^2, col = "blue", pch = 19)
  dev.off()

  txt <- paste(readLines(f), collapse = "\n")
  expect_equal(length(gregexpr("<canvas", txt)[[1]]), 1)
  expect_true(grepl("ctx.arc\\(", txt))     # the plotted points (circles)
  expect_true(grepl("ctx.stroke\\(\\)", txt)) # the axis tick marks/box
  expect_true(grepl("rgba\\(0, 0, 255", txt)) # point colour
})

test_that("each top-level plot() call starts a new canvas page", {
  f <- tempfile(fileext = ".html")
  cnvs(f, width = 300, height = 200)
  plot(1:5)
  plot(5:1)
  dev.off()

  txt <- paste(readLines(f), collapse = "\n")
  expect_equal(length(gregexpr("<canvas", txt)[[1]]), 2)
  expect_equal(length(gregexpr("<script>", txt)[[1]]), 2)
})

test_that("filled shapes (e.g. a barplot) emit fill and stroke calls", {
  f <- tempfile(fileext = ".html")
  cnvs(f, width = 300, height = 200)
  barplot(c(a = 3, b = 7, c = 5), col = "tomato")
  dev.off()

  txt <- paste(readLines(f), collapse = "\n")
  expect_true(grepl("ctx.fillRect\\(", txt))
  expect_true(grepl("rgba\\(255, 99, 71", txt)) # "tomato"
})

test_that("multiple cnvs devices can be open and closed independently", {
  f1 <- tempfile(fileext = ".html")
  f2 <- tempfile(fileext = ".html")

  cnvs(f1, width = 100, height = 100)
  d1 <- dev.cur()
  cnvs(f2, width = 100, height = 100)
  d2 <- dev.cur()
  expect_false(d1 == d2)

  dev.off(d2)
  dev.off(d1)

  expect_true(file.exists(f1))
  expect_true(file.exists(f2))
})

test_that("cnvs() validates its arguments", {
  expect_error(cnvs(file = 123))
  expect_error(cnvs(width = -1))
  expect_error(cnvs(height = 0))
  expect_error(cnvs(pointsize = 0))
})

test_that("grid-based plots (e.g. ggplot2) don't crash the device", {
  # Regression test: grid resolves/clears the current mask on every
  # grid.newpage() by calling dd->setMask unconditionally, without a NULL
  # check. Base graphics never exercises this path, so this needs its own
  # coverage -- see the comments in src/device.c next to cnvs_SetMask()
  # and cnvs_setup_callbacks() for the full story.
  skip_if_not_installed("ggplot2")

  f <- tempfile(fileext = ".html")
  cnvs(f, width = 400, height = 300)
  p <- ggplot2::ggplot(ggplot2::mpg, ggplot2::aes(displ, hwy)) +
    ggplot2::geom_point()
  print(p)
  dev.off()

  expect_true(file.exists(f))
  txt <- paste(readLines(f), collapse = "\n")
  expect_true(grepl("ctx.arc\\(", txt))
})
