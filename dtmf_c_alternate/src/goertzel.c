#include "goertzel.h"
#include <math.h>

#define PI 3.14159265358979323846

void goertzel_init(GoertzelState* state, float frequency, int sample_rate) {
    float omega = 2.0f * PI * frequency / sample_rate;
    state->coeff = 2.0f * cosf(omega);
    state->q1 = 0;
    state->q2 = 0;
    state->sample_count = 0;
}

void goertzel_process(GoertzelState* state, const float* buffer, int size) {
    for (int i = 0; i < size; i++) {
        float q0 = buffer[i] + state->coeff * state->q1 - state->q2;
        state->q2 = state->q1;
        state->q1 = q0;
    }
    state->sample_count += size;
}

float goertzel_magnitude(const GoertzelState* state) {
    float real = state->q1 - state->q2 * cosf(2.0f * PI * state->sample_count);
    float imag = state->q2 * sinf(2.0f * PI * state->sample_count);
    return sqrtf(real * real + imag * imag);
}