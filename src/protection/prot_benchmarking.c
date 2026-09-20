#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h> // for mlockall()

#include "../timing.h"           // perf_timer_*

#include "include/prot_common.h"
#include "include/oc50.h"
#include "include/oc51_dtl.h"
#include "include/oc51_idmt.h"
#include "include/dir67.h"
#include "include/dist21.h"
#include "include/diff87.h"

#define AMOUNT_RUNS_PER_BENCHMARK 20000

#if defined(__GNUC__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

static inline uint32_t
checksum_output(protection_output_t out)
{
    return out.start_mask ^
           (out.trip_mask << 8) ^
           (out.block_mask << 16);
}

static volatile double g_warmup_sink;

/* Shared measurement frame for all benchmarks */
static vpac_measurement_frame_t g_measurement_frame;

/* ANSI 50 context */
static oc50_settings_t  g_oc50_settings;
static oc50_state_t     g_oc50_state;

/* ANSI 51 DTL context */
static oc51_dtl_settings_t g_oc51_dtl_settings;
static oc51_dtl_state_t    g_oc51_dtl_state;

/* ANSI 51 IDMT context */
static oc51_idmt_settings_t g_oc51_idmt_settings;
static oc51_idmt_state_t    g_oc51_idmt_state;

/* ANSI 67 directional overcurrent context */
static dir67_settings_t g_dir67_settings;
static dir67_state_t    g_dir67_state;

/* ANSI 21 distance protection context */
static dist21_settings_t g_dist21_settings;
static dist21_state_t    g_dist21_state;

/* ANSI 87 differential protection context */
static diff87_settings_t g_diff87_settings;
static diff87_state_t    g_diff87_state;

/* Common sample period for all benchmarks (microseconds) */
static const uint32_t g_dt_microseconds = 250;

/* -------------------- Measurement frame setup -------------------- */

static void
initialize_measurement_frame_for_overcurrent(const float *pickup_current)
{
    /* Initialize a deterministic measurement frame.
     * Here we set all phase currents above pickup so we always “see” the element.
     */
    for (int i = 0; i < 4; ++i) {
        g_measurement_frame.current_magnitude[i] = 1.5f * pickup_current[i];
        g_measurement_frame.voltage_magnitude[i] = 0.0f;
        g_measurement_frame.current_angle[i]     = 0.0f;
        g_measurement_frame.voltage_angle[i]     = 0.0f;
    }

    for (int i = 0; i < 3; ++i) {
        g_measurement_frame.local_current[i].real  = 0.0f;
        g_measurement_frame.local_current[i].imag  = 0.0f;
        g_measurement_frame.remote_current[i].real = 0.0f;
        g_measurement_frame.remote_current[i].imag = 0.0f;
    }
}



/* -------------------- ANSI 50 benchmark setup -------------------- */

static void
initialize_oc50_benchmark(void)
{
    /* Enable all four phases (A, B, C, N). */
    g_oc50_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK |
        PROT_PHASE_N_MASK;

    /* Set pickup thresholds for each phase (arbitrary test values). */
    g_oc50_settings.pickup_current[0] = 100.0f;  /* phase A */
    g_oc50_settings.pickup_current[1] = 100.0f;  /* phase B */
    g_oc50_settings.pickup_current[2] = 100.0f;  /* phase C */
    g_oc50_settings.pickup_current[3] =  50.0f;  /* neutral */

    oc50_init(&g_oc50_state);

    initialize_measurement_frame_for_overcurrent(g_oc50_settings.pickup_current);
}

/* Signature must match: double (*perf_timer_func_t)(void). */
static NOINLINE double
benchmark_oc50_step(void)
{
    uint32_t acc = 0;

    for (int i = 0; i < AMOUNT_RUNS_PER_BENCHMARK; ++i) {
        protection_output_t out = oc50_step(&g_oc50_settings,
                                           &g_oc50_state,
                                           &g_measurement_frame,
                                           g_dt_microseconds);
        acc ^= checksum_output(out);
    }
    return (double)acc;
}

