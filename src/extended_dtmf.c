/*
 * extended_dtmf.c
 *
 * Demonstrates a "superset of DTMF" with 256 possible byte values.
 * Each byte is mapped to (rowFreq[rowIndex], colFreq[colIndex]),
 * where rowIndex = (byte >> 4), colIndex = (byte & 0x0F).
 *
 * Encoding:
 *   -e: Read raw bytes from file/stdin, encode them to a wave file of dual-tone signals.
 * Decoding:
 *   -d: Read a wave file of extended DTMF data, decode it back to raw bytes.
 *       (Now uses variable-length tone detection, not fixed 50 ms chunks.)
 *
 * Usage:
 *   extended_dtmf [options]
 *     -e             encode
 *     -d             decode
 *     -i <file>      input file (defaults to stdin)
 *     -o <file>      output file (defaults to stdout)
 *     -v             verbose
 *     -r             real-time decode (flush output after each byte)
 *     -h             help
 *
 * NOTE: This still uses naive signal generation and correlation-based detection.
 *       It is not robust against noise, but is sufficient as a demonstration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>

/* ------------------ Configuration Constants ------------------ */

/* Sample rate (Hz) */
#define SAMPLE_RATE     16000

/* Duration (in seconds) per symbol (i.e., per byte) when ENCODING */
#define SYMBOL_DURATION 0.1  /* 50 ms */

/* Number of samples per symbol (for ENCODING only) */
#define SAMPLES_PER_SYMBOL  ((int)(SAMPLE_RATE * SYMBOL_DURATION))

/* Amplitude for each tone (summed wave may clip if you pick large values) */
#define AMPLITUDE 10000

/*
 * For DECODING, we use a smaller "analysis window" to detect stable tones of arbitrary length.
 * For instance, 20 ms:
 */
#define WINDOW_MS   20
#define WINDOW_SIZE (SAMPLE_RATE * WINDOW_MS / 1000)

/*
 * Number of consecutive windows that must match the same tone before we finalize a symbol.
 * If each symbol is 50 ms, that's about 2.5 windows at 20 ms each, so '2' or '3' might work.
 */
#define MIN_STABLE_WINDOWS 2

/*
 * If the correlation magnitude is below this threshold, we consider it "silence" (no valid tone).
 * Tweak as needed.
 */
#define SILENCE_THRESHOLD 0.02

/* Duration of silence gap between characters (in seconds) */
#define GAP_DURATION 0.015  /* 10 ms */

/* Number of silence samples at 8 kHz */
#define GAP_SAMPLES  ((int)(SAMPLE_RATE * GAP_DURATION))

/* We have 16 possible row frequencies and 16 possible column frequencies,
   giving 256 unique pairs. For bytes 0..15, we match standard DTMF freq sets
   (0..3 row, 0..3 col). Then for 4..15, define arbitrary extended frequencies. */

/* Standard DTMF row frequencies for row indices [0..3]: 697, 770, 852, 941 Hz */
static double standardRow[4]  = {697.0,  770.0,  852.0,  941.0};
/* Standard DTMF col frequencies for col indices [0..3]: 1209, 1336, 1477, 1633 Hz */
static double standardCol[4]  = {1209.0, 1336.0, 1477.0, 1633.0};

/* Extended row frequencies for row indices [4..15]. Just pick some distinct ones. */
static double extendedRow[12] = {
    1000.0, 1060.0, 1120.0, 1180.0,
    1240.0, 1300.0, 1360.0, 1420.0,
    1480.0, 1540.0, 1600.0, 1660.0
};

/* Extended column frequencies for col indices [4..15]. */
static double extendedCol[12] = {
    1700.0, 1780.0, 1860.0, 1940.0,
    2020.0, 2100.0, 2180.0, 2260.0,
    2340.0, 2420.0, 2500.0, 2580.0
};

/* We'll fill these at runtime with row/column frequencies for [0..15]. */
static double rowFreq[16];
static double colFreq[16];

/* ------------------ WAV I/O Helpers ------------------ */

/*
 * Write a minimal 44-byte WAV header for 16-bit PCM, 1 channel.
 * dataSize = the number of bytes of audio data that follows.
 */
