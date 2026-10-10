/* init.c -- native routine registration
 *
 * When an R package has compiled code, it needs to tell R which C
 * functions are callable from R and under what names. This file builds
 * that lookup table and registers it when the package's shared library is
 * loaded (R calls R_init_<pkgname> automatically at load time -- that
 * naming convention is how R finds this function).
 *
 * Registering routines this way (rather than relying on R to search for
 * symbols by name at call time) is both faster and safer: see "System and
 * foreign language interfaces" in Writing R Extensions for the full
 * rationale.
 */
#include "cnvs.h"
#include <R_ext/Rdynload.h>

/* declared in device.c */
SEXP cnvs_device_(SEXP file_, SEXP width_, SEXP height_, SEXP pointsize_, SEXP bg_);

static const R_CallMethodDef CallEntries[] = {
    {"cnvs_device_", (DL_FUNC) &cnvs_device_, 5},
    {NULL, NULL, 0}
};

void R_init_cnvs(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
