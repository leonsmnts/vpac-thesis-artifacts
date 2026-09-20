#ifndef MEAS_SV_H
#define MEAS_SV_H

#include <stddef.h>
#include <stdint.h>

#include "meas_common.h"
#include "../../protection/include/prot_common.h"

/*
 * Measurement context for SV-based phasor estimation.
 *
 * We use a one-cycle window (80 samples) for each channel at 80 samples/cycle:
 *   - current_buffers[4]: Ia, Ib, Ic, In local currents
 *   - voltage_buffers[4]: Va, Vb, Vc, Vn local voltages
 *
 * Remote currents are, for now, treated as instantaneous phasors
 * (no windowing), which is sufficient to exercise ANSI 87 in the testbed.
 */

#define MEAS_SAMPLES_PER_CYCLE 80

typedef struct {
    float samples[MEAS_SAMPLES_PER_CYCLE];
    int   index;   /* current write position */
    int   count;   /* how many samples have been filled (up to MEAS_SAMPLES_PER_CYCLE) */
} meas_channel_buffer_t;

typedef struct {
    meas_channel_buffer_t current_buffers[4]; /* Ia, Ib, Ic, In (local) */
    meas_channel_buffer_t voltage_buffers[4]; /* Va, Vb, Vc, Vn (local) */

    /* Precomputed DFT basis coefficients for k = 0..N-1. */
    float dft_cos[MEAS_SAMPLES_PER_CYCLE];
    float dft_sin[MEAS_SAMPLES_PER_CYCLE];
} meas_sv_context_t;

/*
 * Initialize SV measurement context.
 */
void meas_sv_context_init(meas_sv_context_t *ctx);

/*
 * Process one SV payload:
 *   - push new samples into per-channel buffers
 *   - when enough samples are available, compute phasors
 *   - fill vpac_measurement_frame_t with magnitudes, angles, and
 *     complex local/remote currents for use by protection functions.
 *
 * Parameters:
 *   ctx         - measurement context (buffers, indices)
 *   payload     - SV payload (local + remote currents, local voltages)
 *   out_frame   - measurement frame to be filled for protection layer
 *
 * Notes:
 *   - Phasors are computed over MEAS_SAMPLES_PER_CYCLE samples.
 *   - Before buffers are full, magnitudes/angles are based on instantaneous samples.
 */
void meas_sv_process_sample(meas_sv_context_t       *ctx,
                            const sv_payload_t      *payload,
                            vpac_measurement_frame_t *out_frame);

#endif /* MEAS_SV_H */
