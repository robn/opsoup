#include "opsoup.h"

#define IMAGE_ENTRY         (0x11783)

/* see if this r/m operand has a 32-bit displacement */
int _rm_disp32(uint32_t off, uint8_t *reg) {
    uint8_t mod, rm;

    /* mod is the top two bits of the first byte */
    mod = o->image.core[off] >> 6;

    /* if mod is 3, then its a direct register access, so bail */
    if(mod == 3)
        return 0;

    /* rm is the bottom three bits */
    rm = o->image.core[off] & 0x7;

    /* reg is the middle three bits */
    if(reg != NULL)
        *reg = (o->image.core[off] & 0x38) >> 3;

    /* 4-byte displacement */
    if(mod == 2 || (mod == 0 && rm == 5) || (mod == 0 && rm == 4 && (o->image.core[off + 1] & 0x7) == 5))
        return 1;

    return 0;
}

/* see if the current instruction has a memory access through a vector table */
static label_type_t _vtable_access(uint32_t off) {
    uint8_t b, reg;

    b = o->image.core[off];

    /* skip segment override prefixes */
    if(b == 0x26 || b == 0x2e || b == 0x36 || b == 0x3e || b == 0x64 || b == 0x65) {
        off++;
        b = o->image.core[off];
    }

    /*
     *  jmp r/m32 == 0xff /4
     * call r/m32 == 0xff /2
     */
    if(b == 0xff && _rm_disp32(off + 1, &reg)) {
        if(reg == 4)
            return label_CODE_JUMP;
        if(reg == 2)
            return label_CODE_CALL;
    }

    /*
     * mov reg32,r/m32 == 0x8b /r
     */
    if(b == 0x8b && _rm_disp32(off + 1, NULL))
        return label_DATA;

    /*
     * movzx reg32,r/m8 == 0xf 0xb6 /r
     */
    if(b == 0xf && o->image.core[off + 1] == 0xb6 && _rm_disp32(off + 2, NULL))
        return label_DATA;

    /*
     * mov reg8,r/m8 == 0x8a /r
     * test r/m32,reg32 == 0x85 /r
     */
    if((b == 0x8a || b == 0x85) && _rm_disp32(off + 1, NULL))
        return label_DATA;

    return 0;
}

/* see if the current instruction has a memory access */
static int _mem_access(uint32_t off) {
    uint8_t b, reg;

    b = o->image.core[off];

    /* skip segment override prefixes */
    if(b == 0x26 || b == 0x2e || b == 0x36 || b == 0x3e || b == 0x64 || b == 0x65) {
        off++;
        b = o->image.core[off];
    }

    /*
     * mov r/m32,mem32 == 0xc7 /0
     */
    if(b == 0xc7) {
        _rm_disp32(off + 1, &reg);
        if(reg == 0)
            return 1;
    }

    /*
     * cmp r/m32,mem32 == 0x81 /0
     */
    if(b == 0x81) {
        _rm_disp32(off + 1, &reg);
        if(reg == 7)
            return 1;
    }

    /*
     * push imm32       == 0x68
     *  mov reg32,imm32 == 0xb8+r
     */
    if(b == 0x68 || (b >= 0xb8 && b <= 0xbf))
        return 1;

    return 0;
}

