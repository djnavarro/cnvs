/* device.c -- the cnvs device's lifecycle
 *
 * This is the file that actually plugs cnvs into R's graphics engine. It
 * has two jobs:
 *
 *   1. cnvs_device_() is the entry point called from R (via .Call()). It
 *      builds a `DevDesc` -- a big struct of settings and function
 *      pointers that describes a device to the graphics engine -- fills it
 *      in, hands it to GEcreateDevDesc()/GEaddDevice2(), and registers it
 *      as the active device. Every graphics device package has a function
 *      that looks roughly like this one.
 *
 *   2. The lifecycle callbacks (activate/deactivate/close/newPage/size/
 *      mode) handle events in the device's life other than "draw this
 *      shape" -- those live in draw.c instead.
 *
 * Useful background reading, in the R source tree: `R_ext/GraphicsDevice.h`
 * documents every field and callback in detail with worked examples from
 * real devices; `R_ext/GraphicsEngine.h` documents R_GE_gcontext (the
 * struct of "current graphical parameters" passed to most callbacks) and
 * the various helper macros/constants (R_RGBA, LTY_DASHED, ...).
 */
#include "cnvs.h"
#include <stdlib.h>
#include <string.h>

/* A handful of constants describing character-size bookkeeping that the
 * graphics engine needs from every device (used e.g. when laying out
 * margins and axis labels). These particular numbers are the same ones
 * R's own screen devices use -- there's nothing canvas-specific about
 * them, they're just reasonable defaults for how much visual padding to
 * leave around a character. */
#define CNVS_XCHAR_OFFSET 0.4900
#define CNVS_YCHAR_OFFSET 0.3333
#define CNVS_YLINE_BIAS   0.2000

/* ---------------------------------------------------------------------
 * Lifecycle callbacks
 * --------------------------------------------------------------------- */

void cnvs_Activate(const pDevDesc dd)
{
    /* Called when this device becomes the active one (e.g. after
     * dev.set()). A windowed device might use this to highlight its
     * title bar; we have no window, so there's nothing to do. */
}

void cnvs_Deactivate(pDevDesc dd)
{
    /* The mirror image of Activate(), called when this device stops being
     * the active one. Again, nothing for a file-based device to do. */
}

void cnvs_Mode(int mode, pDevDesc dd)
{
    /* The engine calls this with mode = 1 when it starts a burst of
     * drawing and mode = 0 when it finishes. A screen device uses this to
     * know when to flush pixels to the window; since we write everything
     * straight to the output file as it happens, there's no buffering to
     * flush. */
}

void cnvs_NewPage(const pGEcontext gc, pDevDesc dd)
{
    cnvs_state *state = CNVS(dd);

    /* newPage is called once before the very first thing is drawn, and
     * again every time plotting starts a fresh page (e.g. a second
     * plot() call, or the next panel in par(mfrow = ...)). Each page gets
     * its own <canvas>/<script> block, so if a previous page is open we
     * need to close it out first. */
    if (state->page_count > 0) {
        if (state->clip_active) {
            fprintf(state->fp, "  ctx.restore();\n");
            state->clip_active = FALSE;
        }
        cnvs_end_page(state);
    }
    cnvs_start_page(state, gc);
}

void cnvs_Close(pDevDesc dd)
{
    cnvs_state *state = CNVS(dd);

    if (state->page_count > 0) {
        if (state->clip_active) {
            fprintf(state->fp, "  ctx.restore();\n");
        }
        cnvs_end_page(state);
    }
    cnvs_write_footer(state);
    fclose(state->fp);
    free(state->id_prefix);
    free(state);
    /* `dd` itself (the DevDesc struct) is freed by the graphics engine
     * right after this callback returns -- we only own `state`. */
}

void cnvs_Size(double *left, double *right, double *bottom, double *top, pDevDesc dd)
{
    /* Called if the engine ever needs to ask the device for its current
     * extent (we never resize ourselves, so this always returns the same
     * fixed values that were set up in cnvs_device_()). */
    cnvs_state *state = CNVS(dd);
    *left = 0;
    *right = state->width;
    *top = 0;
    *bottom = state->height;
}

/* ---------------------------------------------------------------------
 * Stubs for the "definitions" callback group (patterns, clipping paths,
 * masks; added in R_GE_version 13 -- see R_ext/GraphicsEngine.h's version
 * history comment).
 *
 * Unlike path/raster/cap/locator (which the header explicitly documents
 * as "can be left unimplemented as NULL"), these ones are not optional in
 * practice: grid resolves/clears the current mask on every
 * grid.newpage() and viewport push by calling straight through
 * dd->setMask, with no NULL check first. Any device whose compiled
 * struct has this field at all (i.e. built against R >= 4.1 headers, like
 * ours) needs a real function pointer here, even if all it does is
 * "nothing, successfully" -- this is the same "template for no support"
 * approach R core documents for updating existing devices to a new
 * R_GE_version without adding real feature support. We don't support
 * gradients/patterns/clipping-paths/masks, so every stub just declines
 * (returns NULL / does nothing) and our drawing callbacks never see a
 * pattern fill or a clip path, only ordinary colours and rectangular
 * clips.
 * --------------------------------------------------------------------- */

