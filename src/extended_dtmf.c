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
 *
 * Usage:
 *   extended_dtmf [options]
 *     -e             encode
 *     -d             decode
 *     -i <file>      input file (defaults to stdin)
 *     -o <file>      output file (defaults to stdout)
 *     -v             verbose
 *     -r             real-time decode (flush output after each chunk)
 *     -h             help
 *
 * NOTE: This example uses naive signal generation and correlation-based detection.
 *       It is not robust against noise or drift but is sufficient as a demonstration.
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
#define SAMPLE_RATE     8000

/* Duration (in seconds) per symbol (i.e., per byte) */
#define SYMBOL_DURATION 0.05  /* 50 ms */

/* Number of samples per symbol */
#define SAMPLES_PER_SYMBOL  ((int)(SAMPLE_RATE * SYMBOL_DURATION))

/* Amplitude for each tone (summed wave may clip if you pick large values) */
#define AMPLITUDE 10000

/* We have 16 possible row frequencies and 16 possible column frequencies,
   giving 256 unique pairs. For bytes 0..15, we match standard DTMF freq sets
   (0..3 row, 0..3 col). Then for 4..15, define arbitrary extended frequencies. */

/* Standard DTMF row frequencies for row indices [0..3]: 697, 770, 852, 941 Hz */
static double standardRow[4]  = {697.0,  770.0,  852.0,  941.0};
/* Standard DTMF col frequencies for col indices [0..3]: 1209,1336,1477,1633 Hz */
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

/* Row/Column frequency tables (16 each). We'll populate them at startup. */
static double rowFreq[16];
static double colFreq[16];

/* ------------------ WAVE I/O Helpers ------------------ */

/*
 * Write a simple 44-byte WAV header for 16-bit PCM, 1 channel.
 * The 'dataSize' is the size in bytes of actual wave data that follows the header.
 */
static void write_wav_header(FILE *out, int dataSize, bool verbose)
{
    /* Chunk sizes for a standard 44-byte header:
     * - RIFF chunk:  36 + dataSize
     * - fmt chunk:   16 bytes
     * - data chunk:  dataSize
     */

    /* RIFF header */
    int overallSize = 36 + dataSize; /* excludes "RIFF" itself which is 4 bytes */
    fwrite("RIFF", 1, 4, out);
    /* 4-byte size field for entire file minus 8 bytes (we have "RIFF" + this size field) */
    uint32_t riffSize = (uint32_t)overallSize;
    fwrite(&riffSize, 4, 1, out);

    /* WAVE format */
    fwrite("WAVE", 1, 4, out);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, out);
    uint32_t fmtChunkSize = 16;   /* size of the fmt chunk (PCM) */
    fwrite(&fmtChunkSize, 4, 1, out);

    /* Audio format (1 = PCM) */
    uint16_t audioFormat = 1;
    fwrite(&audioFormat, 2, 1, out);

    /* Num channels */
    uint16_t numChannels = 1;
    fwrite(&numChannels, 2, 1, out);

    /* Sample rate */
    uint32_t sampleRate = SAMPLE_RATE;
    fwrite(&sampleRate, 4, 1, out);

    /* Byte rate = sampleRate * numChannels * bitsPerSample/8 */
    uint32_t byteRate = SAMPLE_RATE * numChannels * 2;
    fwrite(&byteRate, 4, 1, out);

    /* Block align = numChannels * bitsPerSample/8 */
    uint16_t blockAlign = numChannels * 2;
    fwrite(&blockAlign, 2, 1, out);

    /* Bits per sample */
    uint16_t bitsPerSample = 16;
    fwrite(&bitsPerSample, 2, 1, out);

    /* data chunk */
    fwrite("data", 1, 4, out);
    uint32_t dSize = (uint32_t)dataSize;
    fwrite(&dSize, 4, 1, out);

    if (verbose) {
        fprintf(stderr, "WAV header written (dataSize=%d bytes)\n", dataSize);
    }
}

/*
 * Read and parse a 44-byte WAV header. Returns the data size in bytes (payload),
 * or -1 on failure.
 */
// static long read_wav_header(FILE *in, bool verbose)
// {
//     unsigned char hdr[44];
//     if (fread(hdr, 1, 44, in) != 44) {
//         fprintf(stderr, "Error reading WAV header\n");
//         return -1;
//     }