static void _target_extract(uint32_t off, uint32_t *target, label_type_t *type) {
    uint8_t b1, b2;

    b1 = o->image.core[off];

    /* skip segment override and offset/address prefixes */
    if(b1 == 0x26 || b1 == 0x2e || b1 == 0x36 || b1 == 0x3e || b1 == 0x64 || b1 == 0x65 || b1 == 0x66 || b1 == 0x67) {
        off++;
        b1 = o->image.core[off];
    }

    b2 = o->image.core[off + 1];

    /* grab bag of 8-bit relative instructions
     *   e0-e2: loop, loope, loopne
     *      e3: jexz, jecxz
     */
    if(b1 >= 0xe0 && b1 <= 0xe3) {
        *type = label_CODE_JUMP;
        *target = off + 2 + * (int8_t *) (o->image.core + off + 1);
    }

    /* immediate-mode (offset-based) call */
    else if(b1 == 0xe8) {
        *type = label_CODE_CALL;
        *target = off + 5 + * (int32_t *) (o->image.core + off + 1);
    }

    /* short (8-bit) jump */
    else if(b1 == 0xeb) {
        *type = label_CODE_JUMP;
        *target = off + 2 + * (int8_t *) (o->image.core + off + 1);
    }

    /* long (32-bit) jump */
    else if(b1 == 0xe9) {
        *type = label_CODE_JUMP;
        *target = off + 5 + * (int32_t *) (o->image.core + off + 1);
    }

    /* local (8-bit) conditional jump */
    else if(b1 >= 0x70 && b1 <= 0x7f) {
        *type = label_CODE_JUMP;
        *target = off + 2 + * (int8_t *) (o->image.core + off + 1);
    }

    /* near (32-bit) conditional jump */
    else if(b1 == 0x0f && b2 >= 0x80 && b2 <= 0x8f) {
        *type = label_CODE_JUMP;
        *target = off + 6 + * (int32_t *) (o->image.core + off + 2);
    }
}

