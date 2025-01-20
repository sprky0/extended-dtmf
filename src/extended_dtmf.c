/*
 * extended_dtmf.c
 *
 * Implementation of extended DTMF encoding/decoding.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "extended_dtmf.h"

/* ------------------ Internal Constants ------------------ */

/* Derived constants */
#define SAMPLES_PER_SYMBOL ((int)(DTMF_SAMPLE_RATE * DTMF_SYMBOL_DURATION))
#define GAP_SAMPLES        ((int)(DTMF_SAMPLE_RATE * DTMF_GAP_DURATION))
#define WINDOW_SIZE        ((int)(DTMF_SAMPLE_RATE * DTMF_WINDOW_MS / 1000))

/* Wave generation amplitude */
#define AMPLITUDE          10000

/* Detection parameters */
#define SILENCE_THRESHOLD  0.02
#define MIN_MAG_RATIO      0.3    /* Minimum ratio between strongest/weakest tone */

/* ------------------ Frequency Tables ------------------ */

/* Standard DTMF frequencies (0-3 indices) */
static const double standardRow[4] = {697.0,  770.0,  852.0,  941.0};
static const double standardCol[4] = {1209.0, 1336.0, 1477.0, 1633.0};

/* Extended frequencies (4-15 indices) with wider spacing */
static const double extendedRow[12] = {
    1050.0, 1200.0, 1350.0, 1500.0,  /* +150Hz steps */
    1700.0, 1900.0, 2100.0, 2300.0,  /* +200Hz steps */
    2550.0, 2800.0, 3050.0, 3300.0   /* +250Hz steps */
};

static const double extendedCol[12] = {
    2000.0, 2300.0, 2600.0, 2900.0,  /* +300Hz steps */
    3300.0, 3700.0, 4100.0, 4500.0,  /* +400Hz steps */
    5000.0, 5500.0, 6000.0, 6500.0   /* +500Hz steps */
};

/* Complete frequency tables filled at init */
static double rowFreq[16];
static double colFreq[16];

/* ------------------ Goertzel Algorithm ------------------ */

typedef struct {
    double coeff;
    double q1;
    double q2;
} goertzel_state;

static void goertzel_init(goertzel_state *state, double frequency, double sample_rate) {
    double omega = 2.0 * M_PI * frequency / sample_rate;
    state->coeff = 2.0 * cos(omega);
    state->q1 = 0;
    state->q2 = 0;
}

static void goertzel_process(goertzel_state *state, double sample) {
    double q0 = state->coeff * state->q1 - state->q2 + sample;
    state->q2 = state->q1;
    state->q1 = q0;
}

static double goertzel_magnitude(const goertzel_state *state) {
    return sqrt(state->q1 * state->q1 + state->q2 * state->q2 
              - state->coeff * state->q1 * state->q2);
}

/* ------------------ WAV File Handling ------------------ */

static void write_wav_header(FILE *out, int dataSize, bool verbose) {
    int overallSize = 36 + dataSize; /* for "RIFF" chunkSize field */

    fwrite("RIFF", 1, 4, out);
    uint32_t riffSize = (uint32_t)overallSize;
    fwrite(&riffSize, 4, 1, out);

    fwrite("WAVE", 1, 4, out);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, out);
    uint32_t fmtChunkSize = 16;
    fwrite(&fmtChunkSize, 4, 1, out);

    uint16_t audioFormat = 1; /* PCM */
    fwrite(&audioFormat, 2, 1, out);

    uint16_t numChannels = 1;
    fwrite(&numChannels, 2, 1, out);

    uint32_t sampleRate = DTMF_SAMPLE_RATE;
    fwrite(&sampleRate, 4, 1, out);

    uint32_t byteRate = DTMF_SAMPLE_RATE * numChannels * 2;  /* 16-bit = 2 bytes */
    fwrite(&byteRate, 4, 1, out);

    uint16_t blockAlign = numChannels * 2;
    fwrite(&blockAlign, 2, 1, out);

    uint16_t bitsPerSample = 16;
    fwrite(&bitsPerSample, 2, 1, out);

    /* data chunk */
    fwrite("data", 1, 4, out);
    uint32_t dSize = (uint32_t)dataSize;
    fwrite(&dSize, 4, 1, out);

    if (verbose) {
        fprintf(stderr, "WAV header written (dataSize=%d)\n", dataSize);
    }
}

