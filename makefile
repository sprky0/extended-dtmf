# Makefile for extended DTMF encoder/decoder

# Compiler and flags
CC      := gcc
CFLAGS  := -Wall -Wextra -O2
LDFLAGS := -lm

# Directories
SRCDIR  := src
OBJDIR  := obj
BINDIR  := bin

# Source files
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET  := $(BINDIR)/dtmf

# Create directories if they don't exist
$(shell mkdir -p $(OBJDIR) $(BINDIR))

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/extended_dtmf.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean