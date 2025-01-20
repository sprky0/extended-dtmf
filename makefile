# Simple Makefile for building the extended DTMF CLI tool.

# Compiler and flags
CC      := gcc
CFLAGS  := -Wall -Wextra -O2
LDFLAGS := -lm  # link math library for sin(), etc.

# Directories
SRCDIR  := src
OBJDIR  := obj
BINDIR  := bin

# Target program name
TARGET  := extended_dtmf

# Automatically gather all .c files in SRCDIR
SOURCES := $(wildcard $(SRCDIR)/*.c)
# Convert each .c to a corresponding .o under OBJDIR
OBJECTS := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

# Default goal
all: $(BINDIR)/$(TARGET)

# Link step
$(BINDIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Linking complete: $@"

# Compile step
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled: $< -> $@"

# Clean up intermediate and final build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)
	@echo "Cleaned."

.PHONY: all clean
