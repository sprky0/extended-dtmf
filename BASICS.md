Getting Started with Extended DTMF
---------------------------------

First, make sure you have Python 3.8 or newer installed. Then, create a virtual environment to keep things clean:

```bash
python -m venv venv
source venv/bin/activate  # On Windows, use: venv\Scripts\activate
```

Install the package in development mode with test dependencies:
```bash
pip install -e ".[test]"
```

Running the Tests:
To run the test suite, simply use:
```bash
pytest
```
This will run all the tests and show you the test coverage report.

Using the Command Line Tool:
After installation, you can use the 'dtmf' command directly. Here are some examples:
```bash
# Convert some text to DTMF tones
dtmf encode "Hello World" hello.wav

# Listen to your WAV file using any media player
# Then try decoding it back
dtmf decode hello.wav

# Create a visual analysis of your encoded message
dtmf analyze hello.wav
```

The analysis command will create a spectrogram PNG file in the same directory as your WAV file, which lets you see the frequency patterns visually.

Troubleshooting:
- If the 'dtmf' command isn't found, make sure your virtual environment is activated
- For help with any command, add --help (e.g., `dtmf encode --help`)
- If you get import errors, make sure you installed with the `-e ".[test]"` flag

That's it! You're ready to start encoding messages with DTMF tones.





Some reference re DTMF as applies to the original 
https://www.montana.edu/rmaher/eele477_sp18/EELE_477_Lab_09.pdf