SEXP cnvs_SetPattern(SEXP pattern, pDevDesc dd)
{
    return R_NilValue; /* "I don't support patterns" */
}

void cnvs_ReleasePattern(SEXP ref, pDevDesc dd)
{
    /* nothing to release, since we never created one */
}

SEXP cnvs_SetClipPath(SEXP path, SEXP ref, pDevDesc dd)
{
    return R_NilValue; /* "I don't support non-rectangular clip paths" */
}

void cnvs_ReleaseClipPath(SEXP ref, pDevDesc dd)
{
}

SEXP cnvs_SetMask(SEXP mask, SEXP ref, pDevDesc dd)
{
    return R_NilValue; /* "I don't support masks" */
}

void cnvs_ReleaseMask(SEXP ref, pDevDesc dd)
{
}

/* ---------------------------------------------------------------------
 * Wiring up the callback table
 * --------------------------------------------------------------------- */

static void cnvs_setup_callbacks(pDevDesc dd)
{
    /* Callback functions defined in device.c*/
    dd->activate   = cnvs_Activate;
    dd->deactivate = cnvs_Deactivate;
    dd->close      = cnvs_Close;
    dd->newPage    = cnvs_NewPage;
    dd->size       = cnvs_Size;
    dd->mode       = cnvs_Mode;

    /* Callback functions defined in draw.c*/
    dd->line       = cnvs_Line;
    dd->rect       = cnvs_Rect;
    dd->circle     = cnvs_Circle;
    dd->polyline   = cnvs_Polyline;
    dd->polygon    = cnvs_Polygon;
    dd->text       = cnvs_Text;
    dd->clip       = cnvs_Clip;
    dd->strWidth   = cnvs_StrWidth;
    dd->metricInfo = cnvs_MetricInfo;

    /* Everything below is left NULL on purpose, which tells the graphics
     * engine "this device can't do that" -- it will fall back to other
     * means where it can (e.g. approximating a raster image with
     * polygons), or simply skip the feature. Implementing these is a
     * natural set of follow-up exercises:
     *   - path:    arbitrary multi-polygon paths with winding-rule fills
     *   - raster:  bitmap images (would need base64/ImageData encoding)
     *   - cap:     reading pixels back off the device (needs a real
     *              canvas, i.e. a browser -- not possible from a plain
     *              HTML file written from R)
     *   - locator: interactive mouse-click input (not possible for a
     *              file-based, non-interactive device)
     */
    dd->path            = NULL;
    dd->raster          = NULL;
    dd->cap             = NULL;
    dd->locator         = NULL;
    dd->onExit          = NULL;
    dd->getEvent        = NULL;
    dd->newFrameConfirm = NULL;
    dd->eventHelper     = NULL;
    dd->holdflush       = NULL;

    /* These, by contrast, are NOT safe to leave NULL -- see the long
     * comment above cnvs_SetPattern() et al. They're genuine "declines"
     * (we don't support any of this), just expressed as real function
     * calls rather than a missing pointer. Also in device.c */
    dd->setPattern      = cnvs_SetPattern;
    dd->releasePattern  = cnvs_ReleasePattern;
    dd->setClipPath     = cnvs_SetClipPath;
    dd->releaseClipPath = cnvs_ReleaseClipPath;
    dd->setMask         = cnvs_SetMask;
    dd->releaseMask     = cnvs_ReleaseMask;
}

/* ---------------------------------------------------------------------
 * cnvs_setup_state(): allocate and fill in our own private state
 * --------------------------------------------------------------------- */

/* Allocates a cnvs_state, opens its output file, and fills in every
 * field from the arguments cnvs() was called with. This is everything
 * cnvs_device_() needs to set up on *our* side, before it ever touches
 * the graphics engine's own DevDesc struct (that's cnvs_setup_description()
 * instead, just below).
 *
 * On any failure, frees what it already allocated and calls R's error()
 * -- which performs a longjmp back out of this whole .Call(), so none of
 * the error branches below ever return to their caller. Since nothing
 * referring to `dd` happens until after this succeeds, there's no `dd`
 * to also clean up here; cnvs_device_() only needs to allocate `dd` once
 * it knows state is ready. */
