/*
 * ANSI 51 DTL – definite-time overcurrent
 */

#ifndef PROT_OC51_DTL_H
#define PROT_OC51_DTL_H

#include "prot_common.h"


typedef struct {
    /* Pickup thresholds per phase (same index convention as above). */
    float pickup_current[4];

    /* Fixed operate delay when above pickup (seconds). */
    float operate_delay_seconds;

    /* Optional: which phases are enabled (bit mask). */
    uint32_t enabled_phase_mask;
} oc51_dtl_settings_t;

typedef struct {
    /* Accumulated time above pickup per phase (seconds). */
    float accumulated_time_seconds[4];
} oc51_dtl_state_t;

/* Initialize ANSI 51 definite-time state. */
void oc51_dtl_init(oc51_dtl_state_t *state);

/*
 * Execute one ANSI 51 definite-time step.
 *
 * Parameters:
 *   settings          - element configuration (pickup thresholds, delay, enabled phases).
 *   state             - element state (per-phase accumulated time).
 *   frame             - measurement values for this step (currents, voltages).
 *   dt_microseconds   - elapsed time since previous call in microseconds.
 *
 * Returns:
 *   protection_output_t with start/trip/block masks for this step.
 *
 * Behavior:
 *   - For each enabled phase:
 *       - If current >= pickup: start, accumulate time.
 *       - If accumulated_time >= operate_delay_seconds: trip.
 *       - If current < pickup: reset accumulated time and do not trip.
 */
protection_output_t
oc51_dtl_step(const oc51_dtl_settings_t      *settings,
              oc51_dtl_state_t               *state,
              const vpac_measurement_frame_t *frame,
              uint32_t                        dt_microseconds);


#endif /* PROT_OC51_DTL_H */