void dis_pass1(void) {
    int i, len, ir = 0;
    uint32_t off, target, vtable = 0, voff;
    label_type_t type, vtype;
    char line[256];
    segment_t *s;

    printf("dis1: disassembly, pass 1 - finding obvious labels\n");

    /* force the entry point */
    label_insert(IMAGE_ENTRY, label_CODE_ENTRY, image_seg_find(IMAGE_ENTRY));

    /* code segments */
    for(i = 0; o->image.segment[i].name != NULL; i++) {
        if(o->image.segment[i].type != seg_CODE) continue;

        printf("dis1: processing segment '%s' (size 0x%x)\n", o->image.segment[i].name, o->image.segment[i].size);

        /* find the first relocation in this segment */
        for(; ir < nreloc; ir++)
            if(reloc[ir].off >= o->image.segment[i].coff)
                break;

        /* loop the entire segment */
        off = o->image.segment[i].coff;
        while(off < o->image.segment[i].coff + o->image.segment[i].size) {
            /* get length of this instruction */
            len = disasm(o->image.core + off, line, sizeof(line), 32, off, 1, 0);
            if(len == 0)
                len = eatbyte(o->image.core + off, line, sizeof(line));

            type = label_NONE;

            /* this instruction in where the current relocation got applied */
            if(ir < nreloc && off + len > reloc[ir].off && reloc[ir].off >= o->image.segment[i].coff && reloc[ir].off < o->image.segment[i].cend) {
                target = reloc[ir].target;
                ir++;

                /* identifying what type of data the target points to */
                s = image_seg_find(target);
                if(s == NULL) {
                    if(o->verbose)
                        printf("  target 0x%x (reloc at 0x%x) is not in a segment!\n", target, reloc[ir].off);
                    continue;
                }

                /* if it points to a data or bss segment, its data */
                if(s->type == seg_BSS)
                    type = label_BSS;
                else if(s->type == seg_DATA)
                    type = label_DATA;

                /* r/m32 instructions are pointing at a vector table */
                if((vtype = _vtable_access(off))) {
                    vtable = target;
                    if(type & label_BSS)
                        type = label_BSS_VTABLE;
                    else
                        type = label_DATA_VTABLE;
                }

                /* some normal memory access */
                else if(_mem_access(off)) {
                    if(type & label_BSS)
                        type = label_BSS;
                    else
                        type = label_DATA;
                }

                /* nfi, call it data */
                else
                    type = label_DATA;

                /*
                else {
                    printf("\n           reloc: 0x%x\n", reloc[ir].off);
                    printf("          offset: 0x%x\n", off);
                    printf("          target: 0x%x (%s)\n", target, s->name);
                    printf("     instruction: %s\n", line);
                    printf("           bytes:");

                    for(i = -5; i <= len + 5; i++) {
                        if(i == 0)
                            printf("\033[1;33m");
                        printf(" 0x%x", o->image.core[off + i]);
                        if(i == len)
                            printf("\033[m");
                    }

                    printf("\n\n");

                    abort();
                }
                */
            }

            /* non-relocated instruction */
            else
                _target_extract(off, &target, &type);

            /* add the label (as long as its in a segment) */
            s = image_seg_find(target);
            if(s == NULL) {
                if(o->verbose)
                    printf("  target 0x%x (offset 0x%x) is not in a segment!\n", target, off);
            }

            else {
                /* override instruction if it points to a data or bss segment */
                /*
                if(s->type == seg_BSS)
                    type = label_BSS;
                else if(s->type == seg_DATA)
                    type = label_DATA;
                */
                
                while(type != label_NONE) {

                    /* add it */
                    label_insert(target, type, s);

                    ref_insert(off, target);

                    /* done making label */
                    type = label_NONE;

                    /* possible second relocation on this instruction */
                    if(ir < nreloc && off + len > reloc[ir].off && reloc[ir].off >= o->image.segment[i].coff && reloc[ir].off < o->image.segment[i].cend) {
                        target = reloc[ir].target;
                        ir++;

                        /* its a straight data access */
                        s = image_seg_find(target);
                            
                        if(s != NULL) {
                            if(s->type == seg_BSS)
                                type = label_BSS;
                            else
                                type = label_DATA;
                        }

                        else if(o->verbose)
                            printf("  target 0x%x (reloc at 0x%x) is not in a segment!\n", target, reloc[ir].off);
                    }
                }
            }

            /* found a vector table, process that too */
            if(vtable != 0) {
                if(o->verbose)
                    printf("  vector table found at 0x%x\n", vtable);

                voff = reloc[ir].off - 4;

                /* keep going as long as relocations happen every dword */
                while(reloc[ir].off == voff + 4) {
                    s = image_seg_find(reloc[ir].target);

                    if(s == NULL || s->type == seg_BSS)
                        type = label_BSS;

                    else if(vtype & label_CODE && s->type == seg_DATA)
                        type = label_DATA;

                    else
                        type = vtype;

                    /* add the label */
                    label_insert(reloc[ir].target, type, s);

                    ref_insert(reloc[ir].off, reloc[ir].target);

                    /* next reloc, next vector */
                    ir++;
                    voff += 4;
                }

                ir--;
                vtable = 0;
            }

            /* next instruction */
            off += len;
            
            /* progress report */
            if(o->verbose)
                if(((off - o->image.segment[i].coff) & 0xfffff000) != ((off - o->image.segment[i].coff + len) & 0xfffff000))
                    printf("  processed 0x%x bytes\n", off + len - o->image.segment[i].coff);
        }
    }

    label_print_count("dis1");
}

