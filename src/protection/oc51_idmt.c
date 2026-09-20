#include <stddef.h>   // for NULL
#include <math.h>

#include "include/oc51_idmt.h"


void
oc51_idmt_init(oc51_idmt_state_t *state)
{
    if (state == NULL) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        state->progress[i] = 0.0f;
    }
}

protection_output_t
oc51_idmt_step(const oc51_idmt_settings_t      *settings,
               oc51_idmt_state_t               *state,
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

    const float dt_seconds = (float)dt_microseconds / 1e6f;

    /* Preload curve parameters for convenience. */
    const float a      = settings->curve_constant_a;
    const float p      = settings->curve_exponent_p;
    const float t_mult = settings->time_multiplier;

    const float t_min = settings->minimum_operate_time_seconds;
    const float t_max = settings->maximum_operate_time_seconds;

    for (int phase_index = 0; phase_index < 4; ++phase_index) {
        uint32_t phase_mask = prot_phase_index_to_mask(phase_index);

        if ((settings->enabled_phase_mask & phase_mask) == 0u) {
            continue;
        }

        float measured_current = frame->current_magnitude[phase_index];
        float pickup_current   = settings->pickup_current[phase_index];
        float *progress        = &state->progress[phase_index];

        if (pickup_current <= 0.0f) {
            /* Misconfiguration guard; treat as disabled. */
            *progress = 0.0f;
            continue;
        }

        if (measured_current < pickup_current) {
            /* Below pickup: reset progress and do not start. */
            *progress = 0.0f;
            continue;
        }

        /* Above pickup: element has started. */
        output.start_mask |= phase_mask;

        /* Compute current multiple and operate time from the inverse curve. */
        const float multiple = measured_current / pickup_current;

        /* Avoid numerical issues when multiple is very close to 1.0. */
        if (multiple <= 1.0001f || p <= 0.0f || a <= 0.0f || t_mult <= 0.0f) {
            /* Degenerate case: treat as extremely slow; no progress this step. */
            continue;
        }

        float denominator = powf(multiple, p) - 1.0f;
        if (denominator <= 0.0f) {
            /* If denominator is zero/negative, treat as extremely slow. */
            continue;
        }

        float operate_time = t_mult * (a / denominator);

        /* Apply optional min/max clamps if configured. */
        if (t_min > 0.0f && operate_time < t_min) {
            operate_time = t_min;
        }
        if (t_max > 0.0f && operate_time > t_max) {
            operate_time = t_max;
        }

        if (operate_time <= 0.0f) {
            /* Guard: avoid division by zero. */
            continue;
        }

        /* Increment normalized progress and check for trip. */
        *progress += dt_seconds / operate_time;

        if (*progress >= 1.0f) {
            output.trip_mask |= phase_mask;
            /* Optionally latch progress at 1.0f; or leave it >1.0f. */
            *progress = 1.0f;
        }
    }

    return output;
}
