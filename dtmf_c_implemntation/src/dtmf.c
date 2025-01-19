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
static char dtmf_char_to_code(char c);
static char code_to_dtmf_char(unsigned char code);

// Convert standard DTMF character to our encoding
static char dtmf_char_to_code(char c) {
    // Standard DTMF layout map
    static const char dtmf_chars[] = "123A456B789C*0#D";
    
    // Look up character in standard map
    for (int i = 0; i < 16; i++) {
        if (dtmf_chars[i] == c) {
            return (char)i;
        if (encoder->verbose) {
        fprintf(stderr, "Total samples read: %d\n", total_samples_read);
        if (result_size == 0) {
            fprintf(stderr, "No valid characters were decoded.\n");
            fprintf(stderr, "This could be due to:\n");
            fprintf(stderr, "1. Signal amplitude too low (try adjusting detection_threshold)\n");
            fprintf(stderr, "2. Incorrect frequency detection\n");
            fprintf(stderr, "3. Timing mismatch between encode/decode\n");
        }
    }
    }
    
    // If not found, return original character (for extended encoding)
    return c;
}

// Convert our encoding back to standard DTMF character
static char code_to_dtmf_char(unsigned char code) {
    static const char dtmf_chars[] = "123A456B789C*0#D";
    
    // If within standard DTMF range, convert back
    if (code < 16) {
        return dtmf_chars[code];
    }
    
    // If not in standard range, return original code
    return (char)code;
}

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
        .audio_format = 1,
        .num_channels = 1,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .block_align = 2,
        .byte_rate = SAMPLE_RATE * 2,
        .subchunk2_id = {'d', 'a', 't', 'a'},
        .subchunk2_size = data_size,
        .chunk_size = data_size + 36
    };
    fwrite(&header, sizeof(header), 1, fp);
}

// Add this function to dtmf.c
static void dump_wav_samples(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return;
    
    WavHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return;
    }
    
    // Skip to data chunk
    while (1) {
        if (fread(header.chunk_id, 1, 4, fp) != 4) break;
        if (fread(&header.chunk_size, 4, 1, fp) != 1) break;
        if (memcmp(header.chunk_id, "data", 4) == 0) break;
        fseek(fp, header.chunk_size, SEEK_CUR);
    }
    
    printf("Sample values:\n");
    int16_t sample;
    int count = 0;
    while (fread(&sample, sizeof(int16_t), 1, fp) == 1 && count < 100) {
        printf("%6d ", sample);
        if (++count % 10 == 0) printf("\n");
    }
    printf("\n");
    
    fclose(fp);
}

DTMFEncoder* dtmf_create_encoder(void) {
    DTMFEncoder* encoder = malloc(sizeof(DTMFEncoder));
    if (!encoder) return NULL;

    // Initialize frequency tables
    float std_low_freqs[] = {697.0f, 770.0f, 852.0f, 941.0f};
    float std_high_freqs[] = {1209.0f, 1336.0f, 1477.0f, 1633.0f};
    
    // Map frequencies to keypad layout
    for (int i = 0; i < 16; i++) {
        int row = i / 4;  // Integer division for row
        int col = i % 4;  // Modulo for column
        encoder->low_freqs[i] = std_low_freqs[row];
        encoder->high_freqs[i] = std_high_freqs[col];
    }
    
    encoder->amplitude = 0.8f;
    encoder->sample_rate = SAMPLE_RATE;
    encoder->tone_samples = (TONE_DURATION_MS * SAMPLE_RATE) / 1000;
    encoder->pause_samples = (PAUSE_DURATION_MS * SAMPLE_RATE) / 1000;
    encoder->fade_samples = (FADE_DURATION_MS * SAMPLE_RATE) / 1000;
    encoder->verbose = 0;
    encoder->detection_threshold = 0.001f;

    return encoder;
}

void dtmf_destroy_encoder(DTMFEncoder* encoder) {
    free(encoder);
}

