import pytest
import numpy as np
from extended_dtmf import ExtendedDTMF

class TestExtendedDTMF:
    @pytest.fixture
    def dtmf(self):
        return ExtendedDTMF()
    
    def test_encode_decode_ascii(self, dtmf):
        """Test encoding and decoding of ASCII characters."""
        test_message = "Hello, World!"
        encoded = dtmf.encode_string(test_message)
        decoded = dtmf.decode_signal(encoded)
        assert decoded == test_message
    
    def test_encode_decode_extended(self, dtmf):
        """Test encoding and decoding of extended ASCII characters."""
        test_message = "áéíóú"  # Extended ASCII characters
        encoded = dtmf.encode_string(test_message)
        decoded = dtmf.decode_signal(encoded)
        assert decoded == test_message
    
    def test_frequency_detection(self, dtmf):
        """Test accurate frequency detection."""
        test_char = 'A'  # ASCII 65
        low_freq, high_freq = dtmf.encode_char(ord(test_char))
        signal = dtmf.generate_tone([low_freq, high_freq])
        detected_low, detected_high = dtmf.detect_frequencies(signal)
        
        assert abs(detected_low - low_freq) < dtmf.freq_tolerance
        assert abs(detected_high - high_freq) < dtmf.freq_tolerance
    
    def test_invalid_character(self, dtmf):
        """Test handling of invalid character codes."""
        with pytest.raises(ValueError):
            dtmf.encode_char(256)
    
    def test_noise_tolerance(self, dtmf):
        """Test decoding with added noise."""
        test_message = "Test123"
        encoded = dtmf.encode_string(test_message)
        
        # Add some noise
        noise = np.random.normal(0, 0.01, len(encoded))
        noisy_signal = encoded + noise
        
        decoded = dtmf.decode_signal(noisy_signal)
        assert decoded == test_message
