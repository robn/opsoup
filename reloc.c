#include "opsoup.h"

#define IMAGE_BASE          (0x400000)

reloc_t *reloc = NULL;
int nreloc = 0, sreloc = 0;

void reloc_apply(void) {
    int i;
    u32 pos, next, rva, rsize;
    u16 off;
    segment_t *s;

    /* process each relocation segment (can there be more than one)? */
    for(i = 0; segment[i].name != NULL; i++) {
        if(segment[i].type != seg_RELOC) continue;

        printf("reloc: applying segment '%s'\n", segment[i].name);

        pos = segment[i].coff;
        while(pos < segment[i].cend) {
            rva = * (u32 *) (core + pos); pos += 4;
            rsize = * (u32 *) (core + pos); pos += 4;

            if(rsize == 0)
                break;

            next = pos + (rsize - 8);

            /* have to check each reloc, we only want HIGHLOW relocs */
            while(pos < next) {
                off = * (u16 *) (core + pos); pos += 2;

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
                * (u32 *) (core + reloc[nreloc].off) -= IMAGE_BASE;
                reloc[nreloc].target = (* (u32 *) (core + reloc[nreloc].off));

                /* !!!
                if(verbose)
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
