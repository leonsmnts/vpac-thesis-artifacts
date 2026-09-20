/*
 * Distance protection (ANSI 21).
 *
 * Simplified model:
 *   - For each phase, compute apparent impedance Z = V / I from
 *     voltage/current magnitudes and angles.
 *   - Check if |Z| is within a scalar "reach" setting.
 *   - Optionally apply a directional window relative to line_angle.
 *   - Apply a definite-time operating delay once in zone.
 */

#ifndef PROT_DIST21_H
#define PROT_DIST21_H

#include <stdint.h>
#include "prot_common.h"



typedef enum {
    DIST21_MODE_FORWARD = 0,
    DIST21_MODE_REVERSE = 1,
    DIST21_MODE_NON_DIRECTIONAL = 2
} dist21_mode_t;

typedef struct {
    /* Per-phase reach settings (ohms or per-unit). */
    float reach_ohms[4];  /* phase A, B, C, neutral */

    /* Line angle (radians), e.g. angle of line impedance. */
    float line_angle;

    /* Directional mode: forward/reverse/non-directional. */
    dist21_mode_t mode;

    /* Allowed angular deviation around the ideal line angle (radians). */
    float angle_tolerance;

    /* Minimum voltage/current magnitudes required for a valid decision. */
    float minimum_voltage_magnitude;
    float minimum_current_magnitude;

    /* Definite-time operate delay once in zone (seconds). */
    float operate_delay_seconds;

    /* Which phases are enabled (bit mask). */
    uint32_t enabled_phase_mask;
} dist21_settings_t;

typedef struct {
    /* Per-phase accumulated time in zone and (if applicable) correct direction (seconds). */
    float accumulated_time_seconds[4];
} dist21_state_t;

/* Initialize ANSI 21 state. */
void dist21_init(dist21_state_t *state);

/*
 * Execute one ANSI 21 step.
 *
 * Parameters:
 *   settings          - configuration (reach, line angle, direction, delay, etc.).
 *   state             - per-phase accumulated time.
 *   frame             - measurement values (currents/voltages magnitudes and angles).
 *   dt_microseconds   - elapsed time since previous call (microseconds).
 *
 * Returns:
 *   protection_output_t with:
 *     - start_mask: phases where Z is in-zone (and direction valid).
 *     - trip_mask:  phases where accumulated_time >= operate_delay_seconds.
 *     - block_mask: currently unused (0).
 */
protection_output_t
dist21_step(const dist21_settings_t        *settings,
            dist21_state_t                 *state,
            const vpac_measurement_frame_t *frame,
            uint32_t                        dt_microseconds);

#endif /* PROT_DIST21_H */
