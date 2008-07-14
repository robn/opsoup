NASM_PATH = /home/rob/code/asm/nasm-2.03.01

CC = gcc
CFLAGS = -Wall -ggdb -I$(NASM_PATH)

OBJECTS = data.o disassemble.o image.o import.o label.o main.o ref.o reloc.o
NASM_OBJECTS = \
	$(NASM_PATH)/insnsb.o \
	$(NASM_PATH)/insnsn.o \
	$(NASM_PATH)/insnsd.o \
	$(NASM_PATH)/nasmlib.o \
	$(NASM_PATH)/disasm.o \
	$(NASM_PATH)/regdis.o \
	$(NASM_PATH)/regs.o \
	$(NASM_PATH)/sync.o \

HEADERS = opsoup.h

BINNAME = opsoup

all: $(OBJECTS) $(NASM_OBJECTS) $(HEADERS)
	$(CC) $(CFLAGS) -o $(BINNAME) $(OBJECTS) $(NASM_OBJECTS) $(LDFLAGS)

$(OBJECTS): $(HEADERS)

clean:
	rm -f $(OBJECTS) $(BINNAME)