int dis_pass2(int n) {
    label_t *l;
    int nl, i, ir = 0, len;
    uint32_t off, target, vtable = 0, voff;
    char line[256];
    label_type_t type, vtype;
    segment_t *s;

    printf("dis2: disassembly, pass 2, round %d - finding missed labels\n", n);

    n = 0;

    /* copy the label array, so we can add to it */
    nl = nlabel;
    l = (label_t *) malloc(sizeof(label_t) * nl);
    memcpy(l, label, sizeof(label_t) * nl);

    /* loop the code labels */
    for(i = 0; i < nl; i++) {
        if(!(l[i].type & label_CODE)) continue;

        off = l[i].target;

        /* skip relocs forward to this label */
        for(; ir < nreloc; ir++)
            if(reloc[ir].off >= off)
                break;

        /* disassemble from this label to the next */
        while(1) {
            len = disasm(o->image.core + off, line, sizeof(line), 32, off, 1, 0);
            if(len == 0)
                len = eatbyte(o->image.core + off, line, sizeof(line));

            /* bail if we've gone past the next label */
            if(off + len > l[i].seg->cend || (i < nl - 1 && off + len > l[i + 1].target))
                break;

            type = label_NONE;

            /* this instruction in where the current relocation got applied */
            if(ir < nreloc && off + len > reloc[ir].off && reloc[ir].off >= l[i].seg->coff && reloc[ir].off < l[i].seg->cend) {
                target = reloc[ir].target;
                ir++;

                /* identifying what type of data the target points to */
                s = image_seg_find(target);
                if(s == NULL) {
                    if(o->verbose)
                        printf("  target 0x%x (reloc at 0x%x) is not in a segment!\n", target, reloc[ir].off);
                    continue;
                }

                /* if it points to a data or bss segment, its data */
                if(s->type == seg_BSS)
                    type = label_BSS;
                else if(s->type == seg_DATA)
                    type = label_DATA;

                /* r/m32 instructions are pointing at a vector table */
                if((vtype = _vtable_access(off))) {
                    vtable = target;
                    if(type & label_BSS)
                        type = label_BSS_VTABLE;
                    else
                        type = label_DATA_VTABLE;
                }

                /* some normal memory access */
                else if(_mem_access(off)) {
                    if(type & label_BSS)
                        type = label_BSS_VTABLE;
                    else
                        type = label_DATA_VTABLE;
                }

                /* nfi, call it data */
                else
                    type = label_DATA;

                /*
                else {
                    printf("\n           reloc: 0x%x\n", reloc[ir].off);
                    printf("          offset: 0x%x\n", off);
                    printf("          target: 0x%x (%s)\n", target, s->name);
                    printf("     instruction: %s\n", line);
                    printf("           bytes:");

                    for(i = -5; i <= len + 5; i++) {
                        if(i == 0)
                            printf("\033[1;33m");
                        printf(" 0x%x", o->image.core[off + i]);
                        if(i == len)
                            printf("\033[m");
                    }

                    printf("\n\n");

                    abort();
                }
                */
            }

            /* non-relocated instruction */
            else
                _target_extract(off, &target, &type);

            /* add the label (as long as its in a segment) */
            s = image_seg_find(target);
            if(s == NULL) {
                if(o->verbose)
                    printf("  target 0x%x (offset 0x%x) is not in a segment!\n", target, off);
            }

            else {
                /* override instruction if it points to a data or bss segment */
                /*
                if(s->type == seg_BSS)
                    type = label_BSS;
                else if(s->type == seg_DATA)
                    type = label_DATA;
                */
                
                while(type != label_NONE) {

                    /* add it */
                    label_insert(target, type, s);

                    /* and a reference to it from this instruction */
                    ref_insert(off, target);

                    /* done making label */
                    type = label_NONE;

                    /* possible second relocation on this instruction */
                    if(ir < nreloc && off + len > reloc[ir].off && reloc[ir].off >= l[i].seg->coff && reloc[ir].off < l[i].seg->cend) {
                        target = reloc[ir].target;
                        ir++;

                        /* its a straight data access */
                        s = image_seg_find(target);
                            
                        if(s != NULL) {
                            if(s->type == seg_BSS)
                                type = label_BSS;
                            else
                                type = label_DATA;
                        }

                        else if(o->verbose)
                            printf("  target 0x%x (reloc at 0x%x) is not in a segment!\n", target, reloc[ir].off);
                    }
                }
            }

            /* found a vector table, process that too */
            if(vtable != 0) {
                if(o->verbose)
                    printf("  vector table found at 0x%x\n", vtable);

                voff = reloc[ir].off - 4;

                /* keep going as long as relocations happen every dword */
                while(reloc[ir].off == voff + 4) {
                    s = image_seg_find(reloc[ir].target);

                    if(s == NULL || s->type == seg_BSS)
                        type = label_BSS;

                    else if(vtype & label_CODE && s->type == seg_DATA)
                        type = label_DATA;

                    else
                        type = vtype;

                    /* add the label */
                    label_insert(reloc[ir].target, type, s);

                    /* and a reference to it */
                    ref_insert(reloc[ir].off, reloc[ir].target);

                    /* next reloc, next vector */
                    ir++;
                    voff += 4;
                }

                ir--;
                vtable = 0;
            }

            /* next instruction */
            off += len;
        }

        /* progress report */
        if(o->verbose)
            if(i % 100 == 0)
                printf("  processed %d labels\n", i);
    }

    free(l);

    return label_print_count("dis2");
}