static cnvs_state *cnvs_setup_state(const char *file, int width, int height,
                                    Rboolean standalone, const char *id_prefix)
{
    cnvs_state *state = (cnvs_state *) calloc(1, sizeof(cnvs_state));
    if (state == NULL) error("cnvs: could not allocate device state");

    state->fp = fopen(file, "w");
    if (state->fp == NULL) {
        free(state);
        error("cnvs: could not open '%s' for writing", file);
    }
    state->width = width;
    state->height = height;
    state->page_count = 0;
    state->clip_active = FALSE;
    state->standalone = standalone;
    /* Copied (rather than pointing straight at R's string storage) because
     * state outlives this call -- R could garbage-collect id_prefix_ long
     * before cnvs_Close() runs. Freed again in cnvs_Close(). */
    state->id_prefix = strdup(id_prefix);
    if (state->id_prefix == NULL) {
        fclose(state->fp);
        free(state);
        error("cnvs: could not allocate id_prefix");
    }

    return state;
}

/* ---------------------------------------------------------------------
 * cnvs_setup_description(): the graphics engine's own device description
 * --------------------------------------------------------------------- */

/* Fills in the parts of DevDesc that describe the device's physical
 * characteristics and capabilities to the graphics engine -- size,
 * character-metric bookkeeping, starting graphical parameters, and which
 * optional features (transparency, raster, locator, ...) it supports.
 * This is everything the engine needs to know about the device *before*
 * it starts calling drawing callbacks; wiring up those callbacks
 * themselves is cnvs_setup_callbacks()'s job instead. */
static void cnvs_setup_description(pDevDesc dd, int width, int height, 
                                   double pointsize, int bg)
{
    /* We model the canvas the same way a screen/bitmap device would:
     * origin at the top-left, y increasing downward, one device unit
     * equal to one CSS pixel. That means the coordinates the engine
     * passes into our drawing callbacks are already exactly the numbers
     * we want to write into canvas calls -- no flipping required. */
    dd->left = 0;
    dd->right = width;
    dd->top = 0;
    dd->bottom = height;
    dd->clipLeft = 0;
    dd->clipRight = width;
    dd->clipTop = 0;
    dd->clipBottom = height;

    dd->xCharOffset = CNVS_XCHAR_OFFSET;  /* defined as 0.4900 */
    dd->yCharOffset = CNVS_YCHAR_OFFSET;  /* defined as 0.3333 */
    dd->yLineBias = CNVS_YLINE_BIAS;      /* defined as 0.2000 */
    dd->ipr[0] = dd->ipr[1] = 1.0 / 96.0; /* 96 px per inch */
    dd->cra[0] = 0.9 * pointsize * (96.0 / 72.0);
    dd->cra[1] = 1.2 * pointsize * (96.0 / 72.0);
    dd->gamma = 1;

    dd->canClip = TRUE;        /* we implement the clip() callback */

    /* deviceClip overrides canClip, telling the engine to leave ALL
     * clipping to us rather than also doing its own clipping math on top
     * of ours (see the deviceClip field's comment in
     * R_ext/GraphicsDevice.h). We don't need that -- our clip() callback
     * only ever has to intersect one rectangle, which canvas's own
     * ctx.clip() already does for us -- so we leave the engine's
     * clipping on as a second line of defence instead of overriding it. */
    dd->deviceClip = FALSE;

    dd->canChangeGamma = FALSE;
    dd->canHAdj = 1;  /* we honour hadj at the 3 standard values: 0, 0.5, 1 */

    dd->startps = pointsize;
    dd->startcol = R_RGB(0, 0, 0);
    dd->startfill = bg;
    dd->startlty = LTY_SOLID;
    dd->startfont = 1;
    dd->startgamma = 1;

    dd->haveTransparency = 2;   /* yes -- colours have an alpha channel */
    dd->haveTransparentBg = 2;  /* yes -- fully transparent background is okay */
    dd->haveRaster = 1;         /* no -- raster() callback isn't implemented */
    dd->haveCapture = 1;        /* no -- cap() callback isn't implemented */
    dd->haveLocator = 1;        /* no -- locator() callback isn't implemented */

    /* Unlike a screen device, a file-based device like this one (or
     * pdf()/svglite()) doesn't need R to keep a display list around for
     * replaying/resizing, so we turn it off. */
    dd->displayListOn = FALSE;
}

/* ---------------------------------------------------------------------
 * cnvs_device_(): the entry point called from R via .Call()
 * --------------------------------------------------------------------- */

