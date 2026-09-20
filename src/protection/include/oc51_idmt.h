/*
 * ANSI 51 IDMT – inverse-time overcurrent
 */

#ifndef PROT_OC51_IDMT_H
#define PROT_OC51_IDMT_H

#include "prot_common.h"


typedef struct {
    /* Pickup thresholds per phase (same index convention as above). */
    float pickup_current[4];

    /* Time multiplier / time dial setting. */
    float time_multiplier;

    /* IEC/IEEE curve parameters:
     *   operate_time = time_multiplier * curve_constant_a
     *                  / ( (I/Ip)^curve_exponent_p - 1 )
     *
     * Typical IEC values:
     *   Standard inverse:       a = 0.14,   p = 0.02
     *   Very inverse:           a = 13.5,   p = 1.0
     *   Extremely inverse:      a = 80.0,   p = 2.0
     */
    float curve_constant_a;
    float curve_exponent_p;

    /* Optional clamps on operate time (seconds); set to 0 to disable. */
    float minimum_operate_time_seconds;
    float maximum_operate_time_seconds;

    /* Optional: which phases are enabled (bit mask). */
    uint32_t enabled_phase_mask;
} oc51_idmt_settings_t;

typedef struct {
    /* Normalized progress per phase in [0, +inf).
     * Trip when progress >= 1.0.
     */
    float progress[4];
} oc51_idmt_state_t;

/* Initialize ANSI 51 IDMT state. */
void oc51_idmt_init(oc51_idmt_state_t *state);

/*
 * Execute one ANSI 51 IDMT step.
 *
 * Parameters:
 *   settings          - element configuration (pickup, curve, time multiplier, etc.).
 *   state             - element state (per-phase progress).
 *   frame             - measurement values for this step (currents, voltages).
 *   dt_microseconds   - elapsed time since previous call in microseconds.
 *
 * Returns:
 *   protection_output_t with start/trip/block masks for this step.
 *
 * Behavior:
 *   - For each enabled phase:
 *       - If I < pickup: clear start, reset progress to 0.
 *       - If I >= pickup:
 *           * Set start.
 *           * Compute operate_time from IEC/IEEE inverse curve.
 *           * Increase progress by dt / operate_time.
 *           * Trip when progress >= 1.0.
 */
protection_output_t
oc51_idmt_step(const oc51_idmt_settings_t     *settings,
               oc51_idmt_state_t              *state,
               const vpac_measurement_frame_t *frame,
               uint32_t                        dt_microseconds);


#endif /* PROT_OC51_IDMT_H */
