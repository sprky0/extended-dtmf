#include "dtmf.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>  // Add this line for FILE*, fopen(), fclose(), etc.

#define PI 3.14159265358979323846f

// Simple character to DTMF mapping
static const char* DTMF_CHARS = "123A456B789C*0#D";

static int dtmf_get_character_code(char c) {
    const char* pos = strchr(DTMF_CHARS, c);
    return pos ? (pos - DTMF_CHARS) : -1;
}

DTMFEncoder* dtmf_encoder_create(void) {
    DTMFEncoder* encoder = malloc(sizeof(DTMFEncoder));
    if (!encoder) return NULL;

    // Standard DTMF frequencies
    float low_freqs[] = {697.0f, 770.0f, 852.0f, 941.0f};
    float high_freqs[] = {1209.0f, 1336.0f, 1477.0f, 1633.0f};

    memcpy(encoder->low_frequencies, low_freqs, sizeof(low_freqs));
    memcpy(encoder->high_frequencies, high_freqs, sizeof(high_freqs));

    encoder->amplitude = 0.8f;
    encoder->sample_rate = SAMPLE_RATE;
    encoder->tone_samples = (TONE_DURATION_MS * SAMPLE_RATE) / 1000;
    encoder->pause_samples = (PAUSE_DURATION_MS * SAMPLE_RATE) / 1000;
    encoder->fade_samples = (FADE_DURATION_MS * SAMPLE_RATE) / 1000;
    encoder->detection_threshold = 0.0001f;

    return encoder;
}

void dtmf_encoder_destroy(DTMFEncoder* encoder) {
    free(encoder);
}

int dtmf_encode_to_file(DTMFEncoder* encoder, const char* text, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) return -1;

    // Allocate buffers
    int total_samples = (encoder->tone_samples + encoder->pause_samples) * strlen(text);
    int16_t* audio_buffer = malloc(total_samples * sizeof(int16_t));
    float* tone_buffer = malloc((encoder->tone_samples + encoder->pause_samples) * sizeof(float));
    
    if (!audio_buffer || !tone_buffer) {
        free(audio_buffer);
        free(tone_buffer);
        fclose(file);
        return -1;
    }

    int buffer_index = 0;
    for (int i = 0; text[i]; i++) {
        int code = dtmf_get_character_code(text[i]);
        if (code < 0) continue;  // Skip invalid characters

        int row = code / HIGH_FREQ_COUNT;
        int col = code % HIGH_FREQ_COUNT;

        float low_freq = encoder->low_frequencies[row];
        float high_freq = encoder->high_frequencies[col];

        // Generate tone
        for (int j = 0; j < encoder->tone_samples; j++) {
            float t = (float)j / encoder->sample_rate;
            float envelope = 1.0f;

            // Apply fade
            if (j < encoder->fade_samples) {
                envelope = (float)j / encoder->fade_samples;
            } else if (j > encoder->tone_samples - encoder->fade_samples) {
                envelope = (float)(encoder->tone_samples - j) / encoder->fade_samples;
            }

            tone_buffer[j] = encoder->amplitude * envelope * (
                sinf(2 * PI * low_freq * t) +
                sinf(2 * PI * high_freq * t)
            );
        }

        // Add pause (silence)
        for (int j = encoder->tone_samples; j < encoder->tone_samples + encoder->pause_samples; j++) {
            tone_buffer[j] = 0.0f;
        }

        // Convert to 16-bit PCM
        for (int j = 0; j < encoder->tone_samples + encoder->pause_samples; j++) {
            audio_buffer[buffer_index++] = (int16_t)(tone_buffer[j] * 32767.0f);
        }
    }

    // Write audio data
    fwrite(audio_buffer, sizeof(int16_t), buffer_index, file);
    fclose(file);

    free(audio_buffer);
    free(tone_buffer);
    return 0;
}

char* dtmf_decode_from_file(DTMFEncoder* encoder, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read audio data
    int16_t* audio_buffer = malloc(file_size);
    float* tone_buffer = malloc(encoder->tone_samples * sizeof(float));
    
    if (!audio_buffer || !tone_buffer) {
        free(audio_buffer);
        free(tone_buffer);
        fclose(file);
        return NULL;
    }

    size_t samples_read = fread(audio_buffer, sizeof(int16_t), file_size / sizeof(int16_t), file);
    fclose(file);

    // Prepare decoded string buffer
    char* decoded = malloc(samples_read / (encoder->tone_samples + encoder->pause_samples) + 1);
    int decoded_index = 0;

    // Process samples in tone-sized chunks
    for (size_t i = 0; i < samples_read; i += encoder->tone_samples + encoder->pause_samples) {
        // Convert to float
        for (int j = 0; j < encoder->tone_samples; j++) {
            tone_buffer[j] = audio_buffer[i + j] / 32767.0f;
        }

        // Analyze tone frequencies
        float max_low_mag = 0;
        float max_high_mag = 0;
        int max_low_index = -1;
        int max_high_index = -1;

        // Frequency detection using simple magnitude comparison
        for (int row = 0; row < LOW_FREQ_COUNT; row++) {
            float magnitude = 0;
            for (int j = 0; j < encoder->tone_samples; j++) {
                float t = (float)j / encoder->sample_rate;
                magnitude += tone_buffer[j] * sinf(2 * PI * encoder->low_frequencies[row] * t);
            }
            magnitude = fabsf(magnitude);

            if (magnitude > max_low_mag) {
                max_low_mag = magnitude;
                max_low_index = row;
            }
        }

        for (int col = 0; col < HIGH_FREQ_COUNT; col++) {
            float magnitude = 0;
            for (int j = 0; j < encoder->tone_samples; j++) {
                float t = (float)j / encoder->sample_rate;
                magnitude += tone_buffer[j] * sinf(2 * PI * encoder->high_frequencies[col] * t);
            }
            magnitude = fabsf(magnitude);

            if (magnitude > max_high_mag) {
                max_high_mag = magnitude;
                max_high_index = col;
            }
        }

        // Check if detection is above threshold
        if (max_low_mag > encoder->detection_threshold && 
            max_high_mag > encoder->detection_threshold) {
            int code = max_low_index * HIGH_FREQ_COUNT + max_high_index;
            decoded[decoded_index++] = DTMF_CHARS[code];
        }
    }

    decoded[decoded_index] = '\0';
    free(audio_buffer);
    free(tone_buffer);
    return decoded;
}