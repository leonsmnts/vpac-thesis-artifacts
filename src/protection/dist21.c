#include <stddef.h>   // for NULL
#include <math.h>

#include "include/dist21.h"


/* Check direction for one phase, given apparent impedance angle. */
static int
direction_is_valid(const dist21_settings_t  *settings,
                   float                     impedance_angle)
{
    /* Normalize angles into [-pi, +pi] range. */
    float line_angle      = normalize_angle(settings->line_angle);
    float angle_tolerance = settings->angle_tolerance;

    if (settings->mode == DIST21_MODE_FORWARD) {
        float diff = fabsf(normalize_angle(impedance_angle - line_angle));
        return (diff <= angle_tolerance) ? 1 : 0;
    } else if (settings->mode == DIST21_MODE_REVERSE) {
        float reverse_angle = normalize_angle(line_angle + (float)M_PI);
        float diff          = fabsf(normalize_angle(impedance_angle - reverse_angle));
        return (diff <= angle_tolerance) ? 1 : 0;
    } else {
        /* Non-directional: always valid. */
        return 1;
    }
}

void
dist21_init(dist21_state_t *state)
{
    if (state == NULL) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        state->accumulated_time_seconds[i] = 0.0f;
    }
}

protection_output_t
dist21_step(const dist21_settings_t         *settings,
            dist21_state_t                  *state,
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
    const float min_vmagnitude  = settings->minimum_voltage_magnitude;
    const float min_imagnitude  = settings->minimum_current_magnitude;

    for (int phase_index = 0; phase_index < 4; ++phase_index) {
        uint32_t phase_mask = prot_phase_index_to_mask(phase_index);

        if ((settings->enabled_phase_mask & phase_mask) == 0u) {
            continue;
        }

        float v_mag = frame->voltage_magnitude[phase_index];
        float i_mag = frame->current_magnitude[phase_index];

        /* Require minimum voltage/current for a valid impedance calculation. */
        if (v_mag < min_vmagnitude || i_mag < min_imagnitude || i_mag <= 0.0f) {
            state->accumulated_time_seconds[phase_index] = 0.0f;
            continue;
        }

        /* Apparent impedance magnitude: |Z| = |V| / |I|. */
        float z_mag = v_mag / i_mag;

        /* Simple scalar reach check: inside zone if |Z| <= reach. */
        float reach = settings->reach_ohms[phase_index];
        if (reach <= 0.0f || z_mag > reach) {
            /* Out of zone: reset time and skip. */
            state->accumulated_time_seconds[phase_index] = 0.0f;
            continue;
        }

        /* Apparent impedance angle: angle(Z) = angle(V) - angle(I). */
        float v_angle = frame->voltage_angle[phase_index];
        float i_angle = frame->current_angle[phase_index];
        float z_angle = normalize_angle(v_angle - i_angle);

        /* Directional supervision if configured. */
        int dir_ok = direction_is_valid(settings, z_angle);
        if (!dir_ok) {
            state->accumulated_time_seconds[phase_index] = 0.0f;
            continue;
        }

        /* In zone and direction valid: element has started. */
        output.start_mask |= phase_mask;

        if (operate_delay > 0.0f) {
            float *accumulated_time = &state->accumulated_time_seconds[phase_index];
            *accumulated_time += dt_seconds;

            if (*accumulated_time >= operate_delay) {
                output.trip_mask |= phase_mask;
                /* Optional: Latch at delay for clarity. */
                // *accumulated_time = operate_delay;
            }
        } else {
            /* No delay configured: instantaneous distance trip. */
            output.trip_mask |= phase_mask;
        }
    }

    return output;
}