/* -------------------- ANSI 51 DTL benchmark setup -------------------- */

static void
initialize_oc51_dtl_benchmark(void)
{
    /* Enable all four phases. */
    g_oc51_dtl_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK |
        PROT_PHASE_N_MASK;

    /* Reuse the same pickup values as for 50 for comparability. */
    g_oc51_dtl_settings.pickup_current[0] = 100.0f;
    g_oc51_dtl_settings.pickup_current[1] = 100.0f;
    g_oc51_dtl_settings.pickup_current[2] = 100.0f;
    g_oc51_dtl_settings.pickup_current[3] =  50.0f;

    /* Choose some definite-time delay (seconds). For benchmarking, the actual
     * value does not matter much; we just want the logic exercised.
     * 
     * With delay we don't have "deterministic" execution cost,
     * since path taken depends on this state
     */
    g_oc51_dtl_settings.operate_delay_seconds = 0.0f;

    oc51_dtl_init(&g_oc51_dtl_state);

    initialize_measurement_frame_for_overcurrent(g_oc51_dtl_settings.pickup_current);
}

static NOINLINE double
benchmark_oc51_dtl_step(void)
{
    uint32_t acc = 0;

    for (int i = 0; i < AMOUNT_RUNS_PER_BENCHMARK; ++i) {
        protection_output_t out = oc51_dtl_step(&g_oc51_dtl_settings,
                                               &g_oc51_dtl_state,
                                               &g_measurement_frame,
                                               g_dt_microseconds);
        acc ^= checksum_output(out);
    }
    return (double)acc;
}


/* -------------------- ANSI 51 IDMT benchmark setup -------------------- */

static void
initialize_oc51_idmt_benchmark(void)
{
    /* Enable all four phases. */
    g_oc51_idmt_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK |
        PROT_PHASE_N_MASK;

    /* Reuse the same pickup values as for 50 for comparability. */
    g_oc51_idmt_settings.pickup_current[0] = 100.0f;
    g_oc51_idmt_settings.pickup_current[1] = 100.0f;
    g_oc51_idmt_settings.pickup_current[2] = 100.0f;
    g_oc51_idmt_settings.pickup_current[3] =  50.0f;

    /* Set IEC/IEEE curve parameters. */
    /* Standard inverse time (IEC SIT) curve parameters */
    /* https://productinfo.se.com/micrologicxuserguide/doca0102-micrologic-x/English/BM_MasterPact%20MTZ%20MicroLogic%20X_b5effd44_T001599214.xml/$/TPC_IDMTLProtection_b5effd44_T001599896#:~:text=%CE%B1-,SIT,0.02,-VIT */
    /* https://documentation.deif.com/r/agc-150-ats-designers-handbook-4189341346-uk/ac-protections/common-protections/inverse-time-over-current-ansi-51 */
    g_oc51_idmt_settings.curve_constant_a = 0.14f;
    g_oc51_idmt_settings.curve_exponent_p = 0.02f;
    g_oc51_idmt_settings.time_multiplier = 1.0f;

    /* Set operate time clamps (seconds). */
    g_oc51_idmt_settings.minimum_operate_time_seconds = 0.0f;
    g_oc51_idmt_settings.maximum_operate_time_seconds = 1.0f;

    oc51_idmt_init(&g_oc51_idmt_state);

    initialize_measurement_frame_for_overcurrent(g_oc51_idmt_settings.pickup_current);
}

static NOINLINE double
benchmark_oc51_idmt_step(void)
{
    uint32_t acc = 0;

    for (int i = 0; i < AMOUNT_RUNS_PER_BENCHMARK; ++i) {
        protection_output_t out = oc51_idmt_step(&g_oc51_idmt_settings,
                                                 &g_oc51_idmt_state,
                                                 &g_measurement_frame,
                                                 g_dt_microseconds);
        acc ^= checksum_output(out);
    }
    return (double)acc;
}


