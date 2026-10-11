# AGENTS.md

## What this package is

cnvs is a small R graphics device, written for **learning purposes**: it
renders base/grid graphics as JavaScript `CanvasRenderingContext2D`
calls embedded in a standalone HTML file. The point is to have a
from-scratch, heavily commented implementation that someone unfamiliar
with R’s graphics engine internals can read end to end alongside R’s own
`R_ext/GraphicsDevice.h` and `R_ext/GraphicsEngine.h`.

This shapes priorities differently than a normal package:

- **This is not, and will not be submitted to, CRAN.** There is no
  `cran-comments.md`, no `CRAN-SUBMISSION`, no need to track CRAN policy
  compliance beyond what `R CMD check` already enforces for free.
- **This is not trying to be feature-complete or production quality.**
  See the archived `canvas` package or `webr::canvas()` for a “real”
  canvas device. cnvs deliberately leaves things unimplemented (raster
  images, arbitrary clip paths, exact font metrics – see below) rather
  than chasing parity with them.
- **Clarity of comments is the primary code-quality bar** – above
  brevity, above performance, above covering every edge case. A change
  that makes the implementation faster or shorter but harder to follow
  for a reader who doesn’t already know how graphics devices work is a
  net loss for this package, even if it would be a clear win elsewhere.
  See “The commenting bar” below.

## Repository layout

    R/
      cnvs.R            # the user-facing cnvs() device constructor
      cnvs-package.R    # package-level doc, @useDynLib tag

    src/
      cnvs.h            # shared struct + declarations; a map of the whole device
      device.c          # device lifecycle: create, close, newPage, size,
                         # registering callbacks with the graphics engine --
                         # start here to understand how a device plugs into R
      draw.c            # drawing callbacks: line, rect, circle, polyline,
                         # polygon, text, clip, strWidth, metricInfo
      colour.c          # R colour ints / line styles / fonts -> CSS/JS text
      write.c           # the HTML/JS text templates everything else writes into
      init.c            # native routine registration (R_init_cnvs)

    tests/testthat/     # testthat edition 3
    tmp/                # scratch scripts (gitignored) -- not part of the package

No vignettes, no `NEWS.md`, no pkgdown-only `vignettes/articles/` split
– the package is small enough that the README and the source comments
are the documentation.

## Architecture, in brief

