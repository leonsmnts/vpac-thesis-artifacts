#ifndef PROT_COMMON_H
#define PROT_COMMON_H

#include <stdint.h>

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif


/*
 * Complex float type for phasors.
 */
typedef struct {
    float real;
    float imag;
} vpac_complex_float_t;

/*
 * Measurement frame for a single protection step.
 *
 * The idea is: measurement/DFT layer fills this struct
 * from Sampled Values (SV) before calling any protection function.
 *
 * Conventions:
 *   - Index 0: phase A
 *   - Index 1: phase B
 *   - Index 2: phase C
 *   - Index 3: neutral (N)
 */
typedef struct {
    /* Magnitudes of phase and neutral currents (e.g., RMS or filtered). */
    float current_magnitude[4];   /* Ia, Ib, Ic, In */

    /* Magnitudes of phase and neutral voltages (if available). */
    float voltage_magnitude[4];   /* Va, Vb, Vc, Vn */

    /* Angles of phase and neutral currents (radians or degrees, consistent). */
    /* We use radians */
    float current_angle[4];

    /* Angles of phase and neutral voltages. */
    float voltage_angle[4];

    /* Optional complex representations.
     * For differential/phasor-based functions like 21, 87, etc.
     * Index 0..2: phases A, B, C.
     * 
     * Local and Remote here means it is used between two IEDs.
     */
    vpac_complex_float_t local_current[3];   /* local end for differential */
    vpac_complex_float_t remote_current[3];  /* remote end for differential */
} vpac_measurement_frame_t;

/*
 * Protection function output flags.
 *
 * Each mask uses bits to represent phases:
 *   bit 0 -> phase A
 *   bit 1 -> phase B
 *   bit 2 -> phase C
 *   bit 3 -> neutral
 *
 * Can OR masks from multiple elements to build bay-level logic.
 */
typedef struct {
    uint32_t start_mask;  /* element has picked up (above threshold / in zone) */
    uint32_t trip_mask;   /* element is issuing an operate (trip) command      */
    uint32_t block_mask;  /* element is blocked/inhibited                      */
} protection_output_t;

/* Convenience macros for bit positions */
#define PROT_PHASE_A_MASK (1u << 0)
#define PROT_PHASE_B_MASK (1u << 1)
#define PROT_PHASE_C_MASK (1u << 2)
#define PROT_PHASE_N_MASK (1u << 3)

static inline uint32_t
prot_phase_index_to_mask(int phase_index)
{
    switch (phase_index) {
    case 0: return PROT_PHASE_A_MASK;
    case 1: return PROT_PHASE_B_MASK;
    case 2: return PROT_PHASE_C_MASK;
    case 3: return PROT_PHASE_N_MASK;
    default: return 0u;
    }
}

/* Normalize angle into [-pi, +pi] or equivalent range for comparison. 
 * 
 * IMPORTANT: data-dependent while loops. If the measurement layer guarantees angles already near [-pi, pi], fine; otherwise replace with bounded wrapping logic.
 */
static float
normalize_angle(float angle)
{
    /* If angles are in degrees, adjust constants accordingly. */
    // We use radians here, so 2*pi and pi are used.
    const float two_pi = (float)(2.0 * M_PI);

    /* Bring into [0, 2*pi) first. */
    while (angle >= two_pi) {
        angle -= two_pi;
    }
    while (angle < 0.0f) {
        angle += two_pi;
    }

    /* Then into [-pi, +pi]. */
    if (angle > (float)M_PI) {
        angle -= two_pi;
    }
    return angle;
}

#endif /* PROT_COMMON_H */