static long read_wav_header(FILE *in, bool verbose) {
    char riffHeader[12];
    if (fread(riffHeader, 1, 12, in) != 12) {
        fprintf(stderr, "Error reading initial RIFF header.\n");
        return -1;
    }
    if (memcmp(riffHeader, "RIFF", 4) != 0 || memcmp(riffHeader + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "Not a valid RIFF/WAVE file.\n");
        return -1;
    }

    long dataSize = -1;

    while (1) {
        unsigned char chunkHeader[8];
        if (fread(chunkHeader, 1, 8, in) != 8) {
            fprintf(stderr, "Reached EOF without 'data' chunk.\n");
            return -1;
        }

        uint32_t chunkSize;
        memcpy(&chunkSize, chunkHeader + 4, 4);

        if (!memcmp(chunkHeader, "fmt ", 4)) {
            /* Verify format is compatible */
            uint16_t format, channels, bits;
            uint32_t rate;
            
            if (fread(&format, 2, 1, in) != 1 || format != 1 ||  /* PCM */
                fread(&channels, 2, 1, in) != 1 || channels != 1 ||  /* Mono */
                fread(&rate, 4, 1, in) != 1 || rate != DTMF_SAMPLE_RATE ||
                fseek(in, 6, SEEK_CUR) != 0 ||  /* Skip byteRate and blockAlign */
                fread(&bits, 2, 1, in) != 1 || bits != 16) {  /* 16-bit */
                
                fprintf(stderr, "Unsupported wave format (need: mono 16-bit PCM at %d Hz)\n",
                        DTMF_SAMPLE_RATE);
                return -1;
            }
            
            /* Skip any extra format bytes */
            if (chunkSize > 16) {
                fseek(in, chunkSize - 16, SEEK_CUR);
            }
        }
        else if (!memcmp(chunkHeader, "data", 4)) {
            dataSize = chunkSize;
            if (verbose) {
                fprintf(stderr, "Found 'data' chunk (size=%ld)\n", dataSize);
            }
            break;
        }
        else {
            /* Skip unknown chunk */
            if (verbose) {
                char id[5];
                memcpy(id, chunkHeader, 4);
                id[4] = '\0';
                fprintf(stderr, "Skipping chunk '%s' (%u bytes)\n", id, chunkSize);
            }
            fseek(in, chunkSize, SEEK_CUR);
        }
    }

    return dataSize;
}

/* ------------------ Encoding ------------------ */

static void encode_symbol(uint8_t byteVal, FILE *out) {
    int rowIndex = byteVal >> 4;   /* top nibble */
    int colIndex = byteVal & 0x0F; /* bottom nibble */
    int fadeSamples = (int)(DTMF_SAMPLE_RATE * DTMF_FADE_MS / 1000.0);

    double f1 = rowFreq[rowIndex];
    double f2 = colFreq[colIndex];

    for (int n = 0; n < SAMPLES_PER_SYMBOL; n++) {
        double t = (double)n / DTMF_SAMPLE_RATE;
        double s1 = AMPLITUDE * sin(2.0 * M_PI * f1 * t);
        double s2 = AMPLITUDE * sin(2.0 * M_PI * f2 * t);
        double combined = s1 + s2;

        /* Apply fade envelope */
        double fadeMultiplier = 1.0;
        if (n < fadeSamples) {
            /* Fade in */
            fadeMultiplier = (double)n / fadeSamples;
        } else if (n > SAMPLES_PER_SYMBOL - fadeSamples) {
            /* Fade out */
            fadeMultiplier = (double)(SAMPLES_PER_SYMBOL - n) / fadeSamples;
        }
        combined *= fadeMultiplier;

        /* Clip to int16 range */
        if (combined > 32767.0)  combined = 32767.0;
        if (combined < -32768.0) combined = -32768.0;

        int16_t sample = (int16_t)combined;
        fwrite(&sample, sizeof(int16_t), 1, out);
    }
}

