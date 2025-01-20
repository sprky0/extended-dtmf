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
    fprintf(stderr, "Usage: %s [options] <command>\n", progname);
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  encode    Convert bytes to DTMF audio\n");
    fprintf(stderr, "  decode    Convert DTMF audio back to bytes\n");
    fprintf(stderr, "  freqs     Display frequency tables and exit\n");
    fprintf(stderr, "\nOptions (must appear before command):\n");
    fprintf(stderr, "  -i FILE   Input file (default: stdin)\n");
    fprintf(stderr, "  -o FILE   Output file (default: stdout)\n");
    fprintf(stderr, "  -t TEXT   Input text string (alternative to -i)\n");
    fprintf(stderr, "  -v        Enable verbose output\n");
    fprintf(stderr, "  -s        Enable streaming mode (decode only)\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  %s -t \"Hello\" -o message.wav encode\n", progname);
    fprintf(stderr, "  %s -i input.wav -o output.bin decode\n", progname);
    fprintf(stderr, "  %s freqs\n", progname);
}

/* Structure to hold all command line options */
typedef struct {
    bool verbose;
    bool stream;
    const char *infile;
    const char *outfile;
    const char *text_input;
    const char *command;
} Options;

/* Parse command line arguments */
static int parse_options(int argc, char *argv[], Options *opts) {
    int i = 1;
    
    /* Initialize options */
    memset(opts, 0, sizeof(Options));
    
    /* Parse options first */
    while (i < argc && argv[i][0] == '-') {
        if (!strcmp(argv[i], "-v")) {
            opts->verbose = true;
        }
        else if (!strcmp(argv[i], "-s")) {
            opts->stream = true;
        }
        else if (!strcmp(argv[i], "-i")) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for -i option\n");
                return -1;
            }
            if (opts->text_input) {
                fprintf(stderr, "Error: Cannot use both -i and -t options\n");
                return -1;
            }
            opts->infile = argv[i];
        }
        else if (!strcmp(argv[i], "-o")) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for -o option\n");
                return -1;
            }
            opts->outfile = argv[i];
        }
        else if (!strcmp(argv[i], "-t")) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for -t option\n");
                return -1;
            }
            if (opts->infile) {
                fprintf(stderr, "Error: Cannot use both -i and -t options\n");
                return -1;
            }
            opts->text_input = argv[i];
        }
        else {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            return -1;
        }
        i++;
    }
    
    /* Command must be the next argument */
    if (i >= argc) {
        fprintf(stderr, "Error: No command specified\n");
        return -1;
    }
    
    opts->command = argv[i++];
    
    /* No more arguments should remain */
    if (i < argc) {
        fprintf(stderr, "Error: Unexpected argument after command: %s\n", argv[i]);
        return -1;
    }
    
    /* Validate command */
    if (strcmp(opts->command, "encode") && 
        strcmp(opts->command, "decode") && 
        strcmp(opts->command, "freqs")) {
        fprintf(stderr, "Error: Unknown command: %s\n", opts->command);
        return -1;
    }
    
    /* Validate options */
    if (opts->stream && strcmp(opts->command, "decode") == 0) {
        fprintf(stderr, "Error: Streaming mode (-s) is only valid for decode command\n");
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    Options opts;
    FILE *fin = stdin;
    FILE *fout = stdout;
    int result;
    
    /* Show usage if no arguments */
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Parse command line */
    if (parse_options(argc, argv, &opts) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Initialize DTMF system */
    if (dtmf_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize DTMF system\n");
        return 1;
    }
    
    /* Handle frequency dump command */
    if (!strcmp(opts.command, "freqs")) {
        dtmf_dump_frequencies(stderr);
        return 0;
    }
    
    /* Handle input source */
    if (opts.text_input) {
        fin = fmemopen((void*)opts.text_input, strlen(opts.text_input), "rb");
        if (!fin) {
            fprintf(stderr, "Error: Failed to create memory buffer for text input\n");
            return 1;
        }
    } 
    else if (opts.infile) {
        fin = fopen(opts.infile, "rb");
        if (!fin) {
            fprintf(stderr, "Error: Cannot open input file: %s\n", opts.infile);
            return 1;
        }
    }
    
    /* Handle output destination */
    if (opts.outfile) {
        fout = fopen(opts.outfile, "wb");
        if (!fout) {
            fprintf(stderr, "Error: Cannot open output file: %s\n", opts.outfile);
            if (fin != stdin) {
                fclose(fin);
            }
            return 1;
        }
    } else {
        /* If using stdout for binary data, set binary mode if platform requires it */
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
    }
    
    /* Process the command */
    if (!strcmp(opts.command, "encode")) {
        result = dtmf_encode(fin, fout, opts.verbose);
    }
    else {  /* decode */
        result = dtmf_decode(fin, fout, opts.verbose, opts.stream);
    }
    
    /* Cleanup */
    if (fin != stdin) {
        fclose(fin);
    }
    if (fout != stdout) {
        fclose(fout);
    }
    
    return result;
}