int dtmf_encode_char(DTMFEncoder* encoder, char c, float* buffer) {
    char code = dtmf_char_to_code(c);
    if (code < 0) {
        if (encoder->verbose) {
            fprintf(stderr, "Invalid DTMF character: '%c'\n", c);
        }
        return 0;
    }
    
    int row = code / 4;
    int col = code % 4;
    
    if (encoder->verbose) {
        fprintf(stderr, "Encoding char '%c' (row: %d, col: %d, freqs: %.1f/%.1f Hz)\n",
                c, row, col, encoder->low_freqs[row], encoder->high_freqs[col]);
    }
    
    float low_freq = encoder->low_freqs[row];
    float high_freq = encoder->high_freqs[col];
    
    int total_samples = encoder->tone_samples + encoder->pause_samples;
    for (int i = 0; i < encoder->tone_samples; i++) {
        float t = (float)i / encoder->sample_rate;
        float envelope = 1.0f;
        
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
    
    for (int i = encoder->tone_samples; i < total_samples; i++) {
        buffer[i] = 0.0f;
    }
    
    return total_samples;
}

char dtmf_decode_chunk(const float* buffer, int size, DTMFEncoder* encoder) {
    float max_low_magnitude = 0;
    float max_high_magnitude = 0;
    int max_low_index = -1;
    int max_high_index = -1;
    
    if (encoder->verbose) {
        fprintf(stderr, "\nAnalyzing chunk for DTMF tones:\n");
    }
    
    // Check each row frequency
    for (int i = 0; i < 4; i++) {
        GoertzelState low_state;
        goertzel_init(&low_state, encoder->low_freqs[i], encoder->sample_rate);
        goertzel_process(&low_state, buffer, size);
        float low_mag = goertzel_magnitude(&low_state);
        
        if (encoder->verbose) {
            fprintf(stderr, "Low freq %.1f Hz: magnitude = %.6f\n", 
                    encoder->low_freqs[i], low_mag);
        }
        
        if (low_mag > max_low_magnitude) {
            max_low_magnitude = low_mag;
            max_low_index = i;
        }
    }
    
    // Check each column frequency
    for (int i = 0; i < 4; i++) {
        GoertzelState high_state;
        goertzel_init(&high_state, encoder->high_freqs[i], encoder->sample_rate);
        goertzel_process(&high_state, buffer, size);
        float high_mag = goertzel_magnitude(&high_state);
        
        if (encoder->verbose) {
            fprintf(stderr, "High freq %.1f Hz: magnitude = %.6f\n", 
                    encoder->high_freqs[i], high_mag);
        }
        
        if (high_mag > max_high_magnitude) {
            max_high_magnitude = high_mag;
            max_high_index = i;
        }
    }
    
    if (max_low_magnitude < encoder->detection_threshold || 
        max_high_magnitude < encoder->detection_threshold) {
        if (encoder->verbose) {
            fprintf(stderr, "Signal too weak: low=%.6f, high=%.6f (threshold=%.6f)\n",
                    max_low_magnitude, max_high_magnitude, encoder->detection_threshold);
        }
        return '\0';
    }
    
    // Convert indices to character
    unsigned char code = max_low_index * 4 + max_high_index;
    char decoded = code_to_dtmf_char(code);
    
    if (encoder->verbose) {
        fprintf(stderr, "Decoded '%c' (row=%d, col=%d, freqs=%.1f/%.1f Hz)\n",
                decoded, max_low_index, max_high_index,
                encoder->low_freqs[max_low_index],
                encoder->high_freqs[max_high_index]);
    }
    
    return decoded;
}

int dtmf_encode_string(DTMFEncoder* encoder, const char* text, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) return -1;
    
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
        
        for (int j = 0; j < samples; j++) {
            int16_t sample = buffer[j] * 32767.0f;
            fwrite(&sample, sizeof(int16_t), 1, fp);
        }
    }
    
    fseek(fp, 0, SEEK_SET);
    write_wav_header(fp, total_samples * sizeof(int16_t));
    
    free(buffer);
    fclose(fp);
    return 0;
}

char* dtmf_decode_file(const char* filename, DTMFEncoder* encoder) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;
    
    WavHeader header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }
    
    if (encoder->verbose) {
        fprintf(stderr, "WAV Header Info:\n");
        fprintf(stderr, "Sample Rate: %u\n", header.sample_rate);
        fprintf(stderr, "Bits/Sample: %u\n", header.bits_per_sample);
        fprintf(stderr, "Channels: %u\n", header.num_channels);
        fprintf(stderr, "Audio Format: %u\n", header.audio_format);
        fprintf(stderr, "Subchunk1 Size: %u\n", header.subchunk1_size);
        fprintf(stderr, "Data Size: %u\n", header.subchunk2_size);
        fprintf(stderr, "Samples per char: %d\n", encoder->tone_samples);
    }
    
    if (memcmp(header.chunk_id, "RIFF", 4) != 0 ||
        memcmp(header.format, "WAVE", 4) != 0 ||
        memcmp(header.subchunk1_id, "fmt ", 4) != 0) {
        fprintf(stderr, "Error: Invalid WAV header\n");
        fclose(fp);
        return NULL;
    }

    encoder->sample_rate = header.sample_rate;
    
    float* buffer = malloc(encoder->tone_samples * sizeof(float));
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    
    size_t result_capacity = 256;
    char* result = malloc(result_capacity);
    if (!result) {
        free(buffer);
        fclose(fp);
        return NULL;
    }
    
    int buffer_pos = 0;
    size_t result_size = 0;
    size_t samples_read = 0;
    int16_t sample;
    
    // Skip to the data chunk
    while (1) {
        if (fread(header.chunk_id, 1, 4, fp) != 4) break;
        if (fread(&header.chunk_size, 4, 1, fp) != 1) break;
        if (memcmp(header.chunk_id, "data", 4) == 0) break;
        fseek(fp, header.chunk_size, SEEK_CUR);
    }
    
    if (encoder->verbose) {
        fprintf(stderr, "Starting to process samples...\n");
    }
    
    while (fread(&sample, sizeof(int16_t), 1, fp) == 1) {
        buffer[buffer_pos++] = sample / 32767.0f;
        samples_read++;
        
        if (buffer_pos == encoder->tone_samples) {
            if (encoder->verbose) {
                fprintf(stderr, "\nProcessing chunk at sample %zu\n", samples_read - buffer_pos);
            }
            
            char decoded = dtmf_decode_chunk(buffer, encoder->tone_samples, encoder);
            
            if (decoded != '\0') {
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
            }
            
            buffer_pos = 0;
            
            // Skip pause samples
            fseek(fp, encoder->pause_samples * sizeof(int16_t), SEEK_CUR);
            samples_read += encoder->pause_samples;
        }
    }
    
    if (encoder->verbose) {
        fprintf(stderr, "Total samples processed: %zu\n", samples_read);
    }
    
    result[result_size] = '\0';
    
    free(buffer);
    fclose(fp);
    return result;
}

