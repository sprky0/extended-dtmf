#ifndef GOERTZEL_H
#define GOERTZEL_H

// Goertzel algorithm state
typedef struct {
    float coeff;
    float q1;
    float q2;
    float real;
    float imag;
    int sample_count;
} GoertzelState;

void goertzel_init(GoertzelState* state, float frequency, int sample_rate);
void goertzel_process(GoertzelState* state, const float* buffer, int size);
float goertzel_magnitude(const GoertzelState* state);

#endif // GOERTZEL_H