SEXP cnvs_device_(SEXP file_, SEXP width_, SEXP height_, SEXP pointsize_, 
                  SEXP bg_, SEXP standalone_, SEXP id_prefix_)
{
    const char *file = CHAR(STRING_ELT(file_, 0));
    int width = asInteger(width_);
    int height = asInteger(height_);
    double pointsize = asReal(pointsize_);
    int bg = RGBpar3(bg_, 0, R_RGB(255, 255, 255));
    Rboolean standalone = (Rboolean) asLogical(standalone_);
    const char *id_prefix = CHAR(STRING_ELT(id_prefix_, 0));

    /* Every device does these two checks before creating itself: confirm
     * we were compiled against headers compatible with the running R's
     * graphics engine, and confirm the engine is willing to let us open
     * another device right now. */
    R_GE_checkVersionOrDie(R_GE_version);
    R_CheckDeviceAvailable();

    /* Create the data structure that holds our own per-device state (the
     * open file handle, and bookkeeping like page count and clip state)
     * that every drawing callback needs in order to write the HTML/JS
     * output. */
    cnvs_state *state = cnvs_setup_state(file, width, height, 
                                           standalone, id_prefix);

    /* Allocate the device description struct, dd -- calloc() only
     * allocates and zeroes it; the real initialisation happens just below,
     * in cnvs_setup_description(). */
    pDevDesc dd = (pDevDesc) calloc(1, sizeof(DevDesc));
    if (dd == NULL) {
        fclose(state->fp);
        free(state->id_prefix);
        free(state);
        error("cnvs: could not allocate device description");
    }

    /* Describe this device to the graphics engine -- its size, character/
     * font metrics, starting graphical parameters, and which optional
     * features it supports. That's all set up in this helper. */
    cnvs_setup_description(dd, width, height, pointsize, bg);

    /* `deviceSpecific` is the slot R's DevDesc struct dedicates to a
     * device's own private data (see the comment on cnvs_state in
     * cnvs.h). For cnvs, that's our own state -- the open file handle and
     * per-device state every drawing callback needs, accessed back out via
     * the CNVS(dd) macro. */
    dd->deviceSpecific = state;

    /* `deviceVersion` tells the graphics engine which bundle of features
     * (see the version history at the top of R_ext/GraphicsEngine.h) this
     * device's callback table actually has slots for. It needs to match
     * what we've genuinely wired up in cnvs_setup_callbacks() just below:
     * we provide (trivial, declining) implementations of the "R_GE_definitions"
     * (13) group -- setPattern/setClipPath/setMask and their release
     * counterparts -- because grid calls those unconditionally (e.g. on
     * every grid.newpage(), to clear any mask) regardless of
     * deviceVersion, so leaving them NULL crashes R the moment grid
     * graphics (ggplot2, lattice -- base graphics never triggers this) is
     * used, even though plain plot() calls look fine. We stop at 13,
     * though, because we do NOT implement the group/path/glyph/variable
     * font callbacks that versions 15-17 require; claiming one of those
     * higher versions without implementing them would reintroduce exactly
     * this kind of crash for whatever grid feature touches groups. */
    dd->deviceVersion = R_GE_definitions;

    /* Every R graphics device must specify callback functions that get
     * triggered whenever the device is asked to take action. Those get
     * set up in this helper function. */
    cnvs_setup_callbacks(dd);

    /* Everything above has just been building our own `dd` in memory --
     * the graphics engine doesn't know it exists yet. These three calls
     * are what actually hand it over and make it "the current device":
     *   - GEcreateDevDesc() wraps our `dd` in the engine's own pGEDevDesc
     *     struct, which adds the engine-level bookkeeping (e.g. the
     *     display list below) that sits on top of a device's own
     *     description. From here on, it's this wrapped `gdd`, not the
     *     raw `dd`, that the rest of the engine operates on.
     *   - GEaddDevice2() registers that wrapped device in R's device list
     *     (what dev.list()/dev.cur() report) under the name "cnvs", and
     *     makes it the new current device, so that plotting calls after
     *     this point in R are routed to our callbacks.
     *   - GEinitDisplayList() sets up the (empty) display-list bookkeeping
     *     the engine keeps per device. We've told it we don't want pages
     *     replayed/resized via dd->displayListOn = FALSE above, but every
     *     device -- ours included -- is still expected to have this
     *     called once up front, the same as pdf()/svg() and friends do. */
    pGEDevDesc gdd = GEcreateDevDesc(dd);
    GEaddDevice2(gdd, "cnvs");
    GEinitDisplayList(gdd);

    /* The outer <html>/<body> scaffolding is written once, up front.
     * Individual pages (one <canvas>/<script> block each) are opened and
     * closed by the newPage/close callbacks as plotting proceeds. */
    cnvs_write_header(state);

    return R_NilValue;
}
