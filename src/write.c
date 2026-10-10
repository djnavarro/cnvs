/* write.c -- HTML/JS boilerplate for the cnvs device
 *
 * This file knows nothing about R's graphics model. Its only job is text:
 * writing the <html>/<canvas>/<script> scaffolding that wraps the drawing
 * commands produced by device.c and draw.c. Keeping this separate makes it
 * easy to see, in one place, exactly what the output file looks like.
 *
 * One `<canvas>` + `<script>` pair is written per *page* (i.e. per call to
 * the newPage callback). Wrapping each page's script in an immediately
 * invoked function expression `(function() { ... })()` means the `ctx`
 * variable declared on one page doesn't leak into, or clash with, the next
 * page's script.
 */
#include "cnvs.h"
#include <stdlib.h>
#include <string.h>

void cnvs_write_header(cnvs_context *ctx)
{
    fprintf(ctx->fp,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<title>cnvs output</title>\n"
        "</head>\n"
        "<body>\n");
}

void cnvs_write_footer(cnvs_context *ctx)
{
    fprintf(ctx->fp, "</body>\n</html>\n");
}

void cnvs_start_page(cnvs_context *ctx, const pGEcontext gc)
{
    ctx->page_count++;
    fprintf(ctx->fp,
        "<canvas id=\"cnvs-page-%d\" width=\"%d\" height=\"%d\" "
        "style=\"border: 1px solid #ccc; margin-bottom: 1em;\"></canvas>\n"
        "<script>\n"
        "(function() {\n"
        "  var canvas = document.getElementById(\"cnvs-page-%d\");\n"
        "  var ctx = canvas.getContext(\"2d\");\n",
        ctx->page_count, ctx->width, ctx->height, ctx->page_count);

    /* The graphics engine always calls newPage with the background colour
     * to use for the page (see the `gc` parameter in the `newPage` entry
     * of R_ext/GraphicsDevice.h). We honour it here, before anything else
     * gets drawn, unless it's fully transparent. */
    if (gc != NULL && !R_TRANSPARENT(gc->fill)) {
        char css[32];
        cnvs_colour_to_css(gc->fill, css, sizeof(css));
        fprintf(ctx->fp, "  ctx.fillStyle = \"%s\";\n", css);
        fprintf(ctx->fp, "  ctx.fillRect(0, 0, %d, %d);\n", ctx->width, ctx->height);
    }
}

void cnvs_end_page(cnvs_context *ctx)
{
    fprintf(ctx->fp, "})();\n</script>\n");
}

/* Escape a string for safe inclusion inside a JS double-quoted string
 * literal (used for ctx.fillText(...)). The output buffer is allocated
 * here (worst case every character needs a backslash, so 2x + 1 is always
 * enough) and the caller is responsible for free()ing it. */
char *cnvs_js_escape(const char *in)
{
    size_t n = strlen(in);
    char *out = (char *) malloc(2 * n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        char c = in[i];
        if (c == '\\' || c == '"') {
            out[j++] = '\\';
            out[j++] = c;
        } else if (c == '\n') {
            out[j++] = '\\';
            out[j++] = 'n';
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    return out;
}
