#include <stddef.h>   // for NULL

#include "include/oc50.h"


void
oc50_init(oc50_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->reserved = 0u;
}

protection_output_t
oc50_step(const oc50_settings_t          *settings,
          oc50_state_t                   *state,
          const vpac_measurement_frame_t *frame,
          uint32_t                        dt_microseconds)
{
    (void)state;           /* unused in pure 50 */
    (void)dt_microseconds; /* unused in pure 50 */

    protection_output_t output = {
        .start_mask = 0u,
        .trip_mask  = 0u,
        .block_mask = 0u
    };

    if (settings == NULL || frame == NULL) {
        return output;
    }

    /* For each phase (A, B, C, N) check instantaneous pickup. */
    for (int phase_index = 0; phase_index < 4; ++phase_index) {
        uint32_t phase_mask = prot_phase_index_to_mask(phase_index);

        /* Skip phases that are not enabled. */
        if ((settings->enabled_phase_mask & phase_mask) == 0u) {
            continue;
        }

        float phase_current = frame->current_magnitude[phase_index];
        float pickup        = settings->pickup_current[phase_index];

        /* If current is above pickup, the element has started and trips immediately. */
        if (phase_current >= pickup) {
            output.start_mask |= phase_mask;
            output.trip_mask  |= phase_mask;
        }
        /* In a more advanced design, one could also set block_mask here
         * based on external inhibit signals, undervoltage blocking, etc.
         * For now block_mask is left at zero.
         */
    }

    return output;
}
