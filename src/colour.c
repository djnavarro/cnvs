/* colour.c -- translating R's graphics parameters into CSS/JS
 *
 * R passes every drawing callback a `pGEcontext gc` -- a bundle of
 * "current graphical parameters" (colour, line width, line type, font...)
 * that the device is expected to read and honour (see the R_GE_gcontext
 * struct in R_ext/GraphicsEngine.h). This file is where we translate those
 * values into the matching bits of CSS/canvas JS.
 */
#include "cnvs.h"
#include <string.h>

/* R packs a colour's red, green, blue and alpha channels into a single
 * `int`, one byte each (red in the lowest byte, alpha in the highest) --
 * see R_RGBA()/R_RED()/R_GREEN()/R_BLUE()/R_ALPHA() in
 * R_ext/GraphicsDevice.h. CSS instead wants `rgba(r, g, b, a)` with alpha
 * expressed as a 0..1 fraction rather than a 0..255 byte, so there's a
 * small unit conversion here as well as the byte unpacking. */
void cnvs_colour_to_css(int col, char *buf, size_t buf_size)
{
    int r = R_RED(col);
    int g = R_GREEN(col);
    int b = R_BLUE(col);
    double a = R_ALPHA(col) / 255.0;
    snprintf(buf, buf_size, "rgba(%d, %d, %d, %.3f)", r, g, b, a);
}

/* Set up everything about how a *stroked* path should look: width, caps,
 * joins, and dashing. Colour is deliberately not set here, since not every
 * caller wants the stroke colour (e.g. cnvs_Rect calls this only when it's
 * about to draw a border). */
void cnvs_set_line_style(FILE *fp, const pGEcontext gc)
{
    fprintf(fp, "  ctx.lineWidth = %.2f;\n", gc->lwd);

    switch (gc->lend) {
    case GE_ROUND_CAP:  fprintf(fp, "  ctx.lineCap = \"round\";\n");  break;
    case GE_SQUARE_CAP: fprintf(fp, "  ctx.lineCap = \"square\";\n"); break;
    case GE_BUTT_CAP:
    default:            fprintf(fp, "  ctx.lineCap = \"butt\";\n");   break;
    }

    switch (gc->ljoin) {
    case GE_ROUND_JOIN: fprintf(fp, "  ctx.lineJoin = \"round\";\n"); break;
    case GE_BEVEL_JOIN: fprintf(fp, "  ctx.lineJoin = \"bevel\";\n"); break;
    case GE_MITRE_JOIN:
    default:
        fprintf(fp, "  ctx.lineJoin = \"miter\";\n");
        fprintf(fp, "  ctx.miterLimit = %.2f;\n", gc->lmitre);
        break;
    }

    /* R encodes a dash pattern as up to 8 hex digits packed into one int,
     * read from the least-significant nibble outward, each nibble giving
     * the length of one dash/gap segment in multiples of the line width
     * (see LTY_DASHED, LTY_DOTTED etc. in R_ext/GraphicsEngine.h, and try
     * `strtoi("42", base = 16L)` style arithmetic by hand on LTY_DASHED to
     * see where the "4" and "4" come from). lty == LTY_SOLID (0) means
     * "no dashing"; LTY_BLANK (-1) means invisible, which callers should
     * check for *before* drawing anything at all. */
    if (gc->lty == LTY_SOLID) {
        fprintf(fp, "  ctx.setLineDash([]);\n");
    } else {
        fprintf(fp, "  ctx.setLineDash([");
        unsigned int lty = (unsigned int) gc->lty;
        for (int i = 0; i < 8; i++) {
            unsigned int segment = lty & 0xF;
            if (segment == 0) break;
            fprintf(fp, "%s%.2f", i == 0 ? "" : ", ", segment * gc->lwd);
            lty >>= 4;
        }
        fprintf(fp, "]);\n");
    }
}

/* Build a CSS font shorthand string (e.g. "italic bold 16.0px sans-serif")
 * from the gcontext's font fields and write it to ctx.font. */
void cnvs_set_font(FILE *fp, const pGEcontext gc)
{
    /* Font size: gc->ps is the base pointsize (1 pt = 1/72 inch), and
     * gc->cex is a multiplier on top of that -- the actual rendered size
     * is their product (see the R_GE_gcontext comments). We're treating
     * the device as a 96-dpi screen (see dd->ipr in device.c), so
     * 1 pt = 96/72 px. */
    double size_px = gc->cex * gc->ps * (96.0 / 72.0);

    /* fontface: 1 = plain, 2 = bold, 3 = italic, 4 = bold italic,
     * 5 = symbol (not given any special treatment here -- an honest
     * limitation of this toy device, since proper symbol-font support
     * needs a glyph-mapping table). */
    const char *weight = "normal";
    const char *style = "normal";
    switch (gc->fontface) {
    case 2: weight = "bold";                 break;
    case 3: style  = "italic";                break;
    case 4: weight = "bold"; style = "italic"; break;
    default: break;
    }

    const char *family = gc->fontfamily;
    if (family == NULL || family[0] == '\0') {
        family = "sans-serif";
    } else if (strcmp(family, "sans") == 0) {
        family = "sans-serif";
    } else if (strcmp(family, "mono") == 0) {
        family = "monospace";
    }
    /* "serif" is already a valid CSS generic family name, and anything
     * else is passed through as-is (e.g. a specific font name). */

    fprintf(fp, "  ctx.font = \"%s %s %.1fpx %s\";\n", style, weight, size_px, family);
}
