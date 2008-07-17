#include "opsoup.h"

#define IMAGE_BASE          (0x400000)

reloc_t *reloc = NULL;
int nreloc = 0, sreloc = 0;

void reloc_apply(void) {
    int i;
    uint32_t pos, next, rva, rsize;
    uint16_t off;
    segment_t *s;

    /* process each relocation segment (can there be more than one)? */
    for(i = 0; o->image.segment[i].name != NULL; i++) {
        if(o->image.segment[i].type != seg_RELOC) continue;

        printf("reloc: applying segment '%s'\n", o->image.segment[i].name);

        pos = o->image.segment[i].start;
        while(pos < o->image.segment[i].end) {
            rva = * (uint32_t *) (o->image.core + pos); pos += 4;
            rsize = * (uint32_t *) (o->image.core + pos); pos += 4;

            if(rsize == 0)
                break;

            next = pos + (rsize - 8);

            /* have to check each reloc, we only want HIGHLOW relocs */
            while(pos < next) {
                off = * (uint16_t *) (o->image.core + pos); pos += 2;

                /* HIGHLOW relocs have top 4 bits == 3 */
                if((off >> 12) != 3)
                    continue;

                /* make room if necessary */
                if(nreloc == sreloc) {
                    sreloc += 1024;
                    reloc = (reloc_t *) realloc(reloc, sizeof(reloc_t) * sreloc);
                }

                /* apply the reloc */
                reloc[nreloc].off = rva + (off & 0xfff);
                * (uint32_t *) (o->image.core + reloc[nreloc].off) -= IMAGE_BASE;
                reloc[nreloc].target = (* (uint32_t *) (o->image.core + reloc[nreloc].off));

                /* !!!
                if(o->verbose)
                    printf("  reloc at 0x%x: 0x%x -> 0x%x\n", reloc[nreloc].off, reloc[nreloc].target + IMAGE_BASE, reloc[nreloc].target);
                */

                /* add a label */
                s = image_seg_find(reloc[nreloc].target);
                if(s != NULL)
                    label_insert(reloc[nreloc].target, label_RELOC, s);

                nreloc++;
            }
        }
    }

    printf("reloc: applied %d relocs\n", nreloc);
    
    label_print_count("reloc");
}