//     /* Basic checks */
//     if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr+8, "WAVE", 4) != 0) {
//         fprintf(stderr, "Not a valid RIFF/WAVE file.\n");
//         return -1;
//     }
//     if (memcmp(hdr+12, "fmt ", 4) != 0) {
//         fprintf(stderr, "Missing 'fmt ' chunk.\n");
//         return -1;
//     }
//     if (memcmp(hdr+36, "data", 4) != 0) {
//         fprintf(stderr, "Missing 'data' chunk.\n");
//         return -1;
//     }

//     /* Extract data subchunk size (bytes) from the last 4 bytes of the header */
//     uint32_t dataSize = 0;
//     memcpy(&dataSize, hdr + 40, 4);

//     if (verbose) {
//         fprintf(stderr, "WAV header ok, dataSize=%u\n", dataSize);
//     }
//     return dataSize;
// }

static long robust_read_wav_header(FILE *in, bool verbose) {
    char riffHeader[12];
    if (fread(riffHeader, 1, 12, in) != 12) {
        fprintf(stderr, "Error reading initial RIFF header.\n");
        return -1;
    }
    if (memcmp(riffHeader, "RIFF", 4) != 0 || memcmp(riffHeader+8, "WAVE", 4) != 0) {
        fprintf(stderr, "Not a valid RIFF/WAVE file.\n");
        return -1;
    }

    long dataSize = -1;
    // int gotFmt = 0; // Remove if unused

    while (1) {
        // Read next chunk header
        char chunkHeader[8];
        if (fread(chunkHeader, 1, 8, in) != 8) {
            // No more chunks
            fprintf(stderr, "Reached EOF without 'data' chunk.\n");
            return -1;
        }

        uint32_t chunkSize;
        memcpy(&chunkSize, chunkHeader + 4, 4);

        if (memcmp(chunkHeader, "fmt ", 4) == 0) {
            // We found 'fmt ' chunk
            // gotFmt = 1; // If you don't need this info, remove the variable entirely
            // Read or skip
            fseek(in, chunkSize, SEEK_CUR);
        }
        else if (memcmp(chunkHeader, "data", 4) == 0) {
            // Found data chunk
            dataSize = chunkSize;
            if (verbose) {
                // Use "%ld" instead of "%u" because dataSize is a long
                fprintf(stderr, "Found 'data' chunk (size=%ld)\n", dataSize);
            }
            break;
        }
        else {
            // Some other chunk, skip it
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

/* ------------------ Extended DTMF Encoding ------------------ */

/*
 * Generate SAMPLES_PER_SYMBOL of 16-bit audio data combining rowFreq + colFreq
 * for the given byte, and write to output.
 */
static void encode_symbol(uint8_t byteVal, FILE *out)
{
    int rowIndex = byteVal >> 4;       /* top nibble */
    int colIndex = byteVal & 0x0F;     /* bottom nibble */
    double f1 = rowFreq[rowIndex];
    double f2 = colFreq[colIndex];

    /* For each sample, we compute sum of two sines at f1 and f2. */
    for (int n = 0; n < SAMPLES_PER_SYMBOL; n++) {
        double t = (double)n / (double)SAMPLE_RATE;

        /* Simple sine wave: amplitude * sin(2*pi*freq*t) */
        double sample1 = AMPLITUDE * sin(2.0 * M_PI * f1 * t);
        double sample2 = AMPLITUDE * sin(2.0 * M_PI * f2 * t);

        double combined = sample1 + sample2;  /* might get up to ±2*AMPLITUDE */

        /* Clip to int16 range if needed. Very naive. */
        if (combined > 32767.0)  combined = 32767.0;
        if (combined < -32768.0) combined = -32768.0;

        int16_t pcm = (int16_t)(combined);
        fwrite(&pcm, sizeof(int16_t), 1, out);
    }
}

/*
 * dtmf_encode(): reads all raw bytes from input, writes a wave file with extended DTMF tones.
 */
int dtmf_encode(FILE *fin, FILE *fout, bool verbose)
{
    /* Read entire input into memory first */
    fseek(fin, 0, SEEK_END);
    long fsize = ftell(fin);
    if (fsize < 0) {
        fprintf(stderr, "Error: cannot determine input size.\n");
        return 1;
    }
    fseek(fin, 0, SEEK_SET);

    if (fsize == 0) {
        if (verbose) {
            fprintf(stderr, "No input data; encoding empty output.\n");
        }
        /* Write a valid wave with 0 data. */
        write_wav_header(fout, 0, verbose);
        return 0;
    }

    uint8_t *buffer = (uint8_t *)malloc(fsize);
    if (!buffer) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }
    if (fread(buffer, 1, fsize, fin) != (size_t)fsize) {
        fprintf(stderr, "Error reading input file.\n");
        free(buffer);
        return 1;
    }

    /* Each byte -> SAMPLES_PER_SYMBOL * 2 bytes (16-bit) in wave data */
    long waveDataSize = fsize * (SAMPLES_PER_SYMBOL * sizeof(int16_t));

    /* Write WAV header with placeholder for waveDataSize */
    write_wav_header(fout, waveDataSize, verbose);

    /* Encode each byte as dual-tone chunk */
    for (long i = 0; i < fsize; i++) {
        encode_symbol(buffer[i], fout);
    }

    free(buffer);
    if (verbose) {
        fprintf(stderr, "Encoded %ld bytes into wave data (%ld bytes of audio).\n",
                fsize, waveDataSize);
    }
    return 0;
}

/* ------------------ Extended DTMF Decoding ------------------ */

/*
 * For decoding, we read the wave data in chunks of SAMPLES_PER_SYMBOL * 2 bytes.
 * We do a naive correlation against each possible row freq (16) and col freq (16).
 * We pick the row index & col index with the highest correlation magnitude.
 *
 * In a real design, you would do windowing, AGC, or FFT-based detection. This is a
 * simplistic approach that can work in an ideal scenario.
 */

/* Pre-generate reference signals for correlation.
 * For each of the 16 row frequencies, we store an array of SAMPLES_PER_SYMBOL samples.
 * Same for the 16 column frequencies.
 */
static double rowRef[16][SAMPLES_PER_SYMBOL];
static double colRef[16][SAMPLES_PER_SYMBOL];

/*
 * Build correlation references: rowRef[i][n] = sin(2*pi*rowFreq[i]*n/SAMPLE_RATE)
 */
static void build_references(void)
{
    for (int i = 0; i < 16; i++) {
        double fRow = rowFreq[i];
        double fCol = colFreq[i];
        for (int n = 0; n < SAMPLES_PER_SYMBOL; n++) {
            double t = (double)n / (double)SAMPLE_RATE;
            rowRef[i][n] = sin(2.0 * M_PI * fRow * t);
            colRef[i][n] = sin(2.0 * M_PI * fCol * t);
        }
    }
}

/*
 * Perform naive correlation for row/col sets, pick the best match.
 */
static uint8_t decode_symbol(int16_t *samples)
{
    /* We want to see which rowRef and colRef yields the largest magnitude of dot product. */
    double bestRowVal = -1e30;
    double bestColVal = -1e30;
    int    bestRowIdx = 0;
    int    bestColIdx = 0;

    /* Convert the incoming samples to double for correlation */
    for (int rowIdx = 0; rowIdx < 16; rowIdx++) {
        double sum = 0.0;
        for (int n = 0; n < SAMPLES_PER_SYMBOL; n++) {
            /* scale to [-1..1], ignoring amplitude doubling from 2 tones */
            double sampleVal = (double)samples[n] / 32768.0;
            sum += sampleVal * rowRef[rowIdx][n];
        }
        if (sum > bestRowVal) {
            bestRowVal = sum;
            bestRowIdx = rowIdx;
        }
    }

    for (int colIdx = 0; colIdx < 16; colIdx++) {
        double sum = 0.0;
        for (int n = 0; n < SAMPLES_PER_SYMBOL; n++) {
            double sampleVal = (double)samples[n] / 32768.0;
            sum += sampleVal * colRef[colIdx][n];
        }
        if (sum > bestColVal) {
            bestColVal = sum;
            bestColIdx = colIdx;
        }
    }

    /* Recombine to get the byte value = (rowIdx << 4) + colIdx */
    return (uint8_t)((bestRowIdx << 4) | bestColIdx);
}

/*
 * dtmf_decode(): read wave data in SAMPLES_PER_SYMBOL chunks, do correlation,
 *                output raw bytes. If real-time, flush after each.
 */
int dtmf_decode(FILE *fin, FILE *fout, bool verbose, bool stream)
{
    long dataSize = robust_read_wav_header(fin, verbose);
    if (dataSize < 0) {
        return 1;
    }
    if (dataSize == 0) {
        if (verbose) {
            fprintf(stderr, "No wave data to decode.\n");
        }
        return 0;
    }

    /* Make sure dataSize is multiple of SAMPLES_PER_SYMBOL * 2 bytes. */
    long bytesPerSymbol = (SAMPLES_PER_SYMBOL * sizeof(int16_t));
    long symbolCount = dataSize / bytesPerSymbol;
    if ((dataSize % bytesPerSymbol) != 0) {
        fprintf(stderr, "Warning: dataSize not multiple of %ld; ignoring leftover.\n",
                bytesPerSymbol);
        symbolCount = dataSize / bytesPerSymbol;  /* truncate */
    }
    if (verbose) {
        fprintf(stderr, "Decoding %ld symbols from wave...\n", symbolCount);
    }

    /* Buffer to hold each symbol's PCM samples. */
    int16_t *symbolBuf = (int16_t *)malloc(bytesPerSymbol);
    if (!symbolBuf) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    for (long i = 0; i < symbolCount; i++) {
        if (fread(symbolBuf, 1, bytesPerSymbol, fin) != (size_t)bytesPerSymbol) {
            fprintf(stderr, "Short read on wave data.\n");
            free(symbolBuf);
            return 1;
        }
        uint8_t decodedByte = decode_symbol(symbolBuf);
        fwrite(&decodedByte, 1, 1, fout);
        if (stream) {
            fflush(fout);
        }
    }

    free(symbolBuf);
    if (verbose) {
        fprintf(stderr, "Decoded %ld symbols -> %ld bytes.\n", symbolCount, symbolCount);
    }
    return 0;
}

/* ------------------ CLI / Main ------------------ */

static void print_usage(const char *progName)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "  -e             Encode input (raw bytes) into extended DTMF wave\n"
        "  -d             Decode extended DTMF wave back to raw bytes\n"
        "  -i <file>      Input file (defaults to stdin)\n"
        "  -o <file>      Output file (defaults to stdout)\n"
        "  -v             Verbose\n"
        "  -r             Real-time decode (flush each byte)\n"
        "  -h             Help\n",
        progName
    );
}