- One `cnvs(file, width, height, pointsize, bg)` call opens one `FILE*`
  and writes a self-contained `.html` file as drawing happens, rather
  than buffering in memory. Each device *page* (each call to the
  `newPage` callback – i.e. each top-level
  [`plot()`](https://rdrr.io/r/graphics/plot.default.html)/[`print()`](https://rdrr.io/r/base/print.html)
  of a grid object, not each panel of a `par(mfrow = ...)` layout) gets
  its own `<canvas>` + `<script>` block.
- The device coordinate system is set up the same way a bitmap/screen
  device would be (origin top-left, y increasing downward, one device
  unit = one CSS pixel), so coordinates arriving in callbacks can be
  written straight into canvas calls with no flipping.
- Deliberately unimplemented, with the reasoning given inline next to
  `dd->path = NULL;` etc. in `src/device.c`: `path`, `raster`, `cap`,
  `locator`. Text metrics (`strWidth`/`metricInfo`) are a documented
  heuristic (average glyph width as a fraction of font size), not a
  measurement, because there is no portable way to get real font metrics
  without an actual rendering engine.
- Only one rectangular clip region is tracked at a time (`clip_active`
  in `cnvs_context`); a new
  [`clip()`](https://rdrr.io/r/graphics/clip.html) call always restores
  any previous clip before applying the next one.

## A load-bearing lesson: `deviceVersion` and the “definitions” callbacks

This bit a real debugging session and is easy to reintroduce, so it gets
its own section rather than just a code comment.

`dd->deviceVersion` tells the graphics engine which bundle of *optional*
feature groups (see the version history comment at the top of
`R_ext/GraphicsEngine.h`) the device’s callback table actually has slots
for. The group added at version 13 (`R_GE_definitions`) –
`setPattern`/`releasePattern`/`setClipPath`/`releaseClipPath`/`setMask`/
`releaseMask` – is **not optional in practice** once a device claims
that version or higher: grid calls `dd->setMask` unconditionally on
every `grid.newpage()` (to clear any active mask), with no `NULL` check
first. A device that claims `deviceVersion = R_GE_version` (the newest
level) while leaving those six callbacks `NULL` will segfault R the
first time *grid*-based graphics (ggplot2, lattice) draws a page – while
looking completely fine under base graphics
([`plot()`](https://rdrr.io/r/graphics/plot.default.html), etc.), since
base graphics never triggers that code path. This is exactly what
happened here (see the git history around the `deviceVersion` field in
`src/device.c`).

The fix, and the pattern to keep following: `dd->deviceVersion` must
match what’s genuinely wired up in `cnvs_setup_callbacks()`, not the
newest API version the headers define. Currently that’s
`R_GE_definitions` (13): the six callbacks above are implemented as
explicit, commented stub functions that decline support (return
`R_NilValue` / do nothing), and `deviceVersion` is set to 13 to match.
**Never raise `deviceVersion` without also implementing every callback
in every version group up to and including the new value** – even if the
implementation is just “decline this feature”. Before considering any
device change complete, exercise it through **both** a base-graphics
plot and a grid-based one (e.g. a minimal
`ggplot2::ggplot(...) + geom_point()`) – see the `add-device-callback`
skill for the full checklist.

## The commenting bar

Every callback and helper in `src/` should be readable by someone who
knows C and knows *what a line chart is*, but has never seen
`R_ext/GraphicsDevice.h` before. Concretely, that means:

- Say **why**, not just what. `ctx->clip_active` being restored before a
  new clip is applied is obvious from the code; *why* (canvas clip
  regions can only ever be intersected, never grown, without a restore)
  is the part worth a comment.
- When a field, constant, or calling convention comes from R’s graphics
  engine rather than from this package’s own design, say so and point at
  where it’s documented (`R_ext/GraphicsDevice.h`,
  `R_ext/GraphicsEngine.h`, or a specific macro name) rather than
  asserting it as if it were an arbitrary implementation choice.
- Flag approximations and limitations where they live, not just in the
  README – e.g. `cnvs_StrWidth()`’s heuristic explains in its own
  comment why an exact answer isn’t possible here, rather than silently
  returning a number that looks authoritative.
- New top-level functions get a short header comment explaining what
  triggers the call and what it’s responsible for, matching the style
  already in `draw.c`/`device.c`/`colour.c`/`write.c`.

This bar applies to C and R code alike. It’s fine – expected, even – for
comments here to be longer relative to the code than they would be in a
normal package.

## Skills

When adding a new device callback (implementing `raster`, `path`,
groups, or anything else currently left as a stub or `NULL`), load the
**`add-device-callback`** skill first. It has a step-by-step recipe
covering where to add the declaration/implementation/wiring, the
`deviceVersion` trap above, and how to verify a change against both base
and grid graphics before calling it done.

## Development workflow

- Document with roxygen2 (`devtools::document()`); markdown roxygen is
  enabled.
- Run tests with `devtools::test()`; full checks with
  `devtools::check()`. The package should check cleanly (0
  errors/warnings/notes) even though it isn’t CRAN-bound – a clean check
  is still the cheapest available signal that nothing is obviously
  broken.
- Rebuilding compiled code: `devtools::load_all()` recompiles
  automatically; use `pkgbuild::compile_dll(force = TRUE)` if a change
  doesn’t seem to be picked up.
- Visual verification matters more than usual for this package, since
  the whole point is the rendered output: after a drawing-related
  change, actually render the generated `.html` (e.g. headless Chromium
  –
  `chromium-browser --headless --disable-gpu --no-sandbox --screenshot=out.png file://...`
  – writing the screenshot under `$HOME` rather than `/tmp`, since a
  snap-packaged Chromium’s `/tmp` is a private mount and writes there
  won’t show up outside it) rather than just inspecting the generated JS
  text.
- `tests/testthat/test-cnvs.R` mixes base-graphics and
  `skip_if_not_installed("ggplot2")`-gated grid-graphics tests
  deliberately – keep both kinds when adding coverage for a new
  callback, per the lesson above.

## Conventions

- UK spelling throughout code comments, roxygen documentation, and prose
  (`colour`, `organised`, `behaviour`) – matches `Language: en-GB` in
  `DESCRIPTION` and the existing source.
- Use the base R pipe (`|>`), not the magrittr pipe, in any R code.
- C code targets plain C against R’s public headers (no Rcpp/cpp11) –
  intentional, since hiding the callback-table mechanics behind another
  layer of abstraction would undercut the point of the package.
- Internal C helpers are prefixed `cnvs_` (matching the one exported R
  function, [`cnvs()`](https://cnvs.djnavarro.net/reference/cnvs.md)),
  `UpperCamelCase` after the prefix for graphics engine callbacks
  (`cnvs_Line`, `cnvs_NewPage`) to mirror the `Device_Foo` naming
  convention used in R’s own device sources, and `lower_snake_case` for
  ordinary helpers (`cnvs_colour_to_css`, `cnvs_set_line_style`).