static void write_wav_header(FILE *out, int dataSize, bool verbose)
{
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

    uint32_t sampleRate = SAMPLE_RATE;
    fwrite(&sampleRate, 4, 1, out);

    uint32_t byteRate = SAMPLE_RATE * numChannels * 2; /* 16-bit = 2 bytes */
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

/*
 * A "robust" WAV header reader that skips unknown chunks until it finds "data".
 * Returns size of the data chunk, or -1 on error.
 */
static long robust_read_wav_header(FILE *in, bool verbose)
{
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
            /* We reached EOF without finding data. */
            fprintf(stderr, "Reached EOF without 'data' chunk.\n");
            return -1;
        }

        uint32_t chunkSize;
        memcpy(&chunkSize, chunkHeader + 4, 4);

        if (!memcmp(chunkHeader, "fmt ", 4)) {
            /* skip or parse format chunk */
            fseek(in, chunkSize, SEEK_CUR);
        }
        else if (!memcmp(chunkHeader, "data", 4)) {
            dataSize = chunkSize;
            if (verbose) {
                fprintf(stderr, "Found 'data' chunk (size=%ld)\n", dataSize);
            }
            break;
        }
        else {
            /* skip unknown chunk */
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

/* ------------------ Encoding (unchanged) ------------------ */

/*
 * Generate SAMPLES_PER_SYMBOL of 16-bit audio data for the given byte
 * (rowFreq[rowIndex], colFreq[colIndex]) and write to 'out'.
 */
static void encode_symbol(uint8_t byteVal, FILE *out)
{
    int rowIndex = byteVal >> 4;   /* top nibble */
    int colIndex = byteVal & 0x0F; /* bottom nibble */

    double f1 = rowFreq[rowIndex];
    double f2 = colFreq[colIndex];

    for (int n = 0; n < SAMPLES_PER_SYMBOL; n++) {
        double t = (double)n / (double)SAMPLE_RATE;
        double s1 = AMPLITUDE * sin(2.0 * M_PI * f1 * t);
        double s2 = AMPLITUDE * sin(2.0 * M_PI * f2 * t);
        double combined = s1 + s2; /* might be up to ±2*AMPLITUDE */

        /* Clip to int16 if needed. */
        if (combined > 32767.0)  combined = 32767.0;
        if (combined < -32768.0) combined = -32768.0;

        int16_t sample = (int16_t)combined;
        fwrite(&sample, sizeof(int16_t), 1, out);
    }
}

/**
 * Generate a gap of silence (GAP_DURATION) between symbols.
 */
static void encode_gap(FILE *out)
{
    int16_t zero = 0;
    for (int i = 0; i < GAP_SAMPLES; i++) {
        fwrite(&zero, sizeof(int16_t), 1, out);
    }
}

/*
 * dtmf_encode(): read entire input, produce wave with one 50 ms symbol per byte.
 */
int dtmf_encode(FILE *fin, FILE *fout, bool verbose)
{
    fseek(fin, 0, SEEK_END);
    long fsize = ftell(fin);
    if (fsize < 0) {
        fprintf(stderr, "Error: cannot determine input size.\n");
        return 1;
    }
    fseek(fin, 0, SEEK_SET);

    if (fsize == 0) {
        if (verbose) {
            fprintf(stderr, "No input data; writing empty wave.\n");
        }
        write_wav_header(fout, 0, verbose);
        return 0;
    }

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

    /* total wave bytes = (# of bytes) * (samples per symbol) * (2 bytes/sample) */
    long waveDataSize = fsize * (SAMPLES_PER_SYMBOL * sizeof(int16_t));
    write_wav_header(fout, waveDataSize, verbose);

    for (long i = 0; i < fsize; i++) {
        encode_symbol(buffer[i], fout);
        encode_gap(fout);
    }

    free(buffer);

    if (verbose) {
        fprintf(stderr, "Encoded %ld bytes -> %ld bytes of audio.\n", fsize, waveDataSize);
    }
    return 0;
}

/* ------------------ Decoding with Arbitrary-Length Tones ------------------ */

/*
 * We'll process the audio in short windows (WINDOW_SIZE samples) to see which
 * row/col freq is strongest. Then we'll track stable (row,col) over multiple
 * windows until we see a change or silence, finalizing that as one symbol.
 */

/* Correlation reference arrays for each row/col freq, sized for WINDOW_SIZE. */
static double rowRef[16][WINDOW_SIZE];
static double colRef[16][WINDOW_SIZE];

/*
 * Populate rowRef[i][n] with sin(2*pi*rowFreq[i]*t) for n in [0..WINDOW_SIZE-1].
 * Same for colRef.
 */
static void build_references_arbitrary(void)
{
    for (int i = 0; i < 16; i++) {
        double fRow = rowFreq[i];
        double fCol = colFreq[i];
        for (int n = 0; n < WINDOW_SIZE; n++) {
            double t = (double)n / (double)SAMPLE_RATE;
            rowRef[i][n] = sin(2.0 * M_PI * fRow * t);
            colRef[i][n] = sin(2.0 * M_PI * fCol * t);
        }
    }
}

/* Correlate the given 'window' (WINDOW_SIZE samples) against rowRef/colRef,
 * return the best row & col index, plus a "magnitude" that indicates confidence.
 */
static void correlate_window(const int16_t *samples,
                             int *bestRow, int *bestCol,
                             double *bestMag)
{
    double maxRowVal = -1e30;
    double maxColVal = -1e30;
    int rowIdx = 0;
    int colIdx = 0;

    for (int r = 0; r < 16; r++) {
        double sum = 0.0;
        for (int n = 0; n < WINDOW_SIZE; n++) {
            double s = (double)samples[n] / 32768.0;
            sum += s * rowRef[r][n];
        }
        if (sum > maxRowVal) {
            maxRowVal = sum;
            rowIdx = r;
        }
    }

    for (int c = 0; c < 16; c++) {
        double sum = 0.0;
        for (int n = 0; n < WINDOW_SIZE; n++) {
            double s = (double)samples[n] / 32768.0;
            sum += s * colRef[c][n];
        }
        if (sum > maxColVal) {
            maxColVal = sum;
            colIdx = c;
        }
    }

    /* We'll define "magnitude" as the sum of the best row correlation + best column correlation. */
    *bestRow = rowIdx;
    *bestCol = colIdx;
    *bestMag = maxRowVal + maxColVal;
}

static void finalize_symbol(FILE *fout, bool stream, bool verbose, int row, int col)
{
    uint8_t symbol = (uint8_t)((row << 4) | col);

    /* Write one byte to output */
    fwrite(&symbol, 1, 1, fout);

    /* Flush if real-time mode requested */
    if (stream) {
        fflush(fout);
    }

    if (verbose) {
        fprintf(stderr, "Finalized symbol (row=%d, col=%d) => 0x%02X\n",
                row, col, symbol);
    }
}

/* dtmf_decode():
 * 1) Read wave header -> total data size
 * 2) Read entire PCM data
 * 3) Process in short windows (WINDOW_SIZE)
 * 4) Identify stable tone blocks as single symbols
 */
int dtmf_decode(FILE *fin, FILE *fout, bool verbose, bool stream)
{
    long dataSize = robust_read_wav_header(fin, verbose);
    if (dataSize < 0) {
        return 1;
    }
    if (dataSize == 0) {
        if (verbose) {
            fprintf(stderr, "No wave data.\n");
        }
        return 0;
    }

    long sampleCount = dataSize / (long)sizeof(int16_t);
    if (verbose) {
        fprintf(stderr, "Reading %ld samples.\n", sampleCount);
    }

    /* Read all the samples into memory */
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

    /* Now break into windows of WINDOW_SIZE. If there's leftover, ignore it. */
    long windowCount = sampleCount / WINDOW_SIZE;
    long remainder = sampleCount % WINDOW_SIZE;
    if (verbose && remainder > 0) {
        fprintf(stderr, "Ignoring %ld leftover samples (not a full window).\n", remainder);
    }

    /* We'll track a "current recognized tone" and see how many consecutive windows it persists. */
    enum { STATE_SILENCE, STATE_TONE } state = STATE_SILENCE;
    int currentRow = -1;
    int currentCol = -1;
    int stableCount = 0;

    // /* Helper function to finalize a recognized tone as a single byte. */
    // auto void finalize_symbol(int row, int col) {
    //     uint8_t symbol = (uint8_t)((row << 4) | col);
    //     fwrite(&symbol, 1, 1, fout);
    //     if (stream) {
    //         fflush(fout);
    //     }
    //     if (verbose) {
    //         fprintf(stderr, "Finalized symbol (row=%d, col=%d) => 0x%02X\n",
    //                 row, col, symbol);
    //     }
    // }

    for (long w = 0; w < windowCount; w++) {
        int16_t *windowPtr = &pcmData[w * WINDOW_SIZE];

        int bestRow, bestCol;
        double bestMag;
        correlate_window(windowPtr, &bestRow, &bestCol, &bestMag);

        /* Normalize by number of samples to compare to threshold. */
        double normMag = bestMag / (double)WINDOW_SIZE;

        bool isSilent = (fabs(normMag) < SILENCE_THRESHOLD);

        switch (state) {
            case STATE_SILENCE:
                if (!isSilent) {
                    /* Start tracking this new tone. */
                    currentRow = bestRow;
                    currentCol = bestCol;
                    stableCount = 1;
                    state = STATE_TONE;
                }
            break;

            case STATE_TONE:
                if (isSilent) {
                    if (stableCount >= MIN_STABLE_WINDOWS) {
                        finalize_symbol(fout, stream, verbose, currentRow, currentCol);
                    }
                    // transition to SILENCE, etc.
                } else {
                    if (bestRow == currentRow && bestCol == currentCol) {
                        stableCount++;
                    } else {
                        if (stableCount >= MIN_STABLE_WINDOWS) {
                            finalize_symbol(fout, stream, verbose, currentRow, currentCol);
                        }
                        currentRow = bestRow;
                        currentCol = bestCol;
                        stableCount = 1;
                    }
                }
            break;
        }
    }

    /* End of file. If we ended in the TONE state, finalize if stable. */
    // if (state == STATE_TONE && stableCount >= MIN_STABLE_WINDOWS) {
    //     finalize_symbol(currentRow, currentCol);
    // }
    if (state == STATE_TONE && stableCount >= MIN_STABLE_WINDOWS) {
        finalize_symbol(fout, stream, verbose, currentRow, currentCol);
    }

    free(pcmData);
    return 0;
}

/* ------------------ CLI / Main ------------------ */

static void print_usage(const char *progName)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "  -e             Encode input (raw bytes) into extended DTMF wave\n"
        "  -d             Decode extended DTMF wave back to raw bytes (arbitrary tone length)\n"
        "  -i <file>      Input file (defaults to stdin)\n"
        "  -o <file>      Output file (defaults to stdout)\n"
        "  -v             Verbose\n"
        "  -r             Real-time decode (flush each byte)\n"
        "  -h             Help\n",
        progName
    );
}

