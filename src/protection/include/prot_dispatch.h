#ifndef PROT_DISPATCH_H
#define PROT_DISPATCH_H

#include "prot_common.h"
#include "oc50.h"
#include "oc51_dtl.h"
#include "oc51_idmt.h"
#include "dir67.h"
#include "dist21.h"
#include "diff87.h"

/*
 * Initialize settings and state for all ANSI functions.
 *
 * For now this sets reasonable default test values (similar to benchmarking setup). Later one can load settings from e.g. config or
 * SCL files.
 */
void prot_dispatch_init(void);

/*
 * Call all ANSI protection functions on a single measurement frame.
 *
 * Parameters:
 *   frame           - measurement values (currents, voltages, phasors).
 *   dt_microseconds - elapsed time since previous call (microseconds).
 *   out             - combined protection output; masks are OR'd across
 *                     all functions.
 *
 * Notes:
 *   - This function does not perform any I/O.
 *   - It is designed to be part of the hot protection path.
 */
void prot_dispatch_all(const vpac_measurement_frame_t *frame,
                       uint32_t                        dt_microseconds,
                       protection_output_t             *out);

#endif /* PROT_DISPATCH_H */
