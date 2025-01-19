#include "wav.h"
#include <string.h>

void wav_write_header(FILE* fp, uint32_t data_size, int sample_rate) {
    WavHeader header = {
        .chunk_id = {'R', 'I', 'F', 'F'},
        .chunk_size = data_size + 36,  // data size + header size
        .format = {'W', 'A', 'V', 'E'},
        .subchunk1_id = {'f', 'm', 't', ' '},
        .subchunk1_size = 16,
        .audio_format = 1, // PCM
        .num_channels = 1, // Mono
        .sample_rate = sample_rate,
        .byte_rate = sample_rate * 2, // sample_rate * block_align
        .block_align = 2, // channels * bits_per_sample / 8
        .bits_per_sample = 16,
        .subchunk2_id = {'d', 'a', 't', 'a'},
        .subchunk2_size = data_size
    };
    
    // Ensure header is written completely
    size_t written = fwrite(&header, sizeof(header), 1, fp);
    if (written != 1) {
        fprintf(stderr, "Failed to write WAV header\n");
    }
}

int wav_read_header(FILE* fp, WavHeader* header) {
    if (fread(header, sizeof(WavHeader), 1, fp) != 1) {
        return -1;
    }
    
    // Validate WAV format
    if (memcmp(header->chunk_id, "RIFF", 4) != 0 ||
        memcmp(header->format, "WAVE", 4) != 0 ||
        memcmp(header->subchunk1_id, "fmt ", 4) != 0) {
        return -1;
    }
    
    return 0;
}

int wav_find_data_chunk(FILE* fp) {
    char chunk_id[4];
    uint32_t chunk_size;
    long start_pos = ftell(fp);
    
    while (1) {
        // Debug print
        printf("Current position: %ld\n", ftell(fp));
        
        if (fread(chunk_id, 1, 4, fp) != 4) {
            printf("Failed to read chunk ID\n");
            fseek(fp, start_pos, SEEK_SET);
            return -1;
        }
        
        // Print chunk ID for debugging
        printf("Chunk ID: %c%c%c%c\n", chunk_id[0], chunk_id[1], chunk_id[2], chunk_id[3]);
        
        if (fread(&chunk_size, 4, 1, fp) != 1) {
            printf("Failed to read chunk size\n");
            fseek(fp, start_pos, SEEK_SET);
            return -1;
        }
        
        printf("Chunk size: %u\n", chunk_size);
        
        if (memcmp(chunk_id, "data", 4) == 0) {
            return 0;
        }
        
        // Seek to next chunk
        if (fseek(fp, chunk_size, SEEK_CUR) != 0) {
            printf("Failed to seek to next chunk\n");
            fseek(fp, start_pos, SEEK_SET);
            return -1;
        }
    }
}

void wav_dump_header(const WavHeader* header) {
    printf("WAV Header Info:\n");
    printf("Sample Rate: %u\n", header->sample_rate);
    printf("Bits/Sample: %u\n", header->bits_per_sample);
    printf("Channels: %u\n", header->num_channels);
    printf("Audio Format: %u\n", header->audio_format);
    printf("Subchunk1 Size: %u\n", header->subchunk1_size);
    printf("Data Size: %u\n", header->subchunk2_size);
}