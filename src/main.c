/*
 * main.c
 * 
 * Command-line interface for extended DTMF encoding/decoding.
 */
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extended_dtmf.h"

static void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s [options] <command> <input> [output]\n", progname);
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  encode    Convert bytes to DTMF audio\n");
    fprintf(stderr, "  decode    Convert DTMF audio back to bytes\n");
    fprintf(stderr, "  freqs     Display frequency tables and exit\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -v        Enable verbose output\n");
    fprintf(stderr, "  -s        Enable streaming mode (decode only)\n");
    fprintf(stderr, "\nIf output file is omitted, writes to standard output\n");
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool stream = false;
    int argIdx = 1;
    FILE *fout = stdout;  // Default to stdout
    
    /* Parse options */
    while (argIdx < argc && argv[argIdx][0] == '-') {
        if (!strcmp(argv[argIdx], "-v")) {
            verbose = true;
        }
        else if (!strcmp(argv[argIdx], "-s")) {
            stream = true;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[argIdx]);
            print_usage(argv[0]);
            return 1;
        }
        argIdx++;
    }
    
    /* Need at least command and input file */
    if (argIdx >= argc - 1) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *command = argv[argIdx++];
    const char *infile = argv[argIdx++];
    
    /* Initialize DTMF system */
    if (dtmf_init() != 0) {
        fprintf(stderr, "Failed to initialize DTMF system\n");
        return 1;
    }
    
    /* Handle frequency dump command */
    if (!strcmp(command, "freqs")) {
        dtmf_dump_frequencies(stderr);  // Print to stderr since stdout might be binary
        return 0;
    }
    
    FILE *fin = fopen(infile, "rb");
    if (!fin) {
        fprintf(stderr, "Cannot open input file: %s\n", infile);
        return 1;
    }

    /* If output file specified, open it */
    if (argIdx < argc) {
        const char *outfile = argv[argIdx];
        fout = fopen(outfile, "wb");
        if (!fout) {
            fprintf(stderr, "Cannot open output file: %s\n", outfile);
            fclose(fin);
            return 1;
        }
    } else {
        /* If using stdout for binary data, set binary mode if platform requires it */
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
    }
    
    int result;
    if (!strcmp(command, "encode")) {
        result = dtmf_encode(fin, fout, verbose);
    }
    else if (!strcmp(command, "decode")) {
        result = dtmf_decode(fin, fout, verbose, stream);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        print_usage(argv[0]);
        result = 1;
    }
    
    fclose(fin);
    if (fout != stdout) {
        fclose(fout);
    }
    return result;
}