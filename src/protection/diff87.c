#include <stddef.h>   // for NULL

#include "include/diff87.h"


void
diff87_init(diff87_state_t *state)
{
    if (state == NULL) {
        return;
    }
    for (int i = 0; i < 3; ++i) {
        state->accumulated_time_seconds[i] = 0.0f;
    }
}

protection_output_t
diff87_step(const diff87_settings_t         *settings,
            diff87_state_t                  *state,
            const vpac_measurement_frame_t  *frame,
            uint32_t                         dt_microseconds)
{
    protection_output_t output = {
        .start_mask = 0u,
        .trip_mask  = 0u,
        .block_mask = 0u
    };

    if (settings == NULL || state == NULL || frame == NULL) {
        return output;
    }

    const float dt_seconds      = (float)dt_microseconds / 1e6f;
    const float operate_delay   = settings->operate_delay_seconds;
    const float min_terminal    = settings->minimum_terminal_current;
    const float min_restraint   = settings->minimum_restraint_current;
    const float bias_slope      = settings->bias_slope;
    const float pickup_operate  = settings->pickup_operate_current;
    const float angle_tolerance = settings->angle_tolerance;

    /* Loop only over phases A, B, C; neutral is typically not part of 87. */
    for (int phase_index = 0; phase_index < 3; ++phase_index) {
        uint32_t phase_mask = prot_phase_index_to_mask(phase_index);

        if ((settings->enabled_phase_mask & phase_mask) == 0u) {
            continue;
        }

        vpac_complex_float_t i_local  = frame->local_current[phase_index];
        vpac_complex_float_t i_remote = frame->remote_current[phase_index];

        float mag_local  = current_magnitude_from_complex(i_local);
        float mag_remote = current_magnitude_from_complex(i_remote);

        /* Require some current at either terminal. */
        if (mag_local < min_terminal && mag_remote < min_terminal) {
            state->accumulated_time_seconds[phase_index] = 0.0f;
            continue;
        }

        /* Compute operate and restraint currents. */
        vpac_complex_float_t diff;
        diff.real = i_local.real - i_remote.real;
        diff.imag = i_local.imag - i_remote.imag;

        float operate_current   = current_magnitude_from_complex(diff);
        float restraint_current = 0.5f * (mag_local + mag_remote);

        /* Optional angle restraint between local and remote currents. */
        if (angle_tolerance > 0.0f) {
            float angle_local  = normalize_angle(current_angle_from_complex(i_local));
            float angle_remote = normalize_angle(current_angle_from_complex(i_remote));
            float angle_diff   = fabsf(normalize_angle(angle_local - angle_remote));

            if (angle_diff > angle_tolerance) {
                state->accumulated_time_seconds[phase_index] = 0.0f;
                continue;
            }
        }

        /* Evaluate percentage differential characteristic. */
        float operate_threshold;

        if (restraint_current < min_restraint) {
            /* Below minimum restraint: simple differential pickup. */
            operate_threshold = pickup_operate;
        } else {
            /* Percentage bias: threshold grows with restraint. */
            operate_threshold = pickup_operate + bias_slope * restraint_current;
        }

        if (operate_current < operate_threshold) {
            /* Out of operate region: reset timer. */
            state->accumulated_time_seconds[phase_index] = 0.0f;
            continue;
        }

        /* In operate region: element has started. */
        output.start_mask |= phase_mask;

        if (operate_delay > 0.0f) {
            float *accumulated = &state->accumulated_time_seconds[phase_index];
            *accumulated += dt_seconds;

            if (*accumulated >= operate_delay) {
                output.trip_mask |= phase_mask;
                /* Optional: Latch at delay for clarity. */
                // *accumulated = operate_delay;
            }
        } else {
            /* No delay configured: instantaneous differential trip. */
            output.trip_mask |= phase_mask;
        }
    }

    return output;
}
