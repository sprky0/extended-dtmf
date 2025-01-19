#ifndef ENCODER_H
#define ENCODER_H

#include "goertzel.h"

// DTMF encoder/decoder structure
typedef struct {
    float low_freqs[16];
    float high_freqs[16];
    float amplitude;
    int sample_rate;
    int tone_samples;
    int pause_samples;
    int fade_samples;
    int verbose;
    float detection_threshold;
} DTMFEncoder;

// Internal functions exposed for testing
int dtmf_encode_char(DTMFEncoder* encoder, char c, float* buffer);
char dtmf_decode_chunk(const float* buffer, int size, DTMFEncoder* encoder);

#endif // ENCODER_H