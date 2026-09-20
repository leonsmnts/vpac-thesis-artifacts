/*
 * Differential protection (ANSI 87).
 *
 * Simplified percentage differential model:
 *   - For each phase, compute:
 *       operate_current   = |I_local - I_remote|
 *       restraint_current = 0.5 * (|I_local| + |I_remote|)
 *   - Use a percentage/bias characteristic:
 *       if restraint_current < minimum_restraint_current:
 *           trip when operate_current >= pickup_operate_current
 *       else:
 *           trip when operate_current >= pickup_operate_current
 *                                 + bias_slope * restraint_current
 *   - Optional angle restraint: require |angle(I_local) - angle(I_remote)|
 *     within angle_tolerance to avoid tripping on bad CT behavior.
 *   - Definite-time delay once operate criterion is met.
 */


#ifndef PROT_DIFF87_H
#define PROT_DIFF87_H

#include <stdint.h>
#include <math.h>

#include "prot_common.h"


/* Compute magnitude of a complex current. */
static inline float
current_magnitude_from_complex(vpac_complex_float_t c)
{
    return sqrtf(c.real * c.real + c.imag * c.imag);
}

/* Compute angle of a complex current (radians). */
static inline float
current_angle_from_complex(vpac_complex_float_t c)
{
    return atan2f(c.imag, c.real);
}


/* Differential settings per element (3 phases; neutral usually not used). */
typedef struct {
    /* Basic operate pickup threshold (amps). */
    float pickup_operate_current;

    /* Minimum restraint current below which pure differential pickup is used. */
    float minimum_restraint_current;

    /* Percentage bias slope (dimensionless). Typical values are small (e.g., 0.3). */
    float bias_slope;

    /* Optional angle restraint between local and remote currents (radians). */
    float angle_tolerance;

    /* Minimum current magnitude at either end required for a valid decision. */
    float minimum_terminal_current;

    /* Definite-time delay once operate condition is satisfied (seconds). */
    float operate_delay_seconds;

    /* Which phases are enabled (bit mask; normally A, B, C). */
    uint32_t enabled_phase_mask;
} diff87_settings_t;

/* Differential state: per-phase operate timers. */
typedef struct {
    /* Per-phase accumulated time while in operate region (seconds). */
    float accumulated_time_seconds[3];  /* phases A, B, C */
} diff87_state_t;

/* Initialize ANSI 87 state. */
void diff87_init(diff87_state_t *state);

/*
 * Execute one ANSI 87 step.
 *
 * Parameters:
 *   settings          - configuration (pickup, bias, angle restraint, delay).
 *   state             - per-phase accumulated operate time.
 *   frame             - measurement values including local/remote phasors.
 *   dt_microseconds   - elapsed time since previous call (microseconds).
 *
 * Returns:
 *   protection_output_t with:
 *     - start_mask: phases where operate/bias condition is met.
 *     - trip_mask:  phases where accumulated_time >= operate_delay_seconds.
 *     - block_mask: currently unused (0).
 */
protection_output_t
diff87_step(const diff87_settings_t        *settings,
            diff87_state_t                 *state,
            const vpac_measurement_frame_t *frame,
            uint32_t                        dt_microseconds);

#endif /* PROT_DIFF87_H */