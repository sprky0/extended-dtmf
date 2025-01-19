#ifndef WAV_H
#define WAV_H

#include <stdint.h>
#include <stdio.h>

// WAV header structure
typedef struct {
    char chunk_id[4];       // "RIFF"
    uint32_t chunk_size;
    char format[4];         // "WAVE"
    char subchunk1_id[4];   // "fmt "
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char subchunk2_id[4];   // "data"
    uint32_t subchunk2_size;
} WavHeader;

// WAV file handling functions
void wav_write_header(FILE* fp, uint32_t data_size, int sample_rate);
int wav_read_header(FILE* fp, WavHeader* header);
int wav_find_data_chunk(FILE* fp);
void wav_dump_header(const WavHeader* header);

#endif // WAV_H