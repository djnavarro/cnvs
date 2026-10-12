---
name: add-device-callback
description: Step-by-step recipe for implementing a new cnvs graphics device callback (e.g. raster, path, pattern/group support) or extending an existing one. Use whenever a task touches src/device.c or src/draw.c's callback table.
---

# Adding or changing a device callback

cnvs is a pedagogical package: the goal of any change here is not just
"make it work" but "leave behind something a reader unfamiliar with R's
graphics engine can follow". Read `AGENTS.md`'s "The commenting bar" and
"A load-bearing lesson" sections before starting if you haven't already
-- this skill assumes that context.

## 1. Read the real specification first

Every device callback is documented, with a worked example from a real
device, in R's own headers:

- `R_ext/GraphicsDevice.h` -- the `DevDesc` struct: every field and
  callback signature, with comments describing exactly what the engine
  expects the device to do.
- `R_ext/GraphicsEngine.h` -- `R_GE_gcontext` (the "current graphical
  parameters" struct passed to most callbacks), colour-packing macros
  (`R_RGBA`, `R_RED`, ...), line-type constants (`LTY_DASHED`, ...), and
  the `R_GE_version` history (which version added which feature group).

Find these with, e.g., `R.home("include")` in R, or
`find / -name GraphicsDevice.h 2>/dev/null`. Do not guess a signature or
semantics from memory -- quote/paraphrase what the header says in your
eventual implementation comment instead of inventing an explanation.

## 2. Decide: is this callback actually optional?

The header comments explicitly say `"can be left unimplemented as NULL"`
for some callbacks (`path`, `raster`, `cap`, `locator`, `onExit`,
`getEvent`, `newFrameConfirm`, `eventHelper`, `holdflush`). For those,
leaving them `NULL` is a genuine, documented choice -- just add a
one-line reason to the comment block in `cnvs_setup_callbacks()` (in
`src/device.c`) explaining why cnvs doesn't implement this one, matching
the existing entries there.

For anything **not** on that list -- especially the `R_GE_definitions`
(version 13+) group (`setPattern`, `setClipPath`, `setMask`, and their
`release*` counterparts) -- assume it is called unconditionally by grid
or the engine regardless of whether your device claims to support it,
until you've actually verified otherwise against both base and grid
graphics (step 5). See `AGENTS.md`'s mask/`deviceVersion` story for why
this matters: a `NULL` pointer here doesn't fail gracefully, it
segfaults the whole R session.

If implementing a callback from a versioned feature group (13+), you
must also:
- Implement every *other* callback in that same group (even as a
  declining stub), not just the one you care about.
- Raise `dd->deviceVersion` in `device.c` to match, with a comment
  explaining what's now actually supported versus still declined.
- Not raise `deviceVersion` further than the group you've just finished
  implementing -- leave the next group's callbacks unimplemented and the
  version number reflecting that, until they're done too.

## 3. Where the pieces go

- **Declaration**: add the function prototype to `src/cnvs.h`, next to
  the other declarations from the same file (drawing callbacks under the
  `draw.c` heading, lifecycle/capability callbacks under `device.c`).
- **Implementation**:
  - A callback that draws something on the page (lines, shapes, text,
    images) belongs in `src/draw.c`.
  - A callback about the device's lifecycle, capabilities, or declining
    a feature belongs in `src/device.c`.
  - A new piece of colour/line-style/font translation logic belongs in
    `src/style.c`.
  - A new piece of HTML/JS boilerplate (e.g. a new kind of `<script>`
    block) belongs in `src/write.c`.
- **Wiring**: register the function pointer in `cnvs_setup_callbacks()`
  in `src/device.c`.

## 4. Write the comment before (or alongside) the code

Every new function needs a header comment that a reader without
R-graphics-engine background can follow. At minimum, cover:

- What triggers this callback (a specific R/grid function? every page?
  every clip change?).
- What each non-obvious parameter means, especially anything packed
  (colours, line types) or given in a coordinate system that might
  surprise a reader (device vs. user coordinates, degrees vs. radians).
- *Why* the implementation does what it does, wherever that's not
  obvious from the code alone -- matching the standard already set in
  `draw.c`'s `cnvs_Clip()` and `style.c`'s `cnvs_set_line_style()`.
- Any approximation or limitation, stated plainly (see
  `cnvs_StrWidth()` for the tone to match), rather than silently
  returning a plausible-looking but inexact value.

## 5. Verify against *both* base and grid graphics

Base graphics (`plot()`, `lines()`, `rect()`, ...) and grid graphics
(anything built on `grid`, including ggplot2 and lattice) exercise
different code paths in R's graphics engine -- a change that looks
correct under one can still crash or misbehave under the other (this is
exactly how the `deviceVersion`/mask bug was found). For any
non-trivial callback change:

```r
devtools::load_all()

# base graphics
cnvs(tempfile(fileext = ".html"))
plot(1:10); dev.off()

# grid graphics
cnvs(tempfile(fileext = ".html"))
print(ggplot2::ggplot(ggplot2::mpg, ggplot2::aes(displ, hwy)) + ggplot2::geom_point())
dev.off()
```

Run both in a **fresh `Rscript` process**, not just the interactive
console -- a segfault from a bad callback can otherwise take down a
session you still need. Then add a regression test for whichever paths
you exercised (see `tests/testthat/test-cnvs.R`'s mix of plain and
`skip_if_not_installed("ggplot2")`-gated tests).

## 6. Confirm the rendered output actually looks right

Inspecting the generated JS text for plausibility is not enough --
render it. Headless Chromium works well for this and is already proven
to work in this environment:

```bash
mkdir -p ~/chromedata_tmp
cp /path/to/output.html ~/output.html
chromium-browser --headless --disable-gpu --no-sandbox \
  --user-data-dir=$HOME/chromedata_tmp \
  --screenshot=$HOME/output.png --window-size=WIDTH,HEIGHT \
  "file://$HOME/output.html"
```

Write the HTML and the screenshot under `$HOME`, not `/tmp` or the
scratch directory -- a snap-packaged Chromium has its own private `/tmp`
mount, so files written there by Chromium are invisible to everything
else even though Chromium itself reports writing them successfully.
Clean up `~/output.html`, `~/output.png`, and `~/chromedata_tmp` when
done.

## Checklist before finishing

- [ ] Read the relevant header documentation for this callback; the
      implementation comment reflects what it actually says, not a guess.
- [ ] Decided, and documented inline, whether this callback is safe to
      leave `NULL` -- and if not, implemented every other callback in the
      same version group as at least a declining stub.
- [ ] `dd->deviceVersion` matches what's genuinely implemented, no higher.
- [ ] New/changed functions have a header comment meeting the bar in
      `AGENTS.md` ("The commenting bar").
- [ ] Verified in a fresh `Rscript` process against both a base-graphics
      plot and a grid/ggplot2 plot.
- [ ] Rendered the output (headless Chromium or equivalent) and visually
      confirmed it looks right, not just that the JS text looks plausible.
- [ ] Added a regression test covering the new/changed behaviour.
- [ ] `devtools::check()` is still clean (0 errors/warnings/notes).
