/* draw.c -- the cnvs device's drawing callbacks
 *
 * Every function here corresponds to one entry in R's device callback
 * table (see R_ext/GraphicsDevice.h). R's graphics engine never draws
 * anything itself -- when user code calls e.g. `lines()`, the engine works
 * out the device coordinates and current graphical parameters and then
 * calls `dd->polyline(...)` on whichever device is active. Our job in each
 * callback is just to translate "draw this shape, with these parameters"
 * into the matching sequence of canvas JS calls.
 *
 * A few recurring patterns:
 *   - `gc->col` is the "foreground"/stroke colour, `gc->fill` is the fill
 *     colour. Either can be `NA_INTEGER`, meaning "don't draw that part at
 *     all" (e.g. an unfilled rectangle has gc->fill == NA_INTEGER).
 *   - `gc->lty == LTY_BLANK` means "invisible line", checked separately
 *     from colour.
 *   - Coordinates arrive already in device (pixel) space, thanks to how we
 *     set up dd->left/right/top/bottom in device.c -- we can write them
 *     straight into canvas calls.
 */
#include "cnvs.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void cnvs_Line(double x1, double y1, double x2, double y2,
               const pGEcontext gc, pDevDesc dd)
{
    cnvs_context *ctx = CNVS(dd);
    if (gc->col == NA_INTEGER || gc->lty == LTY_BLANK) return;

    char css[32];
    cnvs_colour_to_css(gc->col, css, sizeof(css));
    cnvs_set_line_style(ctx->fp, gc);
    fprintf(ctx->fp, "  ctx.strokeStyle = \"%s\";\n", css);
    fprintf(ctx->fp, "  ctx.beginPath();\n");
    fprintf(ctx->fp, "  ctx.moveTo(%.2f, %.2f);\n", x1, y1);
    fprintf(ctx->fp, "  ctx.lineTo(%.2f, %.2f);\n", x2, y2);
    fprintf(ctx->fp, "  ctx.stroke();\n");
}

void cnvs_Rect(double x0, double y0, double x1, double y1,
               const pGEcontext gc, pDevDesc dd)
{
    cnvs_context *ctx = CNVS(dd);

    /* (x0,y0) and (x1,y1) are opposite corners, but not guaranteed to
     * arrive as (top-left, bottom-right) -- normalise to canvas's
     * expected (x, y, width, height) form. */
    double left = (x0 < x1) ? x0 : x1;
    double top = (y0 < y1) ? y0 : y1;
    double w = fabs(x1 - x0);
    double h = fabs(y1 - y0);

    if (gc->fill != NA_INTEGER && !R_TRANSPARENT(gc->fill)) {
        char css[32];
        cnvs_colour_to_css(gc->fill, css, sizeof(css));
        fprintf(ctx->fp, "  ctx.fillStyle = \"%s\";\n", css);
        fprintf(ctx->fp, "  ctx.fillRect(%.2f, %.2f, %.2f, %.2f);\n", left, top, w, h);
    }
    if (gc->col != NA_INTEGER && gc->lty != LTY_BLANK) {
        char css[32];
        cnvs_colour_to_css(gc->col, css, sizeof(css));
        cnvs_set_line_style(ctx->fp, gc);
        fprintf(ctx->fp, "  ctx.strokeStyle = \"%s\";\n", css);
        fprintf(ctx->fp, "  ctx.strokeRect(%.2f, %.2f, %.2f, %.2f);\n", left, top, w, h);
    }
}

void cnvs_Circle(double x, double y, double r,
                  const pGEcontext gc, pDevDesc dd)
{
    cnvs_context *ctx = CNVS(dd);

    fprintf(ctx->fp, "  ctx.beginPath();\n");
    fprintf(ctx->fp, "  ctx.arc(%.2f, %.2f, %.2f, 0, 2 * Math.PI);\n", x, y, r);

    if (gc->fill != NA_INTEGER && !R_TRANSPARENT(gc->fill)) {
        char css[32];
        cnvs_colour_to_css(gc->fill, css, sizeof(css));
        fprintf(ctx->fp, "  ctx.fillStyle = \"%s\";\n", css);
        fprintf(ctx->fp, "  ctx.fill();\n");
    }
    if (gc->col != NA_INTEGER && gc->lty != LTY_BLANK) {
        char css[32];
        cnvs_colour_to_css(gc->col, css, sizeof(css));
        cnvs_set_line_style(ctx->fp, gc);
        fprintf(ctx->fp, "  ctx.strokeStyle = \"%s\";\n", css);
        fprintf(ctx->fp, "  ctx.stroke();\n");
    }
}

