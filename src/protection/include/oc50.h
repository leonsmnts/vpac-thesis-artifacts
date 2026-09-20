/*
 * ANSI 50 – instantaneous overcurrent
 */

#ifndef PROT_OC50_H
#define PROT_OC50_H

#include "prot_common.h"


typedef struct {
    /* Pickup thresholds per phase (same index convention as above). */
    float pickup_current[4];

    /* Optional: which phases are enabled (bit mask). */
    uint32_t enabled_phase_mask;
} oc50_settings_t;

typedef struct {
    /* For pure 50 there is typically no internal state.
     * We keep this struct to have a uniform API across functions.
     */
    uint32_t reserved;
} oc50_state_t;

/*
 * Initialize ANSI 50 state.
 * Typically sets state fields to zero.
 */
void oc50_init(oc50_state_t *state);

/*
 * Execute one ANSI 50 step.
 *
 * Parameters:
 *   settings  - element configuration (pickup thresholds, enabled phases).
 *   state     - element state (unused for pure 50, but kept for API symmetry).
 *   frame     - measurement values for this step (currents, voltages).
 *   dt_microseconds - elapsed time since previous call in microseconds.
 *                      Not used for 50, but present for API consistency and
 *                      easy swapping with time-dependent functions (51, etc.).
 *
 * Returns:
 *   protection_output_t with start/trip/block masks set for this step.
 *
 * Notes:
 *   - This function performs no I/O, no dynamic allocation, no blocking calls.
 *   - It is designed to be called in the hot path.
 */
protection_output_t
oc50_step(const oc50_settings_t          *settings,
          oc50_state_t                   *state,
          const vpac_measurement_frame_t *frame,
          uint32_t                        dt_microseconds);


#endif /* PROT_OC50_H */
