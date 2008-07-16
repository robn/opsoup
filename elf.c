#include "opsoup.h"

#include <linux/elf.h>

segment_t *elf_get_segments(image_t *image) {
    Elf32_Ehdr *eh;
    Elf32_Shdr *sh;
    char *strings;
    int i;
    segment_type_t type;

    eh = (Elf32_Ehdr *) image->core;

    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        fprintf(stderr, "elf: not an ELF image\n");
        return NULL;
    }

    if (eh->e_shnum == 0 || eh->e_shstrndx == /* SHN_XINDEX */ 0xffff) {
        fprintf(stderr, "elf: no support for ELF images with more than 65535 sections\n");
        return NULL;
    }

    if (eh->e_ident[EI_CLASS] != ELFCLASS32 ||
        eh->e_ident[EI_VERSION] != EV_CURRENT ||
        eh->e_ident[EI_DATA] != ELFDATA2LSB ||
        eh->e_machine != EM_386) {
        fprintf(stderr, "elf: no support for this ELF class (we handle 32-bit LSB, version 1, for i386)\n");
        return NULL;
    }

    if (eh->e_type != ET_REL) {
        fprintf(stderr, "elf: no support for ELF types other than 'relocatable'\n");
        return NULL;
    }

    sh = (Elf32_Shdr *) (image->core + eh->e_shoff + eh->e_shstrndx * eh->e_shentsize);
    strings = (char *) (image->core + sh->sh_offset);

    printf("elf: %d sections\n", eh->e_shnum);

    for (i = 0; i < eh->e_shnum; i++) {
        sh = (Elf32_Shdr *) (image->core + eh->e_shoff + i * eh->e_shentsize);

        type = seg_NONE;

        switch (sh->sh_type) {
            case SHT_PROGBITS:
                if (!(sh->sh_flags & SHF_ALLOC))
                    continue;

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
                continue;
        }

        printf("elf: segment '%s' is type seg_%s\n", strings + sh->sh_name, type == seg_CODE ? "CODE" : type == seg_DATA ? "DATA" : type == seg_BSS ? "BSS" : "RELOC");
    }

    return NULL;
}
