#include "include/prot_dispatch.h"

/* Global settings/state for each ANSI function.
 * In a real system these could be embedded in a struct or configured from files.
 */
static oc50_settings_t     g_oc50_settings;
static oc50_state_t        g_oc50_state;

static oc51_dtl_settings_t g_oc51_dtl_settings;
static oc51_dtl_state_t    g_oc51_dtl_state;

static oc51_idmt_settings_t g_oc51_idmt_settings;
static oc51_idmt_state_t    g_oc51_idmt_state;

static dir67_settings_t    g_dir67_settings;
static dir67_state_t       g_dir67_state;

static dist21_settings_t   g_dist21_settings;
static dist21_state_t      g_dist21_state;

static diff87_settings_t   g_diff87_settings;
static diff87_state_t      g_diff87_state;


/* Common dt for now; in a real system one can pass actual elapsed time. */
static uint32_t g_default_dt_microseconds = 250;


void
prot_dispatch_init(void)
{
    /* ANSI 50 */
    g_oc50_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK |
        PROT_PHASE_N_MASK;

    g_oc50_settings.pickup_current[0] = 100.0f;
    g_oc50_settings.pickup_current[1] = 100.0f;
    g_oc50_settings.pickup_current[2] = 100.0f;
    g_oc50_settings.pickup_current[3] =  50.0f;

    oc50_init(&g_oc50_state);

    /* ANSI 51 DTL */
    g_oc51_dtl_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK |
        PROT_PHASE_N_MASK;

    g_oc51_dtl_settings.pickup_current[0] = 100.0f;
    g_oc51_dtl_settings.pickup_current[1] = 100.0f;
    g_oc51_dtl_settings.pickup_current[2] = 100.0f;
    g_oc51_dtl_settings.pickup_current[3] =  50.0f;

    g_oc51_dtl_settings.operate_delay_seconds = 0.020f; /* 20 ms */

    oc51_dtl_init(&g_oc51_dtl_state);

    /* ANSI 51 IDMT */
    g_oc51_idmt_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK |
        PROT_PHASE_N_MASK;

    g_oc51_idmt_settings.pickup_current[0] = 100.0f;
    g_oc51_idmt_settings.pickup_current[1] = 100.0f;
    g_oc51_idmt_settings.pickup_current[2] = 100.0f;
    g_oc51_idmt_settings.pickup_current[3] =  50.0f;

    /* Standard inverse time (IEC SIT) curve parameters */
    /* https://documentation.deif.com/r/agc-150-ats-designers-handbook-4189341346-uk/ac-protections/common-protections/inverse-time-over-current-ansi-51 */
    g_oc51_idmt_settings.time_multiplier              = 1.0f;
    g_oc51_idmt_settings.curve_constant_a             = 0.14f;
    g_oc51_idmt_settings.curve_exponent_p             = 0.02f;
    g_oc51_idmt_settings.minimum_operate_time_seconds = 0.010f;
    g_oc51_idmt_settings.maximum_operate_time_seconds = 1.000f;

    oc51_idmt_init(&g_oc51_idmt_state);

    /* ANSI 67 */
    g_dir67_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK;

    g_dir67_settings.pickup_current[0] = 100.0f;
    g_dir67_settings.pickup_current[1] = 100.0f;
    g_dir67_settings.pickup_current[2] = 100.0f;
    g_dir67_settings.pickup_current[3] =  0.0f;  /* neutral unused */

    g_dir67_settings.mode                    = DIR67_MODE_FORWARD;
    g_dir67_settings.torque_angle            = 0.0f;            /* radians */
    g_dir67_settings.angle_tolerance         = 0.34906585f;     /* ~20° */
    g_dir67_settings.minimum_polarizing_voltage = 10.0f;
    g_dir67_settings.operate_delay_seconds   = 0.020f;

    dir67_init(&g_dir67_state);

    /* ANSI 21 */
    g_dist21_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK;

    g_dist21_settings.reach_ohms[0] = 10.0f;
    g_dist21_settings.reach_ohms[1] = 10.0f;
    g_dist21_settings.reach_ohms[2] = 10.0f;
    g_dist21_settings.reach_ohms[3] =  0.0f;  /* neutral unused */

    g_dist21_settings.line_angle              = 0.0f;
    g_dist21_settings.mode                    = DIST21_MODE_FORWARD;
    g_dist21_settings.angle_tolerance         = 0.34906585f;    /* ~20° */
    g_dist21_settings.minimum_voltage_magnitude = 10.0f;
    g_dist21_settings.minimum_current_magnitude = 1.0f;
    g_dist21_settings.operate_delay_seconds   = 0.020f;

    dist21_init(&g_dist21_state);

    /* ANSI 87 */
    g_diff87_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK;

    g_diff87_settings.pickup_operate_current    = 10.0f;
    g_diff87_settings.minimum_restraint_current = 5.0f;
    g_diff87_settings.bias_slope               = 0.3f;
    g_diff87_settings.angle_tolerance          = 0.52359878f;   /* ~30° */
    g_diff87_settings.minimum_terminal_current = 1.0f;
    g_diff87_settings.operate_delay_seconds    = 0.020f;

    diff87_init(&g_diff87_state);
}

void
prot_dispatch_all(const vpac_measurement_frame_t *frame,
                  uint32_t                        dt_microseconds,
                  protection_output_t             *out)
{
    if (!frame || !out) {
        return;
    }

    protection_output_t combined = {
        .start_mask = 0u,
        .trip_mask  = 0u,
        .block_mask = 0u
    };

    uint32_t dt = (dt_microseconds > 0) ? dt_microseconds : g_default_dt_microseconds;

    /* ANSI 50 */
    {
        protection_output_t r = oc50_step(&g_oc50_settings,
                                          &g_oc50_state,
                                          frame,
                                          dt);
        combined.start_mask |= r.start_mask;
        combined.trip_mask  |= r.trip_mask;
        combined.block_mask |= r.block_mask;
    }

    /* ANSI 51 DTL */
    {
        protection_output_t r = oc51_dtl_step(&g_oc51_dtl_settings,
                                              &g_oc51_dtl_state,
                                              frame,
                                              dt);
        combined.start_mask |= r.start_mask;
        combined.trip_mask  |= r.trip_mask;
        combined.block_mask |= r.block_mask;
    }

    /* ANSI 51 IDMT */
    {
        protection_output_t r = oc51_idmt_step(&g_oc51_idmt_settings,
                                               &g_oc51_idmt_state,
                                               frame,
                                               dt);
        combined.start_mask |= r.start_mask;
        combined.trip_mask  |= r.trip_mask;
        combined.block_mask |= r.block_mask;
    }

    /* ANSI 67 */
    {
        protection_output_t r = dir67_step(&g_dir67_settings,
                                           &g_dir67_state,
                                           frame,
                                           dt);
        combined.start_mask |= r.start_mask;
        combined.trip_mask  |= r.trip_mask;
        combined.block_mask |= r.block_mask;
    }

    /* ANSI 21 */
    {
        protection_output_t r = dist21_step(&g_dist21_settings,
                                            &g_dist21_state,
                                            frame,
                                            dt);
        combined.start_mask |= r.start_mask;
        combined.trip_mask  |= r.trip_mask;
        combined.block_mask |= r.block_mask;
    }

    /* ANSI 87 */
    {
        protection_output_t r = diff87_step(&g_diff87_settings,
                                            &g_diff87_state,
                                            frame,
                                            dt);
        combined.start_mask |= r.start_mask;
        combined.trip_mask  |= r.trip_mask;
        combined.block_mask |= r.block_mask;
    }

    *out = combined;
}
