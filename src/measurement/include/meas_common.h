#ifndef MEAS_COMMON_H
#define MEAS_COMMON_H

#include <stdint.h>

/*
 * Simplified Sampled Values payload used between sender and receiver.
 *
 * Real IEC 61850-9-2 SV frames contain:
 *   - 4 currents (Ia, Ib, Ic, In)
 *   - 4 voltages (Va, Vb, Vc, Vn)
 *   - sample count, quality, etc.
 *
 * For differential (ANSI 87) we also need remote currents.
 * In this testbed we pack local and remote currents
 * into the same payload.
 *
 * Here we model just:
 *   - frame_id: unique identifier for the frame (uint32_t)
 *   - sender_timestamp_ns: timestamp at sender (nanoseconds)
 *   - local_current[4]:   Ia, Ib, Ic, In at local end
 *   - local_voltage[4]:   Va, Vb, Vc, Vn at local end
 *   - remote_current[4]:  Ia, Ib, Ic, In at remote end
 *
 * Both sides of the testbed should agree on this binary layout.
 */
typedef struct {
    uint32_t frame_id;
    uint64_t sender_timestamp_ns;
    float    local_current[4];   /* Ia, Ib, Ic, In at local end */
    float    local_voltage[4];   /* Va, Vb, Vc, Vn at local end */
    float    remote_current[4];  /* Ia, Ib, Ic, In at remote end */
} sv_payload_t;

#endif /* MEAS_COMMON_H */
