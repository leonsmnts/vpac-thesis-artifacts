/*
 * ANSI 67 - directional overcurrent
 *
 * Concept:
 *   - Uses current magnitude AND a directional check relative to a
 *     polarizing voltage angle.
 *   - Only trips when current is above pickup AND direction matches
 *     the configured mode (forward / reverse).
 */

#ifndef PROT_DIR67_H
#define PROT_DIR67_H

#include <stdint.h>
#include "prot_common.h"


typedef enum {
    DIR67_MODE_FORWARD = 0,
    DIR67_MODE_REVERSE = 1,
    DIR67_MODE_NON_DIRECTIONAL = 2  /* effectively ANSI 50/51 */
} dir67_mode_t;

typedef struct {
    /* Pickup thresholds per phase (A, B, C, N). */
    float pickup_current[4];

    /* Directional mode (forward/reverse/non-directional). */
    dir67_mode_t mode;

    /* Torque / reference angle (radians or degrees; consistent with frame angles).
     * For simple implementation we interpret "forward" as:
     *    angle(current) - angle(voltage) near torque_angle
     * and "reverse" as near torque_angle +/- 180 degrees.
     */
    float torque_angle;

    /* Allowed deviation around the ideal torque angle (angular window). */
    float angle_tolerance;

    /* Minimum polarizing voltage magnitude required for a valid directional decision. */
    float minimum_polarizing_voltage;

    /* Definite-time delay for trip after directional pickup (seconds). */
    float operate_delay_seconds;

    /* Which phases are enabled (bit mask). */
    uint32_t enabled_phase_mask;
} dir67_settings_t;

typedef struct {
    /* Per-phase accumulated time above pickup and in correct direction (seconds). */
    float accumulated_time_seconds[4];
} dir67_state_t;

/* Initialize ANSI 67 state. */
void dir67_init(dir67_state_t *state);

/*
 * Execute one ANSI 67 step.
 *
 * Parameters:
 *   settings          - configuration (pickup, direction, torque, delay, etc.).
 *   state             - per-phase accumulated time.
 *   frame             - measurement values (currents, voltages, angles).
 *   dt_microseconds   - elapsed time since previous call (microseconds).
 *
 * Returns:
 *   protection_output_t with:
 *     - start_mask: phases where current >= pickup AND direction is valid.
 *     - trip_mask:  phases where accumulated_time >= operate_delay_seconds.
 *     - block_mask: currently unused (0); can later represent directional blocking.
 */
protection_output_t
dir67_step(const dir67_settings_t        *settings,
           dir67_state_t                 *state,
           const vpac_measurement_frame_t *frame,
           uint32_t                       dt_microseconds);

#endif /* PROT_DIR67_H */
