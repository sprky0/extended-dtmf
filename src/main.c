/*
 * main.c
 * 
 * Command-line interface for extended DTMF encoding/decoding.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extended_dtmf.h"

static void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s [options] <command> <input> <output>\n", progname);
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  encode    Convert bytes to DTMF audio\n");
    fprintf(stderr, "  decode    Convert DTMF audio back to bytes\n");
    fprintf(stderr, "  freqs     Display frequency tables and exit\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -v        Enable verbose output\n");
    fprintf(stderr, "  -s        Enable streaming mode (decode only)\n");
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool stream = false;
    int argIdx = 1;
    
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
    
    /* Need at least command */
    if (argIdx >= argc) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *command = argv[argIdx++];
    
    /* Initialize DTMF system */
    if (dtmf_init() != 0) {
        fprintf(stderr, "Failed to initialize DTMF system\n");
        return 1;
    }
    
    /* Handle frequency dump command */
    if (!strcmp(command, "freqs")) {
        dtmf_dump_frequencies(stdout);
        return 0;
    }
    
    /* All other commands need input/output files */
    if (argIdx + 2 > argc) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *infile = argv[argIdx++];
    const char *outfile = argv[argIdx++];
    
    FILE *fin = fopen(infile, "rb");
    if (!fin) {
        fprintf(stderr, "Cannot open input file: %s\n", infile);
        return 1;
    }
    
    FILE *fout = fopen(outfile, "wb");
    if (!fout) {
        fprintf(stderr, "Cannot open output file: %s\n", outfile);
        fclose(fin);
        return 1;
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
    fclose(fout);
    return result;
}