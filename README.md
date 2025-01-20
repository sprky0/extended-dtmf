# DTMF Encoder/Decoder

A command-line tool for encoding binary data into DTMF (Dual-Tone Multi-Frequency) audio signals and decoding it back. This tool can be used to convert any binary data into audio tones that can be transmitted over audio channels and later reconstructed.

## Installation

Compile the program using the provided Makefile:

```bash
make
```

This will create the executable in the current directory.

## Usage

The basic syntax is:

```bash
./dtmf [options] <command>
```

### Commands

- `encode`: Convert binary data into DTMF audio
- `decode`: Convert DTMF audio back to binary data
- `freqs`: Display the frequency tables used for encoding/decoding and exit

### Options

- `-i FILE`: Specify input file (default: standard input)
- `-o FILE`: Specify output file (default: standard output)
- `-v`: Enable verbose output (shows progress and debugging information)
- `-s`: Enable streaming mode (decode only - processes input in real-time)

### Examples

Encode a binary file to WAV:
```bash
./dtmf -i input.bin -o output.wav encode
```

Decode an audio file back to binary:
```bash
./dtmf -i input.wav -o output.bin decode
```

Use with pipes:
```bash
cat input.bin | ./dtmf encode > output.wav
cat input.wav | ./dtmf decode > output.bin
```

Display frequency tables:
```bash
./dtmf freqs
```

Enable verbose output during encoding:
```bash
./dtmf -v -i input.bin -o output.wav encode
```

Use streaming mode for decoding:
```bash
./dtmf -s -i input.wav -o output.bin decode
```

## Input/Output Formats

- For encoding: The input should be any binary data
- For decoding: The input should be WAV or raw audio data containing DTMF tones
- Output format matches the operation (WAV for encode, binary for decode)

## Error Handling

The program will exit with status code 0 on success, or 1 if an error occurs. Error messages are printed to standard error. Common errors include:

- Invalid command or option
- Unable to open input/output files
- DTMF system initialization failure
- Encoding/decoding errors

## Notes

- When using standard output for binary data, the program automatically handles platform-specific binary mode settings
- The streaming mode (-s) option is only valid for decode operations
- Verbose output (-v) is written to standard error to avoid interfering with binary data

## Platform Support

- Linux/Unix systems
- Windows (with automatic binary mode handling)
- Other POSIX-compliant systems