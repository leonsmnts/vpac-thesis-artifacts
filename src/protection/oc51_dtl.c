#include <stddef.h>   // for NULL

#include "include/oc51_dtl.h"

void
oc51_dtl_init(oc51_dtl_state_t *state)
{
    if (state == NULL) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        state->accumulated_time_seconds[i] = 0.0f;
    }
}

protection_output_t
oc51_dtl_step(const oc51_dtl_settings_t      *settings,
              oc51_dtl_state_t               *state,
              const vpac_measurement_frame_t *frame,
              uint32_t                        dt_microseconds)
{
    protection_output_t output = {
        .start_mask = 0u,
        .trip_mask  = 0u,
        .block_mask = 0u
    };

    if (settings == NULL || state == NULL || frame == NULL) {
        return output;
    }

    /* Convert dt from microseconds to seconds once per call. */
    const float dt_seconds = (float)dt_microseconds / 1e6f;

    for (int phase_index = 0; phase_index < 4; ++phase_index) {
        uint32_t phase_mask = prot_phase_index_to_mask(phase_index);

        /* Skip phases that are not enabled. */
        if ((settings->enabled_phase_mask & phase_mask) == 0u) {
            continue;
        }

        float phase_current      = frame->current_magnitude[phase_index];
        float pickup_current     = settings->pickup_current[phase_index];
        float operate_delay      = settings->operate_delay_seconds;
        float *accumulated_time  = &state->accumulated_time_seconds[phase_index];

        if (phase_current >= pickup_current) {
            /* Above pickup: element has started. */
            output.start_mask |= phase_mask;

            /* Accumulate time above pickup. */
            *accumulated_time += dt_seconds;

            /* Check if definite-time delay has expired. */
            if (*accumulated_time >= operate_delay) {
                output.trip_mask |= phase_mask;
            }
        } else {
            /* Below pickup: reset timer. */
            *accumulated_time = 0.0f;
        }

        /* block_mask left at zero for now; external blocking logic can
         * be added later via additional inputs if needed.
         */
    }

    return output;
}
