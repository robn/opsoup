#include "decompile.h"

#define IMAGE_FILE          "gamegfx.exe"

segment_t segment[] = {
    { ".text",      seg_CODE,   0x001000,   0x016a2b,   0x000400 },
    { ".bss",       seg_BSS,    0x018000,   0x0040b4,   0        },
    { ".rdata",     seg_DATA,   0x01d000,   0x0000ca,   0x017000 },
    { ".data",      seg_DATA,   0x01e000,   0x004080,   0x017200 },
    { ".idata",     seg_IMPORT, 0x023000,   0x000ee8,   0x01b400 },
    { "DATASEG",    seg_DATA,   0x024000,   0x113e13,   0x01c400 },
    { "CODESEG",    seg_CODE,   0x138000,   0x05f45d,   0x130400 },
    { ".reloc",     seg_RELOC,  0x19b000,   0x00c1c6,   0x192000 },
    { NULL,         seg_NONE,   0,          0,          0        }
};

u8 *core;

int image_load(void) {
    FILE *f;
    int i, size;

    f = fopen(IMAGE_FILE, "rb");
    if(f == NULL) {
        fprintf(stderr, "load: couldn't open '" IMAGE_FILE "' for reading: %s\n", strerror(errno));
        return 1;
    }

    /* !!! load segment table */

    size = 0;
    for(i = 0; segment[i].name != NULL; i++)
        if(segment[i].coff + segment[i].size > size)
            size = segment[i].coff + segment[i].size;

    core = (u8 *) malloc(size);

    printf("image: allocated %d bytes\n", size);

    for(i = 0; segment[i].name != NULL; i++) {
        printf("image: loading segment '%s' to 0x%x (size 0x%x)\n", segment[i].name, segment[i].coff, segment[i].size);

        if(fseek(f, segment[i].foff, SEEK_SET) < 0) {
            fprintf(stderr, "  seek error: %s\n", strerror(errno));
            return 1;
        }

        if(fread(core + segment[i].coff, 1, segment[i].size, f) != segment[i].size) {
            if(ferror(f))
                fprintf(stderr, "  read error: %s\n", strerror(errno));
            else
                fprintf(stderr, "  short read\n");
            return 1;
        }

        /* calculate end offset */
        segment[i].cend = segment[i].coff + segment[i].size;
    }

    return 0;
}

segment_t *image_seg_find(u32 off) {
    int i;

    for(i = 0; segment[i].name != NULL; i++)
        if(off >= segment[i].coff && off < segment[i].coff + segment[i].size)
            return &segment[i];

    return NULL;
}
