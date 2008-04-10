CC = gcc
CFLAGS = -Wall -ggdb -I../../nasm-0.98.36

OBJECTS = data.o disassemble.o image.o import.o label.o main.o ref.o reloc.o
NASM_PATH = ../../nasm-0.98.36
NASM_OBJECTS = \
	$(NASM_PATH)/insnsd.o \
	$(NASM_PATH)/nasmlib.o \
	$(NASM_PATH)/disasm.o \
	$(NASM_PATH)/sync.o \

HEADERS = decompile.h

BINNAME = decompile

all: $(OBJECTS) $(NASM_OBJECTS) $(HEADERS)
	$(CC) $(CFLAGS) -o $(BINNAME) $(OBJECTS) $(NASM_OBJECTS) $(LDFLAGS)

$(OBJECTS): $(HEADERS)

clean:
	rm -f $(OBJECTS) $(BINNAME)
