#include "opsoup.h"

#include <elf.h>

int elf_make_segment_table(image_t *image) {
    segment_t *segment = NULL;
    int nsegs = 0, cur = 0;
    segment_type_t type;
    Elf32_Ehdr *eh;
    Elf32_Shdr *sh;
    char *strings;
    int i;

    eh = (Elf32_Ehdr *) image->core;

    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        fprintf(stderr, "elf: not an ELF image\n");
        return -1;
    }

    if (eh->e_shnum == 0 || eh->e_shstrndx == /* SHN_XINDEX */ 0xffff) {
        fprintf(stderr, "elf: no support for ELF images with more than 65535 sections\n");
        return -1;
    }

    if (eh->e_ident[EI_CLASS] != ELFCLASS32 ||
        eh->e_ident[EI_VERSION] != EV_CURRENT ||
        eh->e_ident[EI_DATA] != ELFDATA2LSB ||
        eh->e_machine != EM_386) {
        fprintf(stderr, "elf: no support for this ELF class (we handle 32-bit LSB, version 1, for i386)\n");
        return -1;
    }

    if (eh->e_type != ET_REL) {
        fprintf(stderr, "elf: no support for ELF types other than 'relocatable'\n");
        return -1;
    }

    sh = (Elf32_Shdr *) (image->core + eh->e_shoff + eh->e_shstrndx * eh->e_shentsize);
    strings = (char *) (image->core + sh->sh_offset);

    for (i = 0; i < eh->e_shnum; i++) {
        sh = (Elf32_Shdr *) (image->core + eh->e_shoff + i * eh->e_shentsize);

        type = seg_NONE;

        switch (sh->sh_type) {
            case SHT_PROGBITS:
                if (!(sh->sh_flags & SHF_ALLOC))
                    break;

                if (sh->sh_flags & SHF_EXECINSTR)
                    type = seg_CODE;
                else
                    type = seg_DATA;
                break;

            case SHT_NOBITS:
                type = seg_BSS;
                break;

            case SHT_REL:
                type = seg_RELOC;
                break;

            default:
                break;
        }

        if (cur == nsegs) {
            nsegs += 8;
            segment = realloc(segment, sizeof(segment_t) * nsegs);
        }

        segment[cur].name = strings + sh->sh_name;
        segment[cur].type = type;
        segment[cur].start = sh->sh_offset;
        segment[cur].size = sh->sh_size;
        segment[cur].end = segment[cur].start + segment[cur].size;
        segment[cur].info = sh;

        printf("elf: segment '%s' is type seg_%s, start 0x%x, size 0x%x\n", segment[cur].name, type == seg_CODE ? "CODE" : type == seg_DATA ? "DATA" : type == seg_BSS ? "BSS" : type == seg_RELOC ? "RELOC" : "NONE", segment[cur].start, segment[cur].size);

        cur++;
    }

    if (cur == nsegs) {
        nsegs += 1;
        segment = realloc(segment, sizeof(segment_t) * nsegs);
    }

    segment[cur].name = NULL;
    segment[cur].type = seg_NONE;

    image->segment = segment;

    return 0;
}

int elf_relocate(image_t *image) {
    int i, j;
    segment_t *reloc_segment, *target_segment;
    Elf32_Shdr *sh;
    int nrel;
    Elf32_Rel *rel;
    uint32_t *mem;
    intptr_t val;

    for (i = 0; image->segment[i].name != NULL; i++) {
        if (image->segment[i].type != seg_RELOC)
            continue;

        reloc_segment = &image->segment[i];
        sh = reloc_segment->info;

        rel = (Elf32_Rel *) (image->core + sh->sh_offset);
        nrel = sh->sh_size / sh->sh_entsize;

        target_segment = &image->segment[sh->sh_info];
        sh = target_segment->info;

        printf("elf: applying %d relocations from reloc segment '%s' to target segment '%s'\n", nrel, reloc_segment->name, target_segment->name);

        for (j = 0; j < nrel; j++) {
            mem = (uint32_t *) (image->core + rel->r_offset);
            val = (intptr_t) (image->core + sh->sh_offset);

            switch (ELF32_R_TYPE(rel->r_info)) {
                case R_386_32:
                    *mem += val;
                    break;

                case R_386_PC32:
                    *mem += val - (intptr_t) mem;
                    break;

                default:
                    fprintf(stderr, "elf: unknown relocation type %d\n", ELF32_R_TYPE(rel->r_info));
                    return -1;
            }
        }
    }

    return 0;
}