void dis_pass3(FILE *f) {
    int i, j = 0, len, ti;
    uint32_t off;
    char line[256], line2[256], *pos, *num, *rest;
    label_t *l;

    printf("dis3: disassembly, pass 3 - full disassembly and output\n");

    fprintf(f, "\nSECTION .text\n");

    /* loop the code labels */
    for(i = 0; i < nlabel; i++) {
        if(!(label[i].type & label_CODE)) continue;

        /* output the label */
        if((label[i].type & label_CODE_ENTRY) == label_CODE_ENTRY)
            fprintf(f, "\n\nENTRY:                      ; off = %x\n\n", label[i].target);
        else if((label[i].type & label_CODE_CALL) == label_CODE_CALL)
            fprintf(f, "\n\nCALL_%06d:                  ; off = %x\n\n", label[i].num, label[i].target);
        else if((label[i].type & label_CODE_JUMP) == label_CODE_JUMP)
            fprintf(f, "\n\nJUMP_%06d:                  ; off = %x\n\n", label[i].num, label[i].target);

        off = label[i].target;

        while(1) {
            /* get to the nearest reference from this instruction */
            for(; j < nref; j++)
                if(ref[j].off >= off)
                    break;

            /* get a line */
            len = disasm(o->image.core + off, line, sizeof(line), 32, off, 1, 0);
            if(len == 0)
                len = eatbyte(o->image.core + off, line, sizeof(line));

            /* bail if we've gone past the next label */
            if(off + len > label[i].seg->cend || (i < nlabel - 1 && off + len > label[i + 1].target))
                break;

            /* turn memory-location-looking arguments into labels */
            pos = line;
            while((num = strstr(pos, "0x")) != NULL) {
                l = label_find(strtol(num, &rest, 16));
                if(l == NULL) {
                    pos = rest;
                    continue;
                }

                for(ti = 0; ti < ref[j].ntarget; ti++)
                    if(l->target == ref[j].target[ti])
                        break;

                if(ti == ref[j].ntarget) {
                    pos = rest;
                    continue;
                }

                *num = '\0';

                if((l->type & label_CODE_ENTRY) == label_CODE_ENTRY)
                    sprintf(line2, "%sENTRY", line);
                else if((l->type & label_CODE_CALL) == label_CODE_CALL)
                    sprintf(line2, "%sCALL_%06d", line, l->num);
                else if((l->type & label_CODE_JUMP) == label_CODE_JUMP)
                    sprintf(line2, "%sJUMP_%06d", line, l->num);
                else if(l->type & label_BSS)
                    sprintf(line2, "%sBSS_%06d", line, l->num);
                else if(l->type & label_DATA)
                    sprintf(line2, "%sDATA_%06d", line, l->num);
                else if(l->type & label_IMPORT) {
                    if(l->import.symbol != NULL)
                        sprintf(line2, "%sIMPORT_%s", line, l->import.symbol);
                    else
                        sprintf(line2, "%sIMPORT_%s_%d", line, l->import.dllname, l->import.hint);
                }

                /* keep track of the start of the rest of the line, so we can get more numbers */
                pos = strchr(line2, '\0');
                sprintf(pos, "%s", rest);

                strcpy(line, line2);

                pos = line + (pos - line2);
            }

            /* write it out */
            fprintf(f, "    %s\n", line);

            off += len;
        }

        /* progress report */
        if(o->verbose)
            if(i % 100 == 0)
                printf("  processed %d labels\n", i);
    }
}
