import numpy as np
from scipy import signal
from scipy.io import wavfile
import matplotlib.pyplot as plt

class ExtendedDTMF:
    def __init__(self):
        # Define frequency tables
        self.low_freqs = np.array([624 + i * 73 for i in range(16)])
        self.high_freqs = np.array([1209 + i * 127 for i in range(16)])
        
        # Audio parameters
        self.sample_rate = 44100    # Hz
        self.tone_duration = 0.05   # 50ms tone duration (standard DTMF)
        self.pause_duration = 0.05  # 50ms interdigit pause
        self.amplitude = 0.3        # prevent clipping
        self.fade_ms = 5           # 5ms fade in/out to prevent clicking
        
        # FFT parameters
        self.fft_size = 4096
        self.freq_tolerance = 5.0  # Hz tolerance for frequency detection
        
    def encode_char(self, char_code):
        """Convert a character code to its frequency pair."""
        if not 0 <= char_code <= 255:
            raise ValueError("Character code must be between 0 and 255")
        
        row = char_code // 16
        col = char_code % 16
        return self.low_freqs[row], self.high_freqs[col]
    
    def decode_freqs(self, low_freq, high_freq):
        """Convert a frequency pair back to character code."""
        row = np.argmin(np.abs(self.low_freqs - low_freq))
        col = np.argmin(np.abs(self.high_freqs - high_freq))
        return row * 16 + col
    
    def generate_tone(self, frequencies):
        """Generate a tone with given frequencies including fade in/out and pause."""
        # Calculate samples for tone
        tone_samples = int(self.sample_rate * self.tone_duration)
        pause_samples = int(self.sample_rate * self.pause_duration)
        fade_samples = int(self.sample_rate * (self.fade_ms / 1000))
        
        # Generate time array for the tone portion
        t = np.linspace(0, self.tone_duration, tone_samples)
        
        # Generate the basic signal
        signal = np.sum([self.amplitude * np.sin(2 * np.pi * f * t) for f in frequencies], axis=0)
        
        # Create fade in/out envelope
        fade_in = np.linspace(0, 1, fade_samples)
        fade_out = np.linspace(1, 0, fade_samples)
        fade_middle = np.ones(tone_samples - 2 * fade_samples)
        envelope = np.concatenate([fade_in, fade_middle, fade_out])
        
        # Apply envelope
        signal = signal * envelope
        
        # Add pause (silence)
        silence = np.zeros(pause_samples)
        
        # Combine signal and pause
        return np.concatenate([signal, silence])
    
    def detect_frequencies(self, audio_chunk):
        """Detect the strongest frequencies in an audio chunk using FFT."""
        window = signal.windows.hann(len(audio_chunk))
        windowed_chunk = audio_chunk * window
        
        # Compute FFT
        fft = np.fft.rfft(windowed_chunk)
        freqs = np.fft.rfftfreq(len(audio_chunk), 1/self.sample_rate)
        magnitudes = np.abs(fft)
        
        # Find peaks
        peaks = signal.find_peaks(magnitudes, height=max(magnitudes)/10)[0]
        peak_freqs = freqs[peaks]
        peak_mags = magnitudes[peaks]
        
        # Sort peaks by magnitude and get the two strongest
        sorted_indices = np.argsort(peak_mags)[-2:]
        detected_freqs = np.sort(peak_freqs[sorted_indices])
        
        # Classify as low and high frequency
        low_freq = detected_freqs[0]
        high_freq = detected_freqs[1]
        
        return low_freq, high_freq
    
    def encode_string(self, text):
        """Encode a string into audio signal."""
        encoded = np.array([])
        for char in text:
            freqs = self.encode_char(ord(char))
            encoded = np.concatenate([encoded, self.generate_tone(freqs)])
        return encoded
    
    def decode_signal(self, signal):
        """Decode an audio signal back into text."""
        decoded_text = ""
        samples_per_char = int(self.sample_rate * (self.tone_duration + self.pause_duration))
        tone_samples = int(self.sample_rate * self.tone_duration)
        
        for i in range(0, len(signal), samples_per_char):
            # Only analyze the tone portion, ignore the pause
            chunk = signal[i:i + tone_samples]
            if len(chunk) < tone_samples/2:  # Skip incomplete chunks
                break
                
            low_freq, high_freq = self.detect_frequencies(chunk)
            char_code = self.decode_freqs(low_freq, high_freq)
            decoded_text += chr(char_code)
            
        return decoded_text

# Example usage
if __name__ == "__main__":
    dtmf = ExtendedDTMF()
    
    # Encode a test message
    test_message = "Hello, World!"
    print(f"Original message: {test_message}")
    
    # Generate audio signal
    encoded_signal = dtmf.encode_string(test_message)
    
    # Save to WAV file (optional)
    wavfile.write("test_message.wav", dtmf.sample_rate, encoded_signal)
    
    # Decode the signal
    decoded_message = dtmf.decode_signal(encoded_signal)
    print(f"Decoded message: {decoded_message}")
    
    # Plot a spectrogram of the signal (optional)
    plt.figure(figsize=(15, 5))
    plt.specgram(encoded_signal, Fs=dtmf.sample_rate, NFFT=2048)
    plt.ylabel('Frequency (Hz)')
    plt.xlabel('Time (s)')
    plt.title('Spectrogram of encoded message')
    plt.colorbar(label='Intensity')
    plt.show()