void cnvs_Polyline(int n, double *x, double *y,
                    const pGEcontext gc, pDevDesc dd)
{
    cnvs_context *ctx = CNVS(dd);
    if (gc->col == NA_INTEGER || gc->lty == LTY_BLANK || n < 2) return;

    char css[32];
    cnvs_colour_to_css(gc->col, css, sizeof(css));
    cnvs_set_line_style(ctx->fp, gc);
    fprintf(ctx->fp, "  ctx.strokeStyle = \"%s\";\n", css);
    fprintf(ctx->fp, "  ctx.beginPath();\n");
    fprintf(ctx->fp, "  ctx.moveTo(%.2f, %.2f);\n", x[0], y[0]);
    for (int i = 1; i < n; i++) {
        fprintf(ctx->fp, "  ctx.lineTo(%.2f, %.2f);\n", x[i], y[i]);
    }
    fprintf(ctx->fp, "  ctx.stroke();\n");
}

void cnvs_Polygon(int n, double *x, double *y,
                   const pGEcontext gc, pDevDesc dd)
{
    cnvs_context *ctx = CNVS(dd);
    if (n < 2) return;

    fprintf(ctx->fp, "  ctx.beginPath();\n");
    fprintf(ctx->fp, "  ctx.moveTo(%.2f, %.2f);\n", x[0], y[0]);
    for (int i = 1; i < n; i++) {
        fprintf(ctx->fp, "  ctx.lineTo(%.2f, %.2f);\n", x[i], y[i]);
    }
    fprintf(ctx->fp, "  ctx.closePath();\n");

    if (gc->fill != NA_INTEGER && !R_TRANSPARENT(gc->fill)) {
        char css[32];
        cnvs_colour_to_css(gc->fill, css, sizeof(css));
        fprintf(ctx->fp, "  ctx.fillStyle = \"%s\";\n", css);
        fprintf(ctx->fp, "  ctx.fill();\n");
    }
    if (gc->col != NA_INTEGER && gc->lty != LTY_BLANK) {
        char css[32];
        cnvs_colour_to_css(gc->col, css, sizeof(css));
        cnvs_set_line_style(ctx->fp, gc);
        fprintf(ctx->fp, "  ctx.strokeStyle = \"%s\";\n", css);
        fprintf(ctx->fp, "  ctx.stroke();\n");
    }
}

void cnvs_Text(double x, double y, const char *str, double rot, double hadj,
               const pGEcontext gc, pDevDesc dd)
{
    cnvs_context *ctx = CNVS(dd);
    if (gc->col == NA_INTEGER || str == NULL) return;

    char css[32];
    cnvs_colour_to_css(gc->col, css, sizeof(css));

    /* `hadj` says how the string should be justified relative to (x, y):
     * 0 = left-aligned, 0.5 = centred, 1 = right-aligned (and, in
     * principle, any value in between -- but we told the engine
     * `canHAdj = 1`, meaning we only promise to honour these three). This
     * maps directly onto canvas's own `textAlign` property. */
    const char *align = "left";
    if (hadj > 0.25 && hadj < 0.75) align = "center";
    else if (hadj >= 0.75) align = "right";

    /* We draw rotated text by moving the whole coordinate system (origin
     * to (x, y), then rotated) rather than computing rotated coordinates
     * ourselves -- ctx.save()/ctx.restore() bracket the change so it
     * doesn't affect anything drawn afterwards. */
    fprintf(ctx->fp, "  ctx.save();\n");
    cnvs_set_font(ctx->fp, gc);
    fprintf(ctx->fp, "  ctx.fillStyle = \"%s\";\n", css);
    fprintf(ctx->fp, "  ctx.textAlign = \"%s\";\n", align);
    fprintf(ctx->fp, "  ctx.textBaseline = \"alphabetic\";\n");
    fprintf(ctx->fp, "  ctx.translate(%.2f, %.2f);\n", x, y);
    if (rot != 0) {
        /* `rot` is in degrees, anticlockwise from the x-axis, measured in
         * DEVICE coordinates. Our device coordinates have y increasing
         * downward (same as canvas) rather than the "mathematical" y-up
         * convention that "anticlockwise" usually assumes -- so, exactly
         * like R's own Cairo-based raster devices do for the same reason,
         * we negate the angle before calling ctx.rotate() to keep the
         * visual rotation direction consistent with user expectation. The
         * degrees-to-radians conversion itself is left to the generated
         * JS (via Math.PI, as cnvs_Circle() already does for its
         * full-circle arc) rather than computed here in C, so there's
         * only one place in the output that ever needs to agree with
         * JavaScript's idea of pi. */
        fprintf(ctx->fp, "  ctx.rotate(%.5f * Math.PI / 180);\n", -rot);
    }
    char *escaped = cnvs_js_escape(str);
    fprintf(ctx->fp, "  ctx.fillText(\"%s\", 0, 0);\n", escaped);
    free(escaped);
    fprintf(ctx->fp, "  ctx.restore();\n");
}