/*
 * Initialize the rowFreq[] and colFreq[] arrays for all 16 possible indexes.
 * Indices [0..3] = standard DTMF, indices [4..15] = extended sets.
 */
static void init_frequencies(void)
{
    /* First 4 row/col frequencies from standard DTMF */
    for (int i = 0; i < 4; i++) {
        rowFreq[i] = standardRow[i];
        colFreq[i] = standardCol[i];
    }
    /* Next 12 from extended sets */
    for (int i = 4; i < 16; i++) {
        rowFreq[i] = extendedRow[i - 4];
        colFreq[i] = extendedCol[i - 4];
    }
}

int main(int argc, char *argv[])
{
    bool encode = false;
    bool decode = false;
    bool verbose = false;
    bool realtime = false;

    char *inputFile = NULL;
    char *outputFile = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "edi:o:vrh")) != -1) {
        switch (opt) {
            case 'e': encode = true; break;
            case 'd': decode = true; break;
            case 'i': inputFile = optarg; break;
            case 'o': outputFile = optarg; break;
            case 'v': verbose = true; break;
            case 'r': realtime = true; break;
            case 'h':
            default:
                print_usage(argv[0]);
                return (opt == 'h') ? 0 : 1;
        }
    }

    /* Must specify exactly one of -e or -d */
    if ((encode && decode) || (!encode && !decode)) {
        fprintf(stderr, "Error: Must specify either -e or -d (but not both).\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Open files */
    FILE *fin = stdin;
    FILE *fout = stdout;

    if (inputFile) {
        fin = fopen(inputFile, encode ? "rb" : "rb"); /* same for encode/decode */
        if (!fin) {
            perror("fopen inputFile");
            return 1;
        }
    }
    if (outputFile) {
        fout = fopen(outputFile, encode ? "wb" : "wb"); /* same for encode/decode */
        if (!fout) {
            perror("fopen outputFile");
            if (fin != stdin) fclose(fin);
            return 1;
        }
    }

    /* Build frequency tables */
    init_frequencies();

    /* Build correlation references (used only for decoding, but quick to do anyway) */
    build_references();

    /* Dispatch */
    int ret = 0;
    if (encode) {
        ret = dtmf_encode(fin, fout, verbose);
    } else {
        ret = dtmf_decode(fin, fout, verbose, realtime);
    }

    if (fin && fin != stdin) fclose(fin);
    if (fout && fout != stdout) fclose(fout);

    return ret;
}
