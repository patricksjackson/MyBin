CC=gcc
CFLAGS=-Wall -Wextra -O3
INCLUDE_DIR=-I./inc \
			-I./../Primes/inc
AR=ar
LIBS=
SOURCES=./src/main.c
ASOBJECTS=$(ASSOURCES:.s=.o)
OBJECTS=$(SOURCES:.c=.o)
TARGET=libPrimes.a

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $(TARGET) $(OBJECTS)

$(OBJECTS): $(SOURCES)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE_DIR) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET)