double cnvs_StrWidth(const char *str, const pGEcontext gc, pDevDesc dd)
{
    /* There's no way to get an *exact* text width without an actual font
     * rendering engine -- which is exactly what the browser viewing our
     * HTML output will have, but we (running inside R, server-side) do
     * not. Real devices solve this by querying platform font APIs (e.g.
     * Pango, GDI, Core Text); we fall back on a simple heuristic instead:
     * assume each character is, on average, a fixed fraction of the font
     * size wide. This is good enough to get rough layout right, but will
     * not exactly match what the browser renders -- a good illustration
     * of a limitation this toy device doesn't try to hide. */
    double size_px = gc->cex * gc->ps * (96.0 / 72.0);
    double avg_char_width = (gc->fontface == 2 || gc->fontface == 4) ? 0.60 : 0.55;
    return strlen(str) * avg_char_width * size_px;
}

void cnvs_MetricInfo(int c, const pGEcontext gc,
                      double *ascent, double *descent, double *width,
                      pDevDesc dd)
{
    double size_px = gc->cex * gc->ps * (96.0 / 72.0);

    /* Typical typographic proportions for Latin text -- again, an
     * approximation rather than a measurement from the actual font. */
    *ascent = 0.75 * size_px;
    *descent = 0.25 * size_px;

    /* By convention, c == 0 means "just tell me about the font as a
     * whole" (used e.g. to work out line spacing), not a specific
     * character, so width should be 0 in that case. */
    if (c == 0) {
        *width = 0;
        return;
    }
    double avg_char_width = (gc->fontface == 2 || gc->fontface == 4) ? 0.60 : 0.55;
    *width = avg_char_width * size_px;
}

void cnvs_Clip(double x0, double x1, double y0, double y1, pDevDesc dd)
{
    cnvs_context *ctx = CNVS(dd);
    if (ctx->page_count == 0) return; /* nothing drawn yet; nothing to clip */

    /* Canvas clip regions can only ever be *intersected* with the current
     * one -- there's no way to grow or replace a clip region without
     * first ctx.restore()-ing back to a state that didn't have it. Since R
     * just tells us "the clip region is now this rectangle" (not "add
     * this rectangle to the clip region"), we restore any previous clip
     * before applying the new one. */
    if (ctx->clip_active) {
        fprintf(ctx->fp, "  ctx.restore();\n");
    }

    double left = (x0 < x1) ? x0 : x1;
    double right = (x0 < x1) ? x1 : x0;
    double top = (y0 < y1) ? y0 : y1;
    double bottom = (y0 < y1) ? y1 : y0;

    fprintf(ctx->fp, "  ctx.save();\n");
    fprintf(ctx->fp, "  ctx.beginPath();\n");
    fprintf(ctx->fp, "  ctx.rect(%.2f, %.2f, %.2f, %.2f);\n", left, top, right - left, bottom - top);
    fprintf(ctx->fp, "  ctx.clip();\n");
    ctx->clip_active = TRUE;
}
