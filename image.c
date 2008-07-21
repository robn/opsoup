#include "opsoup.h"

#define IMAGE_FILE "ffe.o"

int image_load(void) {
    struct stat st;

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

    o->image.core = (uint8_t *) mmap(NULL, o->image.size, PROT_READ | PROT_WRITE, MAP_PRIVATE, o->image.fd, 0);
    if (o->image.core == MAP_FAILED) {
        fprintf(stderr, "load: couldn't map '" IMAGE_FILE "': %s\n", strerror(errno));
        return 1;
    }

    if (elf_make_segment_table(&o->image) < 0) {
        fprintf(stderr, "load: no segments found\n");
        return 1;
    }

    if (elf_relocate(o) < 0) {
        fprintf(stderr, "load: relocation failed\n");
        return 1;
    }

    /* !!! load segment table */

#if 0
    o->image.size = 0;
    for(i = 0; segment[i].name != NULL; i++)
        if(segment[i].start + segment[i].size > o->image.size)
            o->image.size = segment[i].start + segment[i].size;

    o->image.segment = (segment_t *) malloc(sizeof(segment_t) * i);

    for (i = 0; segment[i].name != NULL; i++)
        o->image.segment[i] = segment[i];

    o->image.core = (uint8_t *) malloc(o->image.size);

    printf("image: allocated %d bytes\n", o->image.size);

    for(i = 0; o->image.segment[i].name != NULL; i++) {
        printf("image: loading segment '%s' to 0x%x (size 0x%x)\n", o->image.segment[i].name, o->image.segment[i].start, o->image.segment[i].size);

        if(fseek(f, o->image.segment[i].foff, SEEK_SET) < 0) {
            fprintf(stderr, "  seek error: %s\n", strerror(errno));
            return 1;
        }

        if(fread(o->image.core + o->image.segment[i].start, 1, o->image.segment[i].size, f) != o->image.segment[i].size) {
            if(ferror(f))
                fprintf(stderr, "  read error: %s\n", strerror(errno));
            else
                fprintf(stderr, "  short read\n");
            return 1;
        }

        /* calculate end offset */
        o->image.segment[i].cend = o->image.segment[i].start + o->image.segment[i].size;
    }
#endif

    return 0;
}

segment_t *image_seg_find(uint8_t *mem) {
    int i;

    for(i = 0; o->image.segment[i].name != NULL; i++)
        if(mem >= o->image.segment[i].start && mem < o->image.segment[i].start + o->image.segment[i].size)
            return &o->image.segment[i];

    return NULL;
}