int dtmf_decode_stream(FILE* fp, DTMFEncoder* encoder, int skip_header) {
    if (skip_header) {
        WavHeader header;
        if (fread(&header, sizeof(header), 1, fp) != 1) return -1;
        encoder->sample_rate = header.sample_rate;
    }
    
    float* buffer = malloc(encoder->tone_samples * sizeof(float));
    int16_t* raw_buffer = malloc((encoder->tone_samples + encoder->pause_samples) * sizeof(int16_t));
    if (!buffer || !raw_buffer) {
        free(buffer);
        free(raw_buffer);
        return -1;
    }
    
    size_t samples_read;
    while ((samples_read = fread(raw_buffer, sizeof(int16_t), 
           encoder->tone_samples + encoder->pause_samples, fp)) > 0) {
        
        if ((int)samples_read < encoder->tone_samples) break;
        
        for (int i = 0; i < encoder->tone_samples; i++) {
            buffer[i] = raw_buffer[i] / 32767.0f;
        }
        
        char decoded = dtmf_decode_chunk(buffer, encoder->tone_samples, encoder);
        if (decoded != '\0') {
            putchar(decoded);
            fflush(stdout);
        }
    }
    
    free(buffer);
    free(raw_buffer);
    return 0;
}

int dtmf_decode_file_stream(const char* filename, DTMFEncoder* encoder) {
    FILE* fp;
    int skip_header = 1;
    
    if (strcmp(filename, "-") == 0) {
        fp = stdin;
        skip_header = 0;
    } else {
        fp = fopen(filename, "rb");
        if (!fp) return -1;
    }
    
    int result = dtmf_decode_stream(fp, encoder, skip_header);
    
    if (fp != stdin) {
        fclose(fp);
    }
    
    return result;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s encode <text|->  [output] [--verbose]     (use - for stdin)\n", argv[0]);
        printf("       %s decode <file> [--stream] [--verbose]      (--stream for real-time output)\n", argv[0]);
        return 1;
    }
    
    DTMFEncoder* encoder = dtmf_create_encoder();
    if (!encoder) {
        printf("Failed to initialize encoder\n");
        return 1;
    }
    
    // Check for verbose flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            encoder->verbose = 1;
            break;
        }
    }
    
    if (strcmp(argv[1], "encode") == 0) {
        const char* output = "output.wav";  // Default output name
        
        // Handle output filename if provided
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--verbose") != 0) {
                output = argv[i];
                break;
            }
        }
        
        if (strcmp(argv[2], "-") == 0) {
            char* buffer = NULL;
            size_t total_size = 0;
            size_t alloc_size = 1024;
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
        int stream_mode = 0;
        
        // Parse flags
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--stream") == 0) {
                stream_mode = 1;
            }
        }

        if (stream_mode) {
            if (dtmf_decode_file_stream(argv[2], encoder) != 0) {
                printf("\nFailed to decode file\n");
                dtmf_destroy_encoder(encoder);
                return 1;
            }
            putchar('\n');
        } else {
            char* decoded = dtmf_decode_file(argv[2], encoder);
            if (!decoded) {
                printf("Failed to decode file\n");
                dtmf_destroy_encoder(encoder);
                return 1;
            }
            if (strlen(decoded) > 0) {
                printf("Decoded text: %s\n", decoded);
            } else {
                printf("No valid DTMF tones detected\n");
            }
            free(decoded);
        }
    }
    
    dtmf_destroy_encoder(encoder);
    return 0;
}