/* -------------------- ANSI 67 benchmark setup -------------------- */

static void
initialize_dir67_benchmark(void)
{
    /* Enable phases A, B, C (skip neutral, typical for phase 67). */
    g_dir67_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK;

    /* Pickup similar to 50/51 for comparison. */
    g_dir67_settings.pickup_current[0] = 100.0f;
    g_dir67_settings.pickup_current[1] = 100.0f;
    g_dir67_settings.pickup_current[2] = 100.0f;
    g_dir67_settings.pickup_current[3] =  0.0f;  /* neutral unused */

    /* Directional configuration: forward, torque angle around 0,
     * tolerance of, say, 20 degrees (~0.349 rad).
     */
    g_dir67_settings.mode                = DIR67_MODE_FORWARD;
    g_dir67_settings.torque_angle        = 0.0f;                /* radians */
    g_dir67_settings.angle_tolerance     = 0.34906585f;         /* ~20° */
    g_dir67_settings.minimum_polarizing_voltage = 10.0f;        /* arbitrary */

    g_dir67_settings.operate_delay_seconds = 0.0f;

    dir67_init(&g_dir67_state);

    /* For benchmarking, we can reuse the same pattern as overcurrent:
     * currents above pickup, voltages non-zero with simple angles.
     */
    for (int i = 0; i < 4; ++i) {
        g_measurement_frame.current_magnitude[i] = 1.5f * g_dir67_settings.pickup_current[i];
        g_measurement_frame.voltage_magnitude[i] = 100.0f;  /* arbitrary non-zero */
        g_measurement_frame.current_angle[i]     = 0.0f;    /* aligned with voltage */
        g_measurement_frame.voltage_angle[i]     = 0.0f;
    }

    for (int i = 0; i < 3; ++i) {
        g_measurement_frame.local_current[i].real  = 0.0f;
        g_measurement_frame.local_current[i].imag  = 0.0f;
        g_measurement_frame.remote_current[i].real = 0.0f;
        g_measurement_frame.remote_current[i].imag = 0.0f;
    }
}

static NOINLINE double
benchmark_dir67_step(void)
{
    uint32_t acc = 0;

    for (int i = 0; i < AMOUNT_RUNS_PER_BENCHMARK; ++i) {
        protection_output_t out = dir67_step(&g_dir67_settings,
                                             &g_dir67_state,
                                             &g_measurement_frame,
                                             g_dt_microseconds);
        acc ^= checksum_output(out);
    }
    return (double)acc;
}


/* -------------------- ANSI 21 benchmark setup -------------------- */

static void
initialize_dist21_benchmark(void)
{
    /* Enable phases A, B, C (skip neutral). */
    g_dist21_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK;

    /* Simple reach settings per phase (ohms or p.u.); arbitrary here. */
    g_dist21_settings.reach_ohms[0] = 10.0f;
    g_dist21_settings.reach_ohms[1] = 10.0f;
    g_dist21_settings.reach_ohms[2] = 10.0f;
    g_dist21_settings.reach_ohms[3] =  0.0f;  /* neutral unused */

    /* Line angle (radians); e.g. 0 for a purely resistive test case. */
    g_dist21_settings.line_angle    = 0.0f;
    g_dist21_settings.mode          = DIST21_MODE_FORWARD;
    g_dist21_settings.angle_tolerance = 0.34906585f;     /* ~20° in radians */

    g_dist21_settings.minimum_voltage_magnitude = 10.0f;
    g_dist21_settings.minimum_current_magnitude = 1.0f;

    g_dist21_settings.operate_delay_seconds = 0.0f;

    dist21_init(&g_dist21_state);

    /* For benchmarking, set Z firmly inside zone with forward direction.
     * Example: |Z| = 5 (reach = 10), angle(Z) = 0 rad.
     */
    for (int i = 0; i < 4; ++i) {
        /* Choose current and voltage such that V/I = 5. */
        g_measurement_frame.current_magnitude[i] = 2.0f;   /* arbitrary */
        g_measurement_frame.voltage_magnitude[i] = 10.0f;  /* Z = 5 */

        /* Align angles with line_angle for forward mode. */
        g_measurement_frame.current_angle[i] = 0.0f;
        g_measurement_frame.voltage_angle[i] = 0.0f;
    }

    for (int i = 0; i < 3; ++i) {
        g_measurement_frame.local_current[i].real  = 0.0f;
        g_measurement_frame.local_current[i].imag  = 0.0f;
        g_measurement_frame.remote_current[i].real = 0.0f;
        g_measurement_frame.remote_current[i].imag = 0.0f;
    }
}

