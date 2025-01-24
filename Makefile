# Makefile for multi-threaded downloader

CC = gcc                  # Compiler
CFLAGS = -Wall -Wextra     # Compiler flags
LDFLAGS = -lcurl -lpthread # Linker flags for curl and pthread
SRC = multithreadedDownloader.c  # Source file
OBJ = multithreadedDownloader.o  # Object file
TARGET = multithreadedDownloader  # Output executable

# Default target to build the program
all: $(TARGET)

# Link the object file to create the executable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

# Compile the source code into an object file
$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

# Clean up object files and the executable
clean:
	rm -f $(OBJ) $(TARGET)

# Rebuild everything from scratch
rebuild: clean all
