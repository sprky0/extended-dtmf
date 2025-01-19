#include "dtmf.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

// Static function declarations
static void goertzel_init(GoertzelState* state, float frequency, int sample_rate);
static void goertzel_process(GoertzelState* state, const float* buffer, int size);
static float goertzel_magnitude(const GoertzelState* state);
static void write_wav_header(FILE* fp, uint32_t data_size);

// Implementation of static functions
static void goertzel_init(GoertzelState* state, float frequency, int sample_rate) {
    float omega = 2.0f * PI * frequency / sample_rate;
    state->coeff = 2.0f * cosf(omega);
    state->q1 = 0;
    state->q2 = 0;
    state->sample_count = 0;
}

static void goertzel_process(GoertzelState* state, const float* buffer, int size) {
    for (int i = 0; i < size; i++) {
        float q0 = buffer[i] + state->coeff * state->q1 - state->q2;
        state->q2 = state->q1;
        state->q1 = q0;
    }
    state->sample_count += size;
}

static float goertzel_magnitude(const GoertzelState* state) {
    float real = state->q1 - state->q2 * cosf(2.0f * PI * state->sample_count);
    float imag = state->q2 * sinf(2.0f * PI * state->sample_count);
    return sqrtf(real * real + imag * imag);
}

static void write_wav_header(FILE* fp, uint32_t data_size) {
    WavHeader header = {
        .chunk_id = {'R', 'I', 'F', 'F'},
        .format = {'W', 'A', 'V', 'E'},
        .subchunk1_id = {'f', 'm', 't', ' '},
        .subchunk1_size = 16,
        .audio_format = 1, // PCM
        .num_channels = 1, // Mono
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .block_align = 2, // channels * bits_per_sample / 8
        .byte_rate = SAMPLE_RATE * 2, // sample_rate * block_align
        .subchunk2_id = {'d', 'a', 't', 'a'},
        .subchunk2_size = data_size,
        .chunk_size = data_size + 36
    };
    
    fwrite(&header, sizeof(header), 1, fp);
}

// Implementation of public functions
DTMFEncoder* dtmf_create_encoder() {
    DTMFEncoder* encoder = malloc(sizeof(DTMFEncoder));
    if (!encoder) return NULL;

    // Initialize frequency tables
    for (int i = 0; i < 16; i++) {
        encoder->low_freqs[i] = 624 + i * 73;
        encoder->high_freqs[i] = 1209 + i * 127;
    }

    encoder->amplitude = 0.3f;
    encoder->sample_rate = SAMPLE_RATE;
    encoder->tone_samples = (TONE_DURATION_MS * SAMPLE_RATE) / 1000;
    encoder->pause_samples = (PAUSE_DURATION_MS * SAMPLE_RATE) / 1000;
    encoder->fade_samples = (FADE_DURATION_MS * SAMPLE_RATE) / 1000;

    return encoder;
}

void dtmf_destroy_encoder(DTMFEncoder* encoder) {
    free(encoder);
}

int dtmf_encode_char(DTMFEncoder* encoder, char c, float* buffer) {
    int code = (unsigned char)c;
    int row = code / 16;
    int col = code % 16;
    
    float low_freq = encoder->low_freqs[row];
    float high_freq = encoder->high_freqs[col];
    
    // Generate samples
    int total_samples = encoder->tone_samples + encoder->pause_samples;
    for (int i = 0; i < encoder->tone_samples; i++) {
        float t = (float)i / encoder->sample_rate;
        float envelope = 1.0f;
        
        // Apply fade in/out
        if (i < encoder->fade_samples) {
            envelope = (float)i / encoder->fade_samples;
        } else if (i > encoder->tone_samples - encoder->fade_samples) {
            envelope = (float)(encoder->tone_samples - i) / encoder->fade_samples;
        }
        
        buffer[i] = encoder->amplitude * envelope * (
            sinf(2 * PI * low_freq * t) +
            sinf(2 * PI * high_freq * t)
        );
    }
    
    // Add silence for pause
    for (int i = encoder->tone_samples; i < total_samples; i++) {
        buffer[i] = 0.0f;
    }
    
    return total_samples;
}

int dtmf_encode_string(DTMFEncoder* encoder, const char* text, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    // Skip header for now
    fseek(fp, sizeof(WavHeader), SEEK_SET);
    
    int samples_per_char = encoder->tone_samples + encoder->pause_samples;
    float* buffer = malloc(samples_per_char * sizeof(float));
    if (!buffer) {
        fclose(fp);
        return -1;
    }
    
    uint32_t total_samples = 0;
    for (int i = 0; text[i]; i++) {
        int samples = dtmf_encode_char(encoder, text[i], buffer);
        total_samples += samples;
        
        // Convert to 16-bit PCM and write
        for (int j = 0; j < samples; j++) {
            int16_t sample = buffer[j] * 32767.0f;
            fwrite(&sample, sizeof(int16_t), 1, fp);
        }
    }
    
    // Go back and write header
    fseek(fp, 0, SEEK_SET);
    write_wav_header(fp, total_samples * sizeof(int16_t));
    
    free(buffer);
    fclose(fp);
    return 0;
}