static void encode_gap(FILE *out) {
    int16_t zero = 0;
    for (int i = 0; i < GAP_SAMPLES; i++) {
        fwrite(&zero, sizeof(int16_t), 1, out);
    }
}

/* ------------------ Decoding ------------------ */


static void detect_frequencies(const int16_t *samples, int *bestRow,   int *bestCol, double *rowMag, double *colMag) {
    double maxRowMag = -1.0;
    double maxColMag = -1.0;
    int rowIdx = 0;
    int colIdx = 0;
    goertzel_state state;

    /* Check row frequencies */
    for (int r = 0; r < 16; r++) {
        goertzel_init(&state, rowFreq[r], DTMF_SAMPLE_RATE);
        
        for (int n = 0; n < WINDOW_SIZE; n++) {
            double sample = (double)samples[n] / 32768.0;
            goertzel_process(&state, sample);
        }
        
        double mag = goertzel_magnitude(&state);
        if (mag > maxRowMag) {
            maxRowMag = mag;
            rowIdx = r;
        }
    }
    
    /* Check column frequencies */
    for (int c = 0; c < 16; c++) {
        goertzel_init(&state, colFreq[c], DTMF_SAMPLE_RATE);
        
        for (int n = 0; n < WINDOW_SIZE; n++) {
            double sample = (double)samples[n] / 32768.0;
            goertzel_process(&state, sample);
        }
        
        double mag = goertzel_magnitude(&state);
        if (mag > maxColMag) {
            maxColMag = mag;
            colIdx = c;
        }
    }
    
    *bestRow = rowIdx;
    *bestCol = colIdx;
    *rowMag = maxRowMag;
    *colMag = maxColMag;
}

static bool validate_tone(int row, int col, double rowMag, double colMag) {
    /* Check magnitude ratio between frequencies */
    double ratio = (rowMag < colMag) ? 
                  (rowMag / colMag) : 
                  (colMag / rowMag);
                  
    if (ratio < MIN_MAG_RATIO) {
        return false;
    }

    /* Basic range validation */
    if (row < 0 || row >= 16 || col < 0 || col >= 16) {
        return false;
    }

    /* Additional magnitude check - both tones should be strong enough */
    if (rowMag < SILENCE_THRESHOLD || colMag < SILENCE_THRESHOLD) {
        return false;
    }

    return true;
}

static void finalize_symbol(FILE *fout, bool stream, bool verbose,
                          int row, int col)
{
    uint8_t symbol = (uint8_t)((row << 4) | col);
    fwrite(&symbol, 1, 1, fout);
    if (stream) {
        fflush(fout);
    }
    if (verbose) {
        fprintf(stderr, "Symbol: row=%d, col=%d => 0x%02X (%c)\n",
                row, col, symbol, 
                (symbol >= 32 && symbol <= 126) ? symbol : '.');
    }
}

/* ------------------ Public Interface ------------------ */

int dtmf_init(void) {
    /* Fill frequency tables */
    for (int i = 0; i < 4; i++) {
        rowFreq[i] = standardRow[i];
        colFreq[i] = standardCol[i];
    }
    for (int i = 4; i < 16; i++) {
        rowFreq[i] = extendedRow[i - 4];
        colFreq[i] = extendedCol[i - 4];
    }
    return 0;
}


