/*
 * extended_dtmf.h
 *
 * Header file for extended DTMF encoding/decoding functionality.
 * Supports standard DTMF frequencies plus extended set for full byte range.
 */

#ifndef EXTENDED_DTMF_H
#define EXTENDED_DTMF_H

#include <stdbool.h>
#include <stdint.h>

/* Public configuration constants that might need adjustment */
#define DTMF_SAMPLE_RATE     16000   /* Sample rate in Hz */
#define DTMF_SYMBOL_DURATION 0.100   /* Duration per symbol when encoding (seconds) */
#define DTMF_GAP_DURATION    0.015   /* Silence gap between symbols (seconds) */
#define DTMF_WINDOW_MS       20      /* Analysis window size for decoding (milliseconds) */
#define DTMF_MIN_STABLE      2       /* Minimum stable windows before symbol is valid */

/* Public interface */

/*
 * Initialize the DTMF system. Must be called before any encode/decode operations.
 * Returns 0 on success, non-zero on failure.
 */
int dtmf_init(void);

/*
 * Encode raw bytes from input file to DTMF audio wave file.
 * Parameters:
 *   fin     - Input file containing raw bytes
 *   fout    - Output file for wave data
 *   verbose - Enable verbose logging
 * Returns 0 on success, non-zero on error
 */
int dtmf_encode(FILE *fin, FILE *fout, bool verbose);

/*
 * Decode DTMF audio from wave file back to raw bytes.
 * Parameters:
 *   fin     - Input wave file
 *   fout    - Output file for decoded bytes
 *   verbose - Enable verbose logging
 *   stream  - Enable real-time streaming mode (flush after each byte)
 * Returns 0 on success, non-zero on error
 */
int dtmf_decode(FILE *fin, FILE *fout, bool verbose, bool stream);

/* For testing/debugging */
void dtmf_dump_frequencies(FILE *out);

#endif /* EXTENDED_DTMF_H */