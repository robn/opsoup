#include "opsoup.h"

#define IMAGE_FILE          "ffe.o"

/*
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
*/

int image_load(void) {
    struct stat st;
    int i;

    if (stat(IMAGE_FILE, &st) < 0) {
        fprintf(stderr, "load: couldn't stat '" IMAGE_FILE "': %s\n", strerror(errno));
        return 1;
    }

    o->image.size = st.st_size;

    o->image.fd = open(IMAGE_FILE, 0, O_RDONLY);
    if (o->image.fd < 0) {
        fprintf(stderr, "load: couldn't open '" IMAGE_FILE "' for reading: %s\n", strerror(errno));
        return 1;
    }

    o->image.core = (uint8_t *) mmap(NULL, o->image.size, PROT_READ, MAP_SHARED, o->image.fd, 0);
    if (o->image.core == MAP_FAILED) {
        fprintf(stderr, "load: couldn't map '" IMAGE_FILE "': %s\n", strerror(errno));
        return 1;
    }

    /* !!! load segment table */

#if 0
    o->image.size = 0;
    for(i = 0; segment[i].name != NULL; i++)
        if(segment[i].coff + segment[i].size > o->image.size)
            o->image.size = segment[i].coff + segment[i].size;

    o->image.segment = (segment_t *) malloc(sizeof(segment_t) * i);

    for (i = 0; segment[i].name != NULL; i++)
        o->image.segment[i] = segment[i];

    o->image.core = (uint8_t *) malloc(o->image.size);

    printf("image: allocated %d bytes\n", o->image.size);

    for(i = 0; o->image.segment[i].name != NULL; i++) {
        printf("image: loading segment '%s' to 0x%x (size 0x%x)\n", o->image.segment[i].name, o->image.segment[i].coff, o->image.segment[i].size);

        if(fseek(f, o->image.segment[i].foff, SEEK_SET) < 0) {
            fprintf(stderr, "  seek error: %s\n", strerror(errno));
            return 1;
        }

        if(fread(o->image.core + o->image.segment[i].coff, 1, o->image.segment[i].size, f) != o->image.segment[i].size) {
            if(ferror(f))
                fprintf(stderr, "  read error: %s\n", strerror(errno));
            else
                fprintf(stderr, "  short read\n");
            return 1;
        }

        /* calculate end offset */
        o->image.segment[i].cend = o->image.segment[i].coff + o->image.segment[i].size;
    }
#endif

    return 0;
}

segment_t *image_seg_find(uint32_t off) {
    int i;

    for(i = 0; o->image.segment[i].name != NULL; i++)
        if(off >= o->image.segment[i].coff && off < o->image.segment[i].coff + o->image.segment[i].size)
            return &o->image.segment[i];

    return NULL;
}