int dtmf_encode(FILE *fin, FILE *fout, bool verbose) {
    /* Get input size */
    fseek(fin, 0, SEEK_END);
    long fsize = ftell(fin);
    if (fsize < 0) {
        fprintf(stderr, "Error: cannot determine input size.\n");
        return 1;
    }
    fseek(fin, 0, SEEK_SET);

    /* Handle empty input */
    if (fsize == 0) {
        if (verbose) {
            fprintf(stderr, "No input data.\n");
        }
        write_wav_header(fout, 0, verbose);
        return 0;
    }

    /* Read input */
    uint8_t *buffer = (uint8_t *)malloc(fsize);
    if (!buffer) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }
    if (fread(buffer, 1, fsize, fin) != (size_t)fsize) {
        fprintf(stderr, "Error reading input.\n");
        free(buffer);
        return 1;
    }

    /* Calculate output size and write header */
    long waveDataSize = fsize * (SAMPLES_PER_SYMBOL * sizeof(int16_t));
    write_wav_header(fout, waveDataSize, verbose);

    /* Encode each byte */
    for (long i = 0; i < fsize; i++) {
        encode_symbol(buffer[i], fout);
        encode_gap(fout);
    }

    free(buffer);
    if (verbose) {
        fprintf(stderr, "Encoded %ld bytes -> %ld bytes of audio.\n",
                fsize, waveDataSize);
    }
    return 0;
}

