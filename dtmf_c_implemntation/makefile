CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -Isrc
LDFLAGS = -lm

SRC_DIR = src
BUILD_DIR = build

SRCS = $(SRC_DIR)/dtmf.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET = dtmf

.PHONY: all clean install uninstall dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/dtmf.h
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)