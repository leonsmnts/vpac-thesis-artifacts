#include <math.h>

#include "include/meas_sv.h"


/*
 * Simple one-cycle DFT phasor estimator for a single channel.
 *
 * Given N samples x[k], k=0..N-1, we estimate the fundamental phasor:
 *   X = (2/N) * sum_k x[k] * e^{-j 2πk/N}
 *
 * Then:
 *   magnitude = hypot(real, imag)
 *   angle     = atan2(imag, real)   (radians)	
 * We precompute cos(2πk/N) and sin(2πk/N) once in the context
 * and reuse them.
 */

static void
compute_phasor_from_buffer(const meas_sv_context_t    *ctx,
                           const meas_channel_buffer_t *buf,
                           float                      *out_real,
                           float                      *out_imag)
{
    if (!ctx || !buf || buf->count < MEAS_SAMPLES_PER_CYCLE) {
        *out_real = 0.0f;
        *out_imag = 0.0f;
        return;
    }

    const int N = MEAS_SAMPLES_PER_CYCLE;
    float real = 0.0f;
    float imag = 0.0f;

    /* Buffer is circular; we treat index as the "next write" position
     * and reconstruct the window in order.
     */
    int start = buf->index; /* this is where the next sample will go */

    for (int k = 0; k < N; ++k) {
        int pos = (start + k) % N;
        float x = buf->samples[pos];

        /* Use precomputed basis for this k. */
        float c = ctx->dft_cos[k];
        float s = ctx->dft_sin[k];

        real += x * c;
        imag -= x * s; /* negative sign for e^{-j*theta} */
    }

    float scale = 2.0f / (float)N;
    *out_real = real * scale;
    *out_imag = imag * scale;
}

void
meas_sv_context_init(meas_sv_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    /* Precompute DFT coefficients for k = 0..N-1. */
    const int N = MEAS_SAMPLES_PER_CYCLE;
    for (int k = 0; k < N; ++k) {
        float angle = (2.0f * (float)M_PI * k) / (float)N;
        ctx->dft_cos[k] = cosf(angle);
        ctx->dft_sin[k] = sinf(angle);
    }

    /* Initialize buffers. */
    for (int channel = 0; channel < 4; ++channel) {
        ctx->current_buffers[channel].index = 0;
        ctx->current_buffers[channel].count = 0;
        for (int i = 0; i < MEAS_SAMPLES_PER_CYCLE; ++i) {
            ctx->current_buffers[channel].samples[i] = 0.0f;
        }

        ctx->voltage_buffers[channel].index = 0;
        ctx->voltage_buffers[channel].count = 0;
        for (int i = 0; i < MEAS_SAMPLES_PER_CYCLE; ++i) {
            ctx->voltage_buffers[channel].samples[i] = 0.0f;
        }
    }
}

/* Push one new sample into a channel buffer. */
static void
push_sample(meas_channel_buffer_t *buf, float sample)
{
    buf->samples[buf->index] = sample;
    buf->index = (buf->index + 1) % MEAS_SAMPLES_PER_CYCLE;
    if (buf->count < MEAS_SAMPLES_PER_CYCLE) {
        buf->count++;
    }
}

void
meas_sv_process_sample(meas_sv_context_t        *ctx,
                       const sv_payload_t       *payload,
                       vpac_measurement_frame_t *out_frame)
{
    if (!ctx || !payload || !out_frame) {
        return;
    }

    /* 1) Push new local samples into buffers. */
    for (int ch = 0; ch < 4; ++ch) {
        push_sample(&ctx->current_buffers[ch], payload->local_current[ch]);
        push_sample(&ctx->voltage_buffers[ch], payload->local_voltage[ch]);
    }

    /* 2) Compute phasors for local currents and voltages (if buffers full). */
    float current_real[4], current_imag[4];
    float voltage_real[4], voltage_imag[4];

    for (int ch = 0; ch < 4; ++ch) {
        compute_phasor_from_buffer(ctx,
                                   &ctx->current_buffers[ch],
                                   &current_real[ch],
                                   &current_imag[ch]);
        compute_phasor_from_buffer(ctx,
                                   &ctx->voltage_buffers[ch],
                                   &voltage_real[ch],
                                   &voltage_imag[ch]);
    }

    /* 3) Fill magnitudes and angles; if buffers not full yet, fall back to
     *    instantaneous samples.
     */
    for (int ch = 0; ch < 4; ++ch) {
        float mag_I, ang_I;
        float mag_V, ang_V;

        if (ctx->current_buffers[ch].count >= MEAS_SAMPLES_PER_CYCLE) {
            mag_I = hypotf(current_real[ch], current_imag[ch]);
            ang_I = atan2f(current_imag[ch], current_real[ch]);
        } else {
            mag_I = payload->local_current[ch];
            ang_I = 0.0f;
        }

        if (ctx->voltage_buffers[ch].count >= MEAS_SAMPLES_PER_CYCLE) {
            mag_V = hypotf(voltage_real[ch], voltage_imag[ch]);
            ang_V = atan2f(voltage_imag[ch], voltage_real[ch]);
        } else {
            mag_V = payload->local_voltage[ch];
            ang_V = 0.0f;
        }

        out_frame->current_magnitude[ch] = mag_I;
        out_frame->current_angle[ch]     = ang_I;
        out_frame->voltage_magnitude[ch] = mag_V;
        out_frame->voltage_angle[ch]     = ang_V;
    }

    /* 4) Fill local complex currents for phases A, B, C.
     *    - If buffers are full, reuse the DFT phasor real/imag.
     *    - Otherwise, treat instantaneous samples as purely real.
     */
    for (int phase = 0; phase < 3; ++phase) {
        if (ctx->current_buffers[phase].count >= MEAS_SAMPLES_PER_CYCLE) {
            out_frame->local_current[phase].real = current_real[phase];
            out_frame->local_current[phase].imag = current_imag[phase];
        } else {
            float inst = payload->local_current[phase];
            out_frame->local_current[phase].real = inst;
            out_frame->local_current[phase].imag = 0.0f;
        }
    }

    /* 5) Fill remote complex currents from remote_current[] payload.
     *    For now we treat remote currents as steady phasors with angle 0.
     */
    for (int phase = 0; phase < 3; ++phase) {
        float mag_remote = payload->remote_current[phase];

        out_frame->remote_current[phase].real = mag_remote;
        out_frame->remote_current[phase].imag = 0.0f;
    }
}
