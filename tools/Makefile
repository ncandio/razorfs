# Makefile for RazorFS Tools
# Builds the filesystem checker and other utilities

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -g
INCLUDES = -I../src
LIBS = -lm -lpthread

# Source files
RAZORFSCK_SOURCES = razorfsck_main.c razorfsck.c razorfsck_repair.c ../src/razor_core.c
RAZORFSCK_OBJECTS = $(RAZORFSCK_SOURCES:.c=.o)

# Targets
TARGETS = razorfsck

# Default target
all: $(TARGETS)

# Build razorfsck
razorfsck: $(RAZORFSCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(RAZORFSCK_OBJECTS) $(LIBS)

# Generic rule for object files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build files
clean:
	rm -f $(RAZORFSCK_OBJECTS) $(TARGETS)
	rm -f *.o ../src/*.o

# Install tools (requires sudo)
install: $(TARGETS)
	@echo "Installing RazorFS tools..."
	sudo cp razorfsck /usr/local/bin/
	sudo chmod 755 /usr/local/bin/razorfsck
	@echo "Installation complete"

# Uninstall tools
uninstall:
	@echo "Uninstalling RazorFS tools..."
	sudo rm -f /usr/local/bin/razorfsck
	@echo "Uninstall complete"

# Test target
test: razorfsck
	@echo "Testing razorfsck tool..."
	./razorfsck --help
	./razorfsck --version
	@echo "Basic tests passed"

# Debug build
debug: CFLAGS += -DDEBUG -g3 -O0
debug: clean $(TARGETS)

# Dependencies (generated with gcc -MM)
razorfsck_main.o: razorfsck_main.c razorfsck.h ../src/razor_core.h
razorfsck.o: razorfsck.c razorfsck.h ../src/razor_core.h
razorfsck_repair.o: razorfsck_repair.c razorfsck.h ../src/razor_core.h
../src/razor_core.o: ../src/razor_core.c ../src/razor_core.h

.PHONY: all clean install uninstall test debug