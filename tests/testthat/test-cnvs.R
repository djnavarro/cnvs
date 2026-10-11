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
  expect_error(cnvs(standalone = "yes"))
  expect_error(cnvs(standalone = NA))
  expect_error(cnvs(id_prefix = 1))
  expect_error(cnvs(id_prefix = NA_character_))
  expect_error(cnvs(id_prefix = ""))
})

test_that("id_prefix controls the <canvas> id, so fragments can be given distinct ids", {
  f1 <- tempfile(fileext = ".html")
  cnvs(f1, width = 300, height = 200, standalone = FALSE, id_prefix = "fig-a")
  plot(1:5)
  dev.off()

  f2 <- tempfile(fileext = ".html")
  cnvs(f2, width = 300, height = 200, standalone = FALSE, id_prefix = "fig-b")
  plot(1:5)
  plot(5:1) # second page should be "fig-b-2"
  dev.off()

  txt1 <- paste(readLines(f1), collapse = "\n")
  txt2 <- paste(readLines(f2), collapse = "\n")
  expect_true(grepl('id="fig-a-1"', txt1, fixed = TRUE))
  expect_true(grepl('getElementById\\("fig-a-1"\\)', txt1))
  expect_true(grepl('id="fig-b-1"', txt2, fixed = TRUE))
  expect_true(grepl('id="fig-b-2"', txt2, fixed = TRUE))
  # the two fragments never share an id, so they could be concatenated
  # into one host document without collisions
  expect_false(grepl("cnvs-page", txt1, fixed = TRUE))
  expect_false(grepl("cnvs-page", txt2, fixed = TRUE))
})

test_that("standalone = FALSE writes only the canvas/script fragment", {
  f <- tempfile(fileext = ".html")
  cnvs(f, width = 300, height = 200, standalone = FALSE)
  plot(1:5)
  dev.off()

  txt <- readLines(f)
  expect_false(any(grepl("<!DOCTYPE html>", txt)))
  expect_false(any(grepl("<head>", txt)))
  expect_false(any(grepl("<body>", txt)))
  expect_false(any(grepl("</html>", txt)))
  # the drawing content itself should be unaffected
  expect_true(any(grepl("<canvas", txt)))
  expect_true(any(grepl("<script>", txt)))
})

test_that("cnvs_embed() returns a knit_asis fragment with no header/footer", {
  skip_if_not_installed("knitr")

  out <- cnvs_embed(plot(1:5, main = "fragment"))

  expect_s3_class(out, "knit_asis")
  expect_true(grepl("<canvas", out))
  expect_true(grepl("<script>", out))
  expect_false(grepl("<!DOCTYPE html>", out))
  expect_false(grepl("<body>", out))
})

test_that("cnvs_embed() gives each call a distinct canvas id", {
  skip_if_not_installed("knitr")

  out1 <- cnvs_embed(plot(1:5))
  out2 <- cnvs_embed(plot(5:1))

  id1 <- regmatches(out1, regexpr('id="[^"]+"', out1))
  id2 <- regmatches(out2, regexpr('id="[^"]+"', out2))
  expect_false(identical(id1, id2))
})

test_that("cnvs_embed() closes the device and removes the temp file even on error", {
  skip_if_not_installed("knitr")

  n_devices_before <- length(dev.list())
  expect_error(cnvs_embed(stop("boom")), "boom")
  expect_equal(length(dev.list()), n_devices_before)
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