static NOINLINE double
benchmark_dist21_step(void)
{
    uint32_t acc = 0;

    for (int i = 0; i < AMOUNT_RUNS_PER_BENCHMARK; ++i) {
        protection_output_t out = dist21_step(&g_dist21_settings,
                                              &g_dist21_state,
                                              &g_measurement_frame,
                                              g_dt_microseconds);
        acc ^= checksum_output(out);
    }
    return (double)acc;
}


/* -------------------- ANSI 87 benchmark setup -------------------- */

static void
initialize_diff87_benchmark(void)
{
    /* Enable phases A, B, C. */
    g_diff87_settings.enabled_phase_mask =
        PROT_PHASE_A_MASK |
        PROT_PHASE_B_MASK |
        PROT_PHASE_C_MASK;

    /* Basic operate pickup, minimum restraint, bias slope. */
    g_diff87_settings.pickup_operate_current    = 10.0f;  /* amps */
    g_diff87_settings.minimum_restraint_current = 5.0f;  /* amps */
    g_diff87_settings.bias_slope                = 0.3f;   /* dimensionless */

    /* Angle restraint: allow ~30 degrees difference. */
    g_diff87_settings.angle_tolerance = 0.52359878f;     /* ~30° in radians */

    /* Minimum terminal current to consider the phase. */
    g_diff87_settings.minimum_terminal_current = 1.0f;

    /* Definite-time delay; e.g. 20 ms. */
    g_diff87_settings.operate_delay_seconds = 0.0f;

    diff87_init(&g_diff87_state);

    /* We want operate_current >= threshold for benchmarking.
     * For benchmarking, set local and remote currents such that:
     *   mag_local  = 25 A
     *   mag_remote = 5 A
     *   operate    = |25 - 5| = 20 A
     *   restraint  = 0.5 * (25 + 5) = 15 A
     *   threshold  = pickup + bias * restraint
     *              = 10 + 0.3 * 15 = 14.5 A
     */
    for (int phase = 0; phase < 3; ++phase) {
        vpac_complex_float_t i_local;
        vpac_complex_float_t i_remote;

        /* Local current: 25 A at angle 0. */
        i_local.real = 25.0f;
        i_local.imag = 0.0f;

        /* Remote current: 5 A at angle 0. */
        i_remote.real = 5.0f;
        i_remote.imag = 0.0f;

        g_measurement_frame.local_current[phase]  = i_local;
        g_measurement_frame.remote_current[phase] = i_remote;

        /* Fill scalar magnitudes/angles consistently. */
        g_measurement_frame.current_magnitude[phase] =
            current_magnitude_from_complex(i_local);
        g_measurement_frame.current_angle[phase] =
            current_angle_from_complex(i_local);

        g_measurement_frame.voltage_magnitude[phase] = 0.0f;
        g_measurement_frame.voltage_angle[phase]     = 0.0f;
    }

    /* Neutral fields can be left at zero. */
    g_measurement_frame.current_magnitude[3] = 0.0f;
    g_measurement_frame.current_angle[3]     = 0.0f;
    g_measurement_frame.voltage_magnitude[3] = 0.0f;
    g_measurement_frame.voltage_angle[3]     = 0.0f;
}

