#ifndef DTMF_H
#define DTMF_H

#include <stdint.h>
#include <stdio.h>

#define SAMPLE_RATE 44100
#define TONE_DURATION_MS 50
#define PAUSE_DURATION_MS 50
#define FADE_DURATION_MS 5
#define FFT_SIZE 4096
#define FREQ_TOLERANCE 5.0

// WAV header structure
typedef struct {
    char chunk_id[4];       // "RIFF"
    uint32_t chunk_size;
    char format[4];         // "WAVE"
    char subchunk1_id[4];   // "fmt "
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char subchunk2_id[4];   // "data"
    uint32_t subchunk2_size;
} WavHeader;

// DTMF tone generator structure
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

// Goertzel algorithm state
typedef struct {
    float coeff;
    float q1;
    float q2;
    float real;
    float imag;
    int sample_count;
} GoertzelState;

// Public function declarations
DTMFEncoder* dtmf_create_encoder(void);
void dtmf_destroy_encoder(DTMFEncoder* encoder);
int dtmf_encode_char(DTMFEncoder* encoder, char c, float* buffer);
char dtmf_decode_chunk(const float* buffer, int size, DTMFEncoder* encoder);
int dtmf_encode_string(DTMFEncoder* encoder, const char* text, const char* filename);
char* dtmf_decode_file(const char* filename, DTMFEncoder* encoder);
int dtmf_decode_file_stream(const char* filename, DTMFEncoder* encoder);
int dtmf_decode_stream(FILE* fp, DTMFEncoder* encoder, int skip_header);

#endif // DTMF_H