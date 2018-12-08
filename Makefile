CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c99 -pedantic -fopenmp
LDFLAGS=-fopenmp
LIBS=-lpng16 -lz -ljpeg

TARGET=gazou_mawari_tubusu
OBJS=gazou_mawari_tubusu.o image_io.o

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS)
