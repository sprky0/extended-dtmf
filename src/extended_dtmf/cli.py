# src/extended_dtmf/cli.py

import sys
import argparse
from pathlib import Path
from scipy.io import wavfile
import matplotlib.pyplot as plt
from .codec import ExtendedDTMF

def encode_command(args):
    """Handle the encode command"""
    dtmf = ExtendedDTMF()
    encoded = dtmf.encode_string(args.text)
    wavfile.write(args.output, dtmf.sample_rate, encoded)
    print(f"Message encoded and saved to {args.output}")

def decode_command(args):
    """Handle the decode command"""
    dtmf = ExtendedDTMF()
    sample_rate, signal = wavfile.read(args.input)
    decoded = dtmf.decode_signal(signal)
    print(f"Decoded message: {decoded}")

def analyze_command(args):
    """Handle the analyze command"""
    dtmf = ExtendedDTMF()
    sample_rate, signal = wavfile.read(args.input)
    
    plt.figure(figsize=(15, 5))
    plt.specgram(signal, Fs=sample_rate, NFFT=2048)
    plt.ylabel('Frequency (Hz)')
    plt.xlabel('Time (s)')
    plt.title('Spectrogram Analysis')
    plt.colorbar(label='Intensity')
    
    # Add horizontal lines for frequency bands
    for freq in dtmf.low_freqs:
        plt.axhline(y=freq, color='r', alpha=0.3, linestyle='--')
    for freq in dtmf.high_freqs:
        plt.axhline(y=freq, color='b', alpha=0.3, linestyle='--')
    
    output_path = args.input.parent / f"{args.input.stem}_analysis.png"
    plt.savefig(output_path)
    plt.close()
    
    print(f"Analysis saved to {output_path}")

def main():
    """Main entry point for the CLI"""
    parser = argparse.ArgumentParser(
        description="Extended DTMF encoder/decoder CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  dtmf encode "Hello, World!" output.wav
  dtmf decode input.wav
  dtmf analyze input.wav
        """
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Commands')
    
    # Encode command
    encode_parser = subparsers.add_parser('encode', help='Encode text to WAV file')
    encode_parser.add_argument('text', help='Text to encode')
    encode_parser.add_argument('output', type=Path, help='Output WAV file')
    
    # Decode command
    decode_parser = subparsers.add_parser('decode', help='Decode WAV file to text')
    decode_parser.add_argument('input', type=Path, help='Input WAV file')
    
    # Analyze command
    analyze_parser = subparsers.add_parser('analyze', help='Analyze WAV file and create spectrogram')
    analyze_parser.add_argument('input', type=Path, help='Input WAV file')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        sys.exit(1)
    
    try:
        if args.command == 'encode':
            encode_command(args)
        elif args.command == 'decode':
            decode_command(args)
        elif args.command == 'analyze':
            analyze_command(args)
    except Exception as e:
        print(f"Error: {str(e)}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()