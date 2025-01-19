#include "dtmf.h"
#include <stdio.h>
#include <stdlib.h>  // Add this line for free()
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s encode <text> <output_file>\n", argv[0]);
        printf("       %s decode <input_file>\n", argv[0]);
        return 1;
    }

    DTMFEncoder* encoder = dtmf_encoder_create();
    if (!encoder) {
        printf("Failed to create encoder\n");
        return 1;
    }

    if (strcmp(argv[1], "encode") == 0) {
        if (dtmf_encode_to_file(encoder, argv[2], argv[3]) != 0) {
            printf("Failed to encode\n");
            dtmf_encoder_destroy(encoder);
            return 1;
        }
        printf("Encoded successfully\n");
    } else if (strcmp(argv[1], "decode") == 0) {
        char* decoded = dtmf_decode_from_file(encoder, argv[2]);
        if (decoded) {
            printf("Decoded: %s\n", decoded);
            free(decoded);
        } else {
            printf("Failed to decode\n");
        }
    }

    dtmf_encoder_destroy(encoder);
    return 0;
}