static void init_frequencies(void)
{
    /* Fill rowFreq[0..3], colFreq[0..3] with standard DTMF */
    for (int i = 0; i < 4; i++) {
        rowFreq[i] = standardRow[i];
        colFreq[i] = standardCol[i];
    }
    /* Fill rowFreq[4..15], colFreq[4..15] with extended sets */
    for (int i = 4; i < 16; i++) {
        rowFreq[i] = extendedRow[i - 4];
        colFreq[i] = extendedCol[i - 4];
    }
}

int main(int argc, char *argv[])
{
    bool encode = false;
    bool decodeFlag = false;
    bool verbose = false;
    bool realtime = false;

    char *inputFile = NULL;
    char *outputFile = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "edi:o:vrh")) != -1) {
        switch (opt) {
            case 'e':
                encode = true;
                break;
            case 'd':
                decodeFlag = true;
                break;
            case 'i':
                inputFile = optarg;
                break;
            case 'o':
                outputFile = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'r':
                realtime = true;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return (opt == 'h') ? 0 : 1;
        }
    }

    if ((encode && decodeFlag) || (!encode && !decodeFlag)) {
        fprintf(stderr, "Error: Must specify either -e or -d (but not both).\n");
        print_usage(argv[0]);
        return 1;
    }

    FILE *fin = stdin;
    FILE *fout = stdout;

    if (inputFile) {
        fin = fopen(inputFile, "rb");
        if (!fin) {
            perror("fopen inputFile");
            return 1;
        }
    }
    if (outputFile) {
        fout = fopen(outputFile, "wb");
        if (!fout) {
            perror("fopen outputFile");
            if (fin != stdin) fclose(fin);
            return 1;
        }
    }

    /* Initialize rowFreq/colFreq arrays */
    init_frequencies();

    if (encode) {
        /* Just encode as before */
        return dtmf_encode(fin, fout, verbose);
    }

    /* decodeFlag = true => decode with new arbitrary-length tone approach */

    /* Build references for short-window correlation */
    build_references_arbitrary();

    /* Decode */
    int ret = dtmf_decode(fin, fout, verbose, realtime);

    if (fin && fin != stdin) fclose(fin);
    if (fout && fout != stdout) fclose(fout);

    return ret;
}
