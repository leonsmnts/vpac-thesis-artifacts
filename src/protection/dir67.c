#include <stddef.h>   // for NULL
#include <math.h>

#include "include/dir67.h"


/* Compute directional status for one phase.
 *
 * Returns:
 *   1 if direction is valid for the configured mode,
 *   0 otherwise.
 */
static int
direction_is_valid(const dir67_settings_t         *settings,
                   const vpac_measurement_frame_t *frame,
                   int                             phase_index)
{
    if (settings->mode == DIR67_MODE_NON_DIRECTIONAL) {
        /* Non-directional: always valid (pure overcurrent). */
        return 1;
    }
    
    /* Use phase-to-neutral voltage as polarizing reference for the same phase. */
    float current_angle = frame->current_angle[phase_index];
    float voltage_angle = frame->voltage_angle[phase_index];

    float voltage_magnitude = frame->voltage_magnitude[phase_index];

    /* If polarizing voltage is too low, treat direction as invalid. */
    if (voltage_magnitude < settings->minimum_polarizing_voltage) {
        return 0;
    }

    /* Relative angle between current and voltage. */
    float relative_angle = normalize_angle(current_angle - voltage_angle);

    /* Normalize reference torque angle into [-pi, +pi]. */
    float torque_angle    = normalize_angle(settings->torque_angle);
    float angle_tolerance = settings->angle_tolerance;

    /* Forward / reverse decision using simple angular windows. */
    if (settings->mode == DIR67_MODE_FORWARD) {
        float diff = fabsf(normalize_angle(relative_angle - torque_angle));
        return (diff <= angle_tolerance) ? 1 : 0;
    } else /* if (settings->mode == DIR67_MODE_REVERSE) */ {
        /* For reverse, compare against torque_angle +/- pi. */
        float reverse_angle = normalize_angle(torque_angle + (float)M_PI);
        float diff = fabsf(normalize_angle(relative_angle - reverse_angle));
        return (diff <= angle_tolerance) ? 1 : 0;
    }
    /* Non-directional case is handled above, so we should never reach here. */
}

void
dir67_init(dir67_state_t *state)
{
    if (state == NULL) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        state->accumulated_time_seconds[i] = 0.0f;
    }
}

protection_output_t
dir67_step(const dir67_settings_t         *settings,
           dir67_state_t                  *state,
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

    const float dt_seconds = (float)dt_microseconds / 1e6f;
    const float operate_delay = settings->operate_delay_seconds;

    for (int phase_index = 0; phase_index < 4; ++phase_index) {
        uint32_t phase_mask = prot_phase_index_to_mask(phase_index);

        if ((settings->enabled_phase_mask & phase_mask) == 0u) {
            continue;
        }

        float phase_current     = frame->current_magnitude[phase_index];
        float pickup_current    = settings->pickup_current[phase_index];
        float *accumulated_time = &state->accumulated_time_seconds[phase_index];

        /* Basic pickup check. */
        if (phase_current < pickup_current || pickup_current <= 0.0f) {
            /* Below pickup: reset timer and do not start. */
            *accumulated_time = 0.0f;
            continue;
        }

        /* Directional check. */
        int dir_ok = direction_is_valid(settings, frame, phase_index);
        if (!dir_ok) {
            /* Above pickup but wrong direction: no start, timer reset. */
            *accumulated_time = 0.0f;
            continue;
        }

        /* Above pickup AND direction valid: element has started. */
        output.start_mask |= phase_mask;

        if (operate_delay > 0.0f) {
            *accumulated_time += dt_seconds;

            if (*accumulated_time >= operate_delay) {
                output.trip_mask |= phase_mask;
                /* Latch timer at operate_delay if desired. */
                //*accumulated_time = operate_delay;
            }
        } else {
            /* No definite-time delay configured: instantaneous directional trip. */
            output.trip_mask |= phase_mask;
        }
    }

    return output;
}
