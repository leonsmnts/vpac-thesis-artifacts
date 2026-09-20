#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h> // for mlockall()

#include "../timing.h"                  /* perf_timer_* */

#include "include/meas_common.h"
#include "include/meas_sv.h"
#include "../protection/include/prot_common.h"

#define AMOUNT_RUNS_PER_BENCHMARK 2000

#if defined(__GNUC__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

static volatile double g_warmup_sink;

/* Shared measurement context and frame for the benchmark. */
static meas_sv_context_t       g_meas_ctx;
static sv_payload_t            g_payload;
static vpac_measurement_frame_t g_measurement_frame;

/* Common sample period (microseconds) */
static const uint32_t g_dt_microseconds = 250;

/* -------------------- Measurement benchmark setup -------------------- */

static void
initialize_meas_benchmark(void)
{
    /* Initialize measurement context (buffers + DFT coefficients). */
    meas_sv_context_init(&g_meas_ctx);

    /* Initialize a deterministic SV payload.
     *
     * For timing, actual values are not critical as long as the path
     * exercises:
     *   - buffer updates,
     *   - one-cycle DFT,
     *   - magnitude/angle computation,
     *   - local/remote complex current filling.
     *
     * We use simple non-zero values for all channels.
     */

    g_payload.frame_id = 0;
    g_payload.sender_timestamp_ns = 0;

    /* Local currents: Ia, Ib, Ic, In (amps). */
    g_payload.local_current[0] = 100.0f;
    g_payload.local_current[1] = 110.0f;
    g_payload.local_current[2] = 120.0f;
    g_payload.local_current[3] =  50.0f;

    /* Local voltages: Va, Vb, Vc, Vn (volts). */
    g_payload.local_voltage[0] = 10.0f;
    g_payload.local_voltage[1] = 10.0f;
    g_payload.local_voltage[2] = 10.0f;
    g_payload.local_voltage[3] =  0.0f;

    /* Remote currents: simple scaled values for differential testing. */
    g_payload.remote_current[0] =  80.0f;
    g_payload.remote_current[1] =  90.0f;
    g_payload.remote_current[2] = 100.0f;
    g_payload.remote_current[3] =   0.0f;

    /* Initialize measurement frame to zero; it will be filled by
     * meas_sv_process_sample().
     */
    for (int i = 0; i < 4; ++i) {
        g_measurement_frame.current_magnitude[i] = 0.0f;
        g_measurement_frame.current_angle[i]     = 0.0f;
        g_measurement_frame.voltage_magnitude[i] = 0.0f;
        g_measurement_frame.voltage_angle[i]     = 0.0f;
    }

    for (int phase = 0; phase < 3; ++phase) {
        g_measurement_frame.local_current[phase].real  = 0.0f;
        g_measurement_frame.local_current[phase].imag  = 0.0f;
        g_measurement_frame.remote_current[phase].real = 0.0f;
        g_measurement_frame.remote_current[phase].imag = 0.0f;
    }

    /* Fill the buffers with 79 samples to immediately allow a DFT computation on the next sample. */
    for (int sample_idx = 0; sample_idx < MEAS_SAMPLES_PER_CYCLE - 1; ++sample_idx) {
        meas_sv_process_sample(&g_meas_ctx,
                               &g_payload,
                               &g_measurement_frame);
    }
}

/* Signature must match: double (*perf_timer_func_t)(void). */
static NOINLINE double
benchmark_meas_sv_step(void)
{
    float acc = 0.0f;

    for (int i = 0; i < AMOUNT_RUNS_PER_BENCHMARK; ++i) {
        /* We ignore dt inside meas_sv_process_sample for now; it operates
        * purely on sample buffers and payload content.
        */
        meas_sv_process_sample(&g_meas_ctx,
                               &g_payload,
                               &g_measurement_frame);
        
        acc += g_measurement_frame.current_magnitude[0];
    }

    return (double)acc;
}

/* -------------------- Benchmark descriptor and driver -------------------- */

typedef struct {
    const char        *name;          /* human-readable label */
    const char        *csv_filename;  /* where to write CSV */
    perf_timer_func_t  func;          /* function to benchmark */
    void             (*init_fn)(void);/* function to initialize context/state */
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

    const int default_sample_count = 30000;

    const benchmark_case_t benchmarks[] = {
        {
            .name         = "Measurement layer: SV -> frame",
            .csv_filename = "meas_sv_benchmark.csv",
            .func         = benchmark_meas_sv_step,
            .init_fn      = initialize_meas_benchmark
        }
    };

    const size_t benchmark_count = sizeof(benchmarks) / sizeof(benchmarks[0]);

    perf_timer_t *timers[benchmark_count];
    for (size_t i = 0; i < benchmark_count; ++i) {
        timers[i] = NULL;
    }

    for (size_t i = 0; i < benchmark_count; ++i) {
        const benchmark_case_t *b = &benchmarks[i];

        /* Initialize measurement context and payload. */
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
