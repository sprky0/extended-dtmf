# build_binary.py

import PyInstaller.__main__
import sys
import os

def build_binary():
    """Build a standalone binary with PyInstaller."""
    
    # Determine platform-specific settings
    is_windows = sys.platform.startswith('win')
    is_mac = sys.platform.startswith('darwin')
    
    # Base configuration
    config = [
        'src/extended_dtmf/cli.py',  # Our entry point
        '--name=dtmf',
        '--onefile',  # Create a single executable
        '--clean',    # Clean PyInstaller cache
        '--noconfirm',  # Replace output directory without asking
    ]
    
    # Platform specific options
    if is_windows:
        config.extend([
            '--noconsole',  # Don't show console window on Windows
            '--icon=resources/dtmf.ico'  # Windows icon if we have one
        ])
    elif is_mac:
        config.extend([
            '--add-binary=/System/Library/Frameworks/Tk.framework/Tk:tk',
            '--add-binary=/System/Library/Frameworks/Tcl.framework/Tcl:tcl',
        ])
    
    # Add data files (if needed)
    # config.extend(['--add-data=src/extended_dtmf/data:data'])
    
    # Hidden imports for scipy and numpy
    config.extend([
        '--hidden-import=numpy',
        '--hidden-import=scipy',
        '--hidden-import=scipy.signal',
        '--hidden-import=scipy.fft',
        '--hidden-import=scipy.io.wavfile',
    ])
    
    # Run PyInstaller
    PyInstaller.__main__.run(config)

if __name__ == '__main__':
    build_binary()