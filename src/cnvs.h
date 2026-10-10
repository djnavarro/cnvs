/* cnvs.h -- shared declarations for the cnvs graphics device
 *
 * R graphics devices are split across several callback functions (one for
 * drawing a line, one for drawing text, one for starting a new page, ...)
 * that the graphics engine calls as it processes plotting commands. This
 * header ties those callbacks together: it defines the small struct we use
 * to remember "where are we writing output to" between calls, and declares
 * every function that the different .c files need to share.
 *
 * The source files are organised by what they are responsible for:
 *   - write.c   turns "start a page" / "end a page" into HTML/JS text
 *   - colour.c  turns R graphics-parameter values (colours, line styles,
 *               fonts) into the equivalent CSS/JS text
 *   - device.c  the device *lifecycle*: creating it, closing it, resizing
 *               it, starting new pages
 *   - draw.c    the actual drawing primitives: lines, rectangles, circles,
 *               polygons, text
 */
#ifndef CNVS_H
#define CNVS_H

#include <R.h>
#include <Rinternals.h>
#include <R_ext/GraphicsEngine.h>
#include <R_ext/GraphicsDevice.h>
#include <stdio.h>

/* ---------------------------------------------------------------------
 * Device-specific state.
 *
 * R's DevDesc struct has a single `void *deviceSpecific` slot for a device
 * to stash whatever private data it needs. Every callback is handed the
 * pDevDesc, and the very first thing each one does is pull this struct
 * back out via dd->deviceSpecific (see the CNVS() macro below). This is
 * the *only* memory our device has from one call to the next -- there are
 * no global variables, because R allows more than one cnvs device to be
 * open at once.
 * --------------------------------------------------------------------- */
typedef struct {
    FILE *fp;              /* the open .html file we are writing to */
    int width;             /* device width, in CSS pixels */
    int height;            /* device height, in CSS pixels */
    int page_count;        /* how many <canvas> pages we've started so far */
    Rboolean clip_active;  /* TRUE if we've done ctx.save()+ctx.clip() since
                            * the last matching ctx.restore() */
} cnvs_context;

/* Convenience accessor: fetch our struct back out of a pDevDesc. */
#define CNVS(dd) ((cnvs_context *) (dd)->deviceSpecific)

/* --- write.c: writing HTML/JS boilerplate text to the output file --- */
void cnvs_write_header(cnvs_context *ctx);
void cnvs_write_footer(cnvs_context *ctx);
void cnvs_start_page(cnvs_context *ctx, const pGEcontext gc);
void cnvs_end_page(cnvs_context *ctx);
char *cnvs_js_escape(const char *in);

/* --- colour.c: translating R graphics-parameter values into CSS/JS --- */
void cnvs_colour_to_css(int col, char *buf, size_t buf_size);
void cnvs_set_line_style(FILE *fp, const pGEcontext gc);
void cnvs_set_font(FILE *fp, const pGEcontext gc);

/* --- device.c: device lifecycle callbacks --- */
void cnvs_Activate(const pDevDesc dd);
void cnvs_Deactivate(pDevDesc dd);
void cnvs_Close(pDevDesc dd);
void cnvs_NewPage(const pGEcontext gc, pDevDesc dd);
void cnvs_Size(double *left, double *right, double *bottom, double *top, pDevDesc dd);
void cnvs_Mode(int mode, pDevDesc dd);

/* --- draw.c: device drawing callbacks --- */
void cnvs_Line(double x1, double y1, double x2, double y2,
               const pGEcontext gc, pDevDesc dd);
void cnvs_Rect(double x0, double y0, double x1, double y1,
               const pGEcontext gc, pDevDesc dd);
void cnvs_Circle(double x, double y, double r,
                  const pGEcontext gc, pDevDesc dd);
void cnvs_Polyline(int n, double *x, double *y,
                    const pGEcontext gc, pDevDesc dd);
void cnvs_Polygon(int n, double *x, double *y,
                   const pGEcontext gc, pDevDesc dd);
void cnvs_Text(double x, double y, const char *str, double rot, double hadj,
               const pGEcontext gc, pDevDesc dd);
void cnvs_Clip(double x0, double x1, double y0, double y1, pDevDesc dd);
double cnvs_StrWidth(const char *str, const pGEcontext gc, pDevDesc dd);
void cnvs_MetricInfo(int c, const pGEcontext gc,
                      double *ascent, double *descent, double *width,
                      pDevDesc dd);

#endif /* CNVS_H */