static NOINLINE double
benchmark_diff87_step(void)
{
    uint32_t acc = 0;
    for (int i = 0; i < AMOUNT_RUNS_PER_BENCHMARK; ++i) {
        protection_output_t out = diff87_step(&g_diff87_settings,
                                              &g_diff87_state,
                                              &g_measurement_frame,
                                              g_dt_microseconds);
        acc ^= checksum_output(out);
    }
    return (double)acc;
}

 
/* -------------------- Benchmark descriptor and driver -------------------- */

typedef struct {
    const char        *name;         /* human-readable label */
    const char        *csv_filename; /* where to write CSV */
    perf_timer_func_t  func;         /* function to benchmark */
    void             (*init_fn)(void); /* function to initialize context/state */
} benchmark_case_t;

int
main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Lock memory */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall failed");
        exit(EXIT_FAILURE);
    }

    const int default_sample_count = 40000;

    /* Add new ANSI functions by extending this array. */
    const benchmark_case_t benchmarks[] = {
        {
            .name         = "ANSI 50 instantaneous overcurrent",
            .csv_filename = "oc50_benchmark.csv",
            .func         = benchmark_oc50_step,
            .init_fn      = initialize_oc50_benchmark
        },
        {
            .name         = "ANSI 51 definite-time overcurrent",
            .csv_filename = "oc51_dtl_benchmark.csv",
            .func         = benchmark_oc51_dtl_step,
            .init_fn      = initialize_oc51_dtl_benchmark
        },
        {
            .name         = "ANSI 51 inverse-time overcurrent",
            .csv_filename = "oc51_idmt_benchmark.csv",
            .func         = benchmark_oc51_idmt_step,
            .init_fn      = initialize_oc51_idmt_benchmark
        },
        {
            .name         = "ANSI 67 directional overcurrent",
            .csv_filename = "dir67_benchmark.csv",
            .func         = benchmark_dir67_step,
            .init_fn      = initialize_dir67_benchmark
        },
        {
            .name         = "ANSI 21 distance protection",
            .csv_filename = "dist21_benchmark.csv",
            .func         = benchmark_dist21_step,
            .init_fn      = initialize_dist21_benchmark
        },
        {
            .name         = "ANSI 87 differential protection",
            .csv_filename = "diff87_benchmark.csv",
            .func         = benchmark_diff87_step,
            .init_fn      = initialize_diff87_benchmark
        }
    };

    const size_t benchmark_count = sizeof(benchmarks) / sizeof(benchmarks[0]);

    perf_timer_t *timers[benchmark_count];
    for (size_t i = 0; i < benchmark_count; ++i) {
        timers[i] = NULL;
    }

    for (size_t i = 0; i < benchmark_count; ++i) {
        const benchmark_case_t *b = &benchmarks[i];

        /* Initialize measurement + function-specific state. */
        if (b->init_fn) {
            b->init_fn();
        }

        perf_timer_t *timer = perf_timer_create(b->func,
                                                default_sample_count,
                                                b->csv_filename);
        if (!timer) {
            fprintf(stderr, "Failed to create perf_timer for %s\n", b->name);
            continue;
        }

        /* Run the benchmark function a few times to warm up caches and branch predictors. */
        for (int n = 0; n < 1000; ++n) {
            g_warmup_sink += b->func();
        }

        for (int n = 0; n < default_sample_count; ++n) {
            perf_timer_measure(timer);
        }

        timers[i] = timer;
    }

    /* Leave the timing-critical phase before doing stdout / disk I/O. */
    for (size_t i = 0; i < benchmark_count; ++i) {
        if (!timers[i]) {
            continue;
        }

        printf("Benchmark: %s\n", benchmarks[i].name);
        perf_timer_stats(timers[i], benchmarks[i].name);
        perf_timer_save(timers[i]);
        perf_timer_free(timers[i]);
        printf("\n");
    }

    return EXIT_SUCCESS;
}