char dtmf_decode_chunk(const float* buffer, int size, DTMFEncoder* encoder) {
    float max_low_magnitude = 0;
    float max_high_magnitude = 0;
    int max_low_index = 0;
    int max_high_index = 0;
    
    // Check each possible frequency
    for (int i = 0; i < 16; i++) {
        GoertzelState low_state, high_state;
        
        // Initialize Goertzel for both frequencies
        goertzel_init(&low_state, encoder->low_freqs[i], encoder->sample_rate);
        goertzel_init(&high_state, encoder->high_freqs[i], encoder->sample_rate);
        
        // Process the buffer
        goertzel_process(&low_state, buffer, size);
        goertzel_process(&high_state, buffer, size);
        
        // Get magnitudes
        float low_mag = goertzel_magnitude(&low_state);
        float high_mag = goertzel_magnitude(&high_state);
        
        // Update maximum if necessary
        if (low_mag > max_low_magnitude) {
            max_low_magnitude = low_mag;
            max_low_index = i;
        }
        if (high_mag > max_high_magnitude) {
            max_high_magnitude = high_mag;
            max_high_index = i;
        }
    }
    
    // Convert indices to character
    return (char)(max_low_index * 16 + max_high_index);
}

char* dtmf_decode_file(const char* filename, DTMFEncoder* encoder) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    
    // Read WAV header
    WavHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }
    
    // Validate WAV format
    if ((int)header.sample_rate != encoder->sample_rate ||
        header.bits_per_sample != 16 ||
        header.num_channels != 1) {
        fclose(fp);
        return NULL;
    }
    
    // Allocate buffer for one character duration
    int samples_per_char = encoder->tone_samples;
    float* buffer = malloc(samples_per_char * sizeof(float));
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    
    // Allocate result string buffer (dynamically grown)
    size_t result_capacity = 256;
    size_t result_size = 0;
    char* result = malloc(result_capacity);
    if (!result) {
        free(buffer);
        fclose(fp);
        return NULL;
    }
    
    // Read and decode chunks
    int16_t sample;
    int buffer_pos = 0;
    while (fread(&sample, sizeof(int16_t), 1, fp) == 1) {
        // Convert to float
        buffer[buffer_pos++] = sample / 32767.0f;
        
        if (buffer_pos == samples_per_char) {
            // Decode chunk
            char decoded = dtmf_decode_chunk(buffer, samples_per_char, encoder);
            
            // Add to result string
            if (result_size + 1 >= result_capacity) {
                result_capacity *= 2;
                char* new_result = realloc(result, result_capacity);
                if (!new_result) {
                    free(buffer);
                    free(result);
                    fclose(fp);
                    return NULL;
                }
                result = new_result;
            }
            
            result[result_size++] = decoded;
            buffer_pos = 0;
            
            // Skip pause samples
            fseek(fp, encoder->pause_samples * sizeof(int16_t), SEEK_CUR);
        }
    }
    
    result[result_size] = '\0';
    
    free(buffer);
    fclose(fp);
    return result;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s encode <text|->  [output]    (use - for stdin)\n", argv[0]);
        printf("       %s decode <file>\n", argv[0]);
        return 1;
    }
    
    DTMFEncoder* encoder = dtmf_create_encoder();
    if (!encoder) {
        printf("Failed to initialize encoder\n");
        return 1;
    }
    
    if (strcmp(argv[1], "encode") == 0) {
        const char* output = argc > 3 ? argv[3] : "output.wav";
        
        if (strcmp(argv[2], "-") == 0) {
            // Read from stdin
            char* buffer = NULL;
            size_t total_size = 0;
            size_t alloc_size = 1024;  // Start with 1KB
            buffer = malloc(alloc_size);
            
            if (!buffer) {
                printf("Failed to allocate memory\n");
                dtmf_destroy_encoder(encoder);
                return 1;
            }
            
            int c;
            while ((c = getchar()) != EOF) {
                if (total_size + 1 >= alloc_size) {
                    alloc_size *= 2;
                    char* new_buffer = realloc(buffer, alloc_size);
                    if (!new_buffer) {
                        printf("Failed to reallocate memory\n");
                        free(buffer);
                        dtmf_destroy_encoder(encoder);
                        return 1;
                    }
                    buffer = new_buffer;
                }
                buffer[total_size++] = (char)c;
            }
            buffer[total_size] = '\0';
            
            if (dtmf_encode_string(encoder, buffer, output) != 0) {
                printf("Failed to encode string\n");
                free(buffer);
                dtmf_destroy_encoder(encoder);
                return 1;
            }
            free(buffer);
        } else {
            if (dtmf_encode_string(encoder, argv[2], output) != 0) {
                printf("Failed to encode string\n");
                dtmf_destroy_encoder(encoder);
                return 1;
            }
        }
    } else if (strcmp(argv[1], "decode") == 0) {
        char* decoded = dtmf_decode_file(argv[2], encoder);
        if (!decoded) {
            printf("Failed to decode file\n");
            dtmf_destroy_encoder(encoder);
            return 1;
        }
        printf("Decoded text: %s\n", decoded);
        free(decoded);
    }
    
    dtmf_destroy_encoder(encoder);
    return 0;
}