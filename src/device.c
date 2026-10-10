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
    cnvs_context *ctx = CNVS(dd);

    /* newPage is called once before the very first thing is drawn, and
     * again every time plotting starts a fresh page (e.g. a second
     * plot() call, or the next panel in par(mfrow = ...)). Each page gets
     * its own <canvas>/<script> block, so if a previous page is open we
     * need to close it out first. */
    if (ctx->page_count > 0) {
        if (ctx->clip_active) {
            fprintf(ctx->fp, "  ctx.restore();\n");
            ctx->clip_active = FALSE;
        }
        cnvs_end_page(ctx);
    }
    cnvs_start_page(ctx, gc);
}

void cnvs_Close(pDevDesc dd)
{
    cnvs_context *ctx = CNVS(dd);

    if (ctx->page_count > 0) {
        if (ctx->clip_active) {
            fprintf(ctx->fp, "  ctx.restore();\n");
        }
        cnvs_end_page(ctx);
    }
    cnvs_write_footer(ctx);
    fclose(ctx->fp);
    free(ctx);
    /* `dd` itself (the DevDesc struct) is freed by the graphics engine
     * right after this callback returns -- we only own `ctx`. */
}

void cnvs_Size(double *left, double *right, double *bottom, double *top, pDevDesc dd)
{
    /* Called if the engine ever needs to ask the device for its current
     * extent (we never resize ourselves, so this always returns the same
     * fixed values that were set up in cnvs_device_()). */
    cnvs_context *ctx = CNVS(dd);
    *left = 0;
    *right = ctx->width;
    *top = 0;
    *bottom = ctx->height;
}

/* ---------------------------------------------------------------------
 * Wiring up the callback table
 * --------------------------------------------------------------------- */

static void cnvs_setup_callbacks(pDevDesc dd)
{
    dd->activate   = cnvs_Activate;
    dd->deactivate = cnvs_Deactivate;
    dd->close      = cnvs_Close;
    dd->newPage    = cnvs_NewPage;
    dd->size       = cnvs_Size;
    dd->mode       = cnvs_Mode;

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
}

/* ---------------------------------------------------------------------
 * cnvs_device_(): the entry point called from R via .Call()
 * --------------------------------------------------------------------- */

SEXP cnvs_device_(SEXP file_, SEXP width_, SEXP height_, SEXP pointsize_, SEXP bg_)
{
    const char *file = CHAR(STRING_ELT(file_, 0));
    int width = asInteger(width_);
    int height = asInteger(height_);
    double pointsize = asReal(pointsize_);
    int bg = RGBpar3(bg_, 0, R_RGB(255, 255, 255));

    /* Every device does these two checks before creating itself: confirm
     * we were compiled against headers compatible with the running R's
     * graphics engine, and confirm the engine is willing to let us open
     * another device right now. */
    R_GE_checkVersionOrDie(R_GE_version);
    R_CheckDeviceAvailable();

    pDevDesc dd = (pDevDesc) calloc(1, sizeof(DevDesc));
    if (dd == NULL) error("cnvs: could not allocate device description");

    cnvs_context *ctx = (cnvs_context *) calloc(1, sizeof(cnvs_context));
    if (ctx == NULL) {
        free(dd);
        error("cnvs: could not allocate device context");
    }

    ctx->fp = fopen(file, "w");
    if (ctx->fp == NULL) {
        free(ctx);
        free(dd);
        error("cnvs: could not open '%s' for writing", file);
    }
    ctx->width = width;
    ctx->height = height;
    ctx->page_count = 0;
    ctx->clip_active = FALSE;

    /* ---- Physical device description ----
     * We model the canvas the same way a screen/bitmap device would:
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

    dd->xCharOffset = CNVS_XCHAR_OFFSET;
    dd->yCharOffset = CNVS_YCHAR_OFFSET;
    dd->yLineBias = CNVS_YLINE_BIAS;
    dd->ipr[0] = dd->ipr[1] = 1.0 / 96.0; /* 96 px per inch, like a typical screen */
    dd->cra[0] = 0.9 * pointsize * (96.0 / 72.0);
    dd->cra[1] = 1.2 * pointsize * (96.0 / 72.0);
    dd->gamma = 1;

    dd->canClip = TRUE;        /* we implement the clip() callback */
    dd->canChangeGamma = FALSE;
    dd->canHAdj = 1;           /* we honour hadj at the 3 standard values: 0, 0.5, 1 */

    dd->startps = pointsize;
    dd->startcol = R_RGB(0, 0, 0);
    dd->startfill = bg;
    dd->startlty = LTY_SOLID;
    dd->startfont = 1;
    dd->startgamma = 1;

    dd->haveTransparency = 2;   /* yes -- colours carry an alpha channel through to rgba() */
    dd->haveTransparentBg = 2;  /* yes -- a fully transparent background is supported */
    dd->haveRaster = 1;         /* no -- the raster() callback isn't implemented */
    dd->haveCapture = 1;        /* no -- the cap() callback isn't implemented */
    dd->haveLocator = 1;        /* no -- the locator() callback isn't implemented */

    /* Unlike a screen device, a file-based device like this one (or
     * pdf()/svglite()) doesn't need R to keep a display list around for
     * replaying/resizing, so we turn it off. */
    dd->displayListOn = FALSE;

    dd->deviceSpecific = ctx;
    dd->deviceVersion = R_GE_version;
    dd->deviceClip = FALSE; /* let the engine do its own clipping math too */

    cnvs_setup_callbacks(dd);

    pGEDevDesc gdd = GEcreateDevDesc(dd);
    GEaddDevice2(gdd, "cnvs");
    GEinitDisplayList(gdd);

    /* The outer <html>/<body> scaffolding is written once, up front.
     * Individual pages (one <canvas>/<script> block each) are opened and
     * closed by the newPage/close callbacks as plotting proceeds. */
    cnvs_write_header(ctx);

    return R_NilValue;
}