int dtmf_decode(FILE *fin, FILE *fout, bool verbose, bool stream) {
    /* Read wave header */
    long dataSize = read_wav_header(fin, verbose);
    if (dataSize < 0) {
        return 1;
    }
    if (dataSize == 0) {
        if (verbose) {
            fprintf(stderr, "No wave data.\n");
        }
        return 0;
    }

    /* Read all samples */
    long sampleCount = dataSize / sizeof(int16_t);
    long windowCount = sampleCount / WINDOW_SIZE;
    long remainder = sampleCount % WINDOW_SIZE;
    
    if (verbose) {
        fprintf(stderr, "Reading %ld samples from wave file...\n", sampleCount);
    }

    int16_t *pcmData = (int16_t *)malloc(dataSize);
    if (!pcmData) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }
    
    if (fread(pcmData, sizeof(int16_t), sampleCount, fin) != (size_t)sampleCount) {
        fprintf(stderr, "Short read on PCM data.\n");
        free(pcmData);
        return 1;
    }

    if (verbose) {
        fprintf(stderr, "Processing %ld windows of %d samples", 
                windowCount, WINDOW_SIZE);
        if (remainder) {
            fprintf(stderr, " (ignoring %ld trailing samples)", remainder);
        }
        fprintf(stderr, "\n");
    }

    enum { 
        STATE_SILENCE,    /* Waiting for a tone */
        STATE_TONE,       /* Currently tracking a tone */
        STATE_GAP         /* In a gap between tones */
    } state = STATE_SILENCE;

    int currentRow = -1;
    int currentCol = -1;
    int stableCount = 0;
    int silenceCount = 0;
    long symbolsDecoded = 0;
    
    const int MIN_SILENCE_WINDOWS = (int)(DTMF_GAP_DURATION * DTMF_SAMPLE_RATE / WINDOW_SIZE);
    const int MAX_SILENCE_WINDOWS = MIN_SILENCE_WINDOWS * 3;  // Allow for longer gaps

    for (long w = 0; w < windowCount; w++) {
        int16_t *windowPtr = &pcmData[w * WINDOW_SIZE];
        int bestRow, bestCol;
        double rowMag, colMag;

        detect_frequencies(windowPtr, &bestRow, &bestCol, &rowMag, &colMag);

        /* Normalize magnitudes */
        rowMag /= WINDOW_SIZE;
        colMag /= WINDOW_SIZE;
        double totalMag = rowMag + colMag;
        bool isSilent = (totalMag < SILENCE_THRESHOLD);

        if (verbose) {
            fprintf(stderr, "Window %ld: row=%d col=%d rmag=%.3f cmag=%.3f %s state=%d stable=%d silence=%d\n",
                    w, bestRow, bestCol, rowMag, colMag,
                    isSilent ? "(silent)" : "", state, stableCount, silenceCount);
        }

        switch (state) {
            case STATE_SILENCE:
                if (!isSilent && validate_tone(bestRow, bestCol, rowMag, colMag)) {
                    /* Found start of new tone */
                    currentRow = bestRow;
                    currentCol = bestCol;
                    stableCount = 1;
                    silenceCount = 0;
                    state = STATE_TONE;
                    if (verbose) {
                        fprintf(stderr, "Found tone start: row=%d col=%d\n",
                                currentRow, currentCol);
                    }
                }
                break;

            case STATE_TONE:
                if (isSilent) {
                    silenceCount++;
                    if (silenceCount >= MIN_SILENCE_WINDOWS) {
                        /* Enough silence to consider tone ended */
                        if (stableCount >= DTMF_MIN_STABLE) {
                            finalize_symbol(fout, stream, verbose, currentRow, currentCol);
                            symbolsDecoded++;
                        }
                        state = STATE_GAP;
                        if (verbose) {
                            fprintf(stderr, "Enter gap after %d stable windows\n",
                                    stableCount);
                        }
                    }
                } else if (validate_tone(bestRow, bestCol, rowMag, colMag)) {
                    silenceCount = 0;  // Reset silence counter on valid tone
                    if (bestRow == currentRow && bestCol == currentCol) {
                        stableCount++;
                    } else {
                        /* Different tone detected */
                        if (stableCount >= DTMF_MIN_STABLE) {
                            finalize_symbol(fout, stream, verbose, currentRow, currentCol);
                            symbolsDecoded++;
                        }
                        currentRow = bestRow;
                        currentCol = bestCol;
                        stableCount = 1;
                        if (verbose) {
                            fprintf(stderr, "Tone change: row=%d col=%d\n",
                                    currentRow, currentCol);
                        }
                    }
                }
                break;

            case STATE_GAP:
                if (!isSilent && validate_tone(bestRow, bestCol, rowMag, colMag)) {
                    /* New tone after gap */
                    currentRow = bestRow;
                    currentCol = bestCol;
                    stableCount = 1;
                    silenceCount = 0;
                    state = STATE_TONE;
                    if (verbose) {
                        fprintf(stderr, "Found tone after gap: row=%d col=%d\n",
                                currentRow, currentCol);
                    }
                } else {
                    silenceCount++;
                    if (silenceCount > MAX_SILENCE_WINDOWS) {
                        /* Extended silence, go back to initial state */
                        state = STATE_SILENCE;
                        if (verbose) {
                            fprintf(stderr, "Extended silence, resetting\n");
                        }
                    }
                }
                break;
        }
    }

    /* Handle final symbol if stable */
    if (state == STATE_TONE && stableCount >= DTMF_MIN_STABLE) {
        finalize_symbol(fout, stream, verbose, currentRow, currentCol);
        symbolsDecoded++;
    }

    if (verbose) {
        fprintf(stderr, "Decoded %ld symbols from %ld windows of audio\n",
                symbolsDecoded, windowCount);
    }

    free(pcmData);
    return 0;
}

/* Helper function to dump frequency tables - useful for debugging */
void dtmf_dump_frequencies(FILE *out) {
    fprintf(out, "\nExtended DTMF Frequency Tables:\n");
    fprintf(out, "\nStandard DTMF (indices 0-3):\n");
    fprintf(out, "     Row (Hz)   Col (Hz)\n");
    for (int i = 0; i < 4; i++) {
        fprintf(out, "%2d:  %7.1f   %7.1f\n", i, rowFreq[i], colFreq[i]);
    }
    
    fprintf(out, "\nExtended Set (indices 4-15):\n");
    fprintf(out, "     Row (Hz)   Col (Hz)\n");
    for (int i = 4; i < 16; i++) {
        fprintf(out, "%2d:  %7.1f   %7.1f\n", i, rowFreq[i], colFreq[i]);
    }
    
    fprintf(out, "\nValid byte values are constructed as: (row_index << 4) | col_index\n");
    fprintf(out, "For example: row=2, col=3 => 0x23, row=15, col=15 => 0xFF\n\n");
}
