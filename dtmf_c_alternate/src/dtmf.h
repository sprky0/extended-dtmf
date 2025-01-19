#ifndef DTMF_H
#define DTMF_H

#include <stdint.h>
#include <stdio.h>  // Add this for FILE* type definition

// DTMF tone generation parameters
#define SAMPLE_RATE 44100
#define TONE_DURATION_MS 100
#define PAUSE_DURATION_MS 50
#define FADE_DURATION_MS 10

// DTMF frequency sets
#define LOW_FREQ_COUNT 4
#define HIGH_FREQ_COUNT 4

typedef struct {
    // Encoder configuration
    float low_frequencies[LOW_FREQ_COUNT];
    float high_frequencies[HIGH_FREQ_COUNT];
    float amplitude;
    int sample_rate;
    int tone_samples;
    int pause_samples;
    int fade_samples;
    
    // Decoding parameters
    float detection_threshold;
} DTMFEncoder;

// Core function prototypes
DTMFEncoder* dtmf_encoder_create(void);
void dtmf_encoder_destroy(DTMFEncoder* encoder);
int dtmf_encode_to_file(DTMFEncoder* encoder, const char* text, const char* filename);
char* dtmf_decode_from_file(DTMFEncoder* encoder, const char* filename);

#endif // DTMF_H