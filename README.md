# Extended DTMF

A Python library implementing an extended version of DTMF (Dual-Tone Multi-Frequency) signaling that supports the full LATIN-1 character set.


## Features

- Encodes/decodes all 256 LATIN-1 characters using dual-tone signals
- Robust frequency detection using FFT
- Noise-tolerant design
- Visualization capabilities for debugging

## Installation

```bash
pip install extended-dtmf
```

## Usage

```python
from extended_dtmf import ExtendedDTMF

# Create encoder/decoder
dtmf = ExtendedDTMF()

# Encode a message
encoded_signal = dtmf.encode_string("Hello, World!")

# Save to WAV file
from scipy.io import wavfile
wavfile.write("message.wav", dtmf.sample_rate, encoded_signal)

# Decode a signal
decoded_message = dtmf.decode_signal(encoded_signal)
print(decoded_message)  # "Hello, World!"
```

## Testing

```bash
pip install .[test]
pytest
```

## Concept

Given the existing set of dual tone encoded numerals we are familar with from touch tone telephones,
I wanted to try extending that encoding out fuurther to encompass a larger character set.  This implementation
presently can handle the ISO/IEC 8859-1 aka LATIN-1 character set.

Let's rock.  Here is the curreently supported parirings and their byte.  Row and Column zero contain the associated frequency,
and their intersection contains the encoded character byte.


| Hz | 1209 | 1336 | 1463 | 1590 | 1717 | 1844 | 1971 | 2098 | 2225 | 2352 | 2479 | 2606 | 2733 | 2860 | 2987 | 3114 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 624 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
| 697 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 |
| 770 | 32 | 33 | 34 | 35 | 36 | 37 | 38 | 39 | 40 | 41 | 42 | 43 | 44 | 45 | 46 | 47 |
| 843 | 48 | 49 | 50 | 51 | 52 | 53 | 54 | 55 | 56 | 57 | 58 | 59 | 60 | 61 | 62 | 63 |
| 916 | 64 | 65 | 66 | 67 | 68 | 69 | 70 | 71 | 72 | 73 | 74 | 75 | 76 | 77 | 78 | 79 |
| 989 | 80 | 81 | 82 | 83 | 84 | 85 | 86 | 87 | 88 | 89 | 90 | 91 | 92 | 93 | 94 | 95 |
| 1062 | 96 | 97 | 98 | 99 | 100 | 101 | 102 | 103 | 104 | 105 | 106 | 107 | 108 | 109 | 110 | 111 |
| 1135 | 112 | 113 | 114 | 115 | 116 | 117 | 118 | 119 | 120 | 121 | 122 | 123 | 124 | 125 | 126 | 127 |
| 1208 | 128 | 129 | 130 | 131 | 132 | 133 | 134 | 135 | 136 | 137 | 138 | 139 | 140 | 141 | 142 | 143 |
| 1281 | 144 | 145 | 146 | 147 | 148 | 149 | 150 | 151 | 152 | 153 | 154 | 155 | 156 | 157 | 158 | 159 |
| 1354 | 160 | 161 | 162 | 163 | 164 | 165 | 166 | 167 | 168 | 169 | 170 | 171 | 172 | 173 | 174 | 175 |
| 1427 | 176 | 177 | 178 | 179 | 180 | 181 | 182 | 183 | 184 | 185 | 186 | 187 | 188 | 189 | 190 | 191 |
| 1500 | 192 | 193 | 194 | 195 | 196 | 197 | 198 | 199 | 200 | 201 | 202 | 203 | 204 | 205 | 206 | 207 |
| 1573 | 208 | 209 | 210 | 211 | 212 | 213 | 214 | 215 | 216 | 217 | 218 | 219 | 220 | 221 | 222 | 223 |
| 1646 | 224 | 225 | 226 | 227 | 228 | 229 | 230 | 231 | 232 | 233 | 234 | 235 | 236 | 237 | 238 | 239 |
| 1719 | 240 | 241 | 242 | 243 | 244 | 245 | 246 | 247 | 248 | 249 | 250 | 251 | 252 | 253 | 254 | 255 |


## License

MIT License

