#include "opsoup.h"

/* see if this r/m operand has a 32-bit displacement */
int _rm_disp32(uint8_t *mem, uint8_t *reg) {
    uint8_t mod, rm;

    /* mod is the top two bits of the first byte */
    mod = mem[0] >> 6;

    /* if mod is 3, then its a direct register access, so bail */
    if(mod == 3)
        return 0;

    /* rm is the bottom three bits */
    rm = mem[0] & 0x7;

    /* reg is the middle three bits */
    if(reg != NULL)
        *reg = (mem[0] & 0x38) >> 3;

    /* 4-byte displacement */
    if(mod == 2 || (mod == 0 && rm == 5) || (mod == 0 && rm == 4 && (mem[1] & 0x7) == 5))
        return 1;

    return 0;
}

/* see if the current instruction has a memory access through a vector table */
static label_type_t _vtable_access(uint8_t *mem) {
    uint8_t b, reg;

    b = mem[0];

    /* skip segment override prefixes */
    if(b == 0x26 || b == 0x2e || b == 0x36 || b == 0x3e || b == 0x64 || b == 0x65) {
        mem++;
        b = mem[0];
    }

    /*
     *  jmp r/m32 == 0xff /4
     * call r/m32 == 0xff /2
     */
    if(b == 0xff && _rm_disp32(mem+1, &reg)) {
        if(reg == 4)
            return label_CODE_JUMP;
        if(reg == 2)
            return label_CODE_CALL;
    }

    /*
     * mov reg32,r/m32 == 0x8b /r
     */
    if(b == 0x8b && _rm_disp32(mem+1, NULL))
        return label_DATA;

    /*
     * movzx reg32,r/m8 == 0xf 0xb6 /r
     */
    if(b == 0xf && mem[1] == 0xb6 && _rm_disp32(mem+2, NULL))
        return label_DATA;

    /*
     * mov reg8,r/m8 == 0x8a /r
     * test r/m32,reg32 == 0x85 /r
     */
    if((b == 0x8a || b == 0x85) && _rm_disp32(mem+1, NULL))
        return label_DATA;

    return 0;
}

/* see if the current instruction has a memory access */
static int _mem_access(uint8_t *mem) {
    uint8_t b, reg;

    b = mem[0];

    /* skip segment override prefixes */
    if(b == 0x26 || b == 0x2e || b == 0x36 || b == 0x3e || b == 0x64 || b == 0x65) {
        mem++;
        b = mem[0];
    }

    /*
     * mov r/m32,mem32 == 0xc7 /0
     */
    if(b == 0xc7) {
        _rm_disp32(mem+1, &reg);
        if(reg == 0)
            return 1;
    }

    /*
     * cmp r/m32,mem32 == 0x81 /0
     */
    if(b == 0x81) {
        _rm_disp32(mem+1, &reg);
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

static void _target_extract(uint8_t *mem, uint8_t **target, label_type_t *type) {
    uint8_t b1, b2;

    b1 = mem[0];

    /* skip segment override and offset/address prefixes */
    if(b1 == 0x26 || b1 == 0x2e || b1 == 0x36 || b1 == 0x3e || b1 == 0x64 || b1 == 0x65 || b1 == 0x66 || b1 == 0x67) {
        mem++;
        b1 = mem[0];
    }

    b2 = mem[1];

    /* grab bag of 8-bit relative instructions
     *   e0-e2: loop, loope, loopne
     *      e3: jexz, jecxz
     */
    if(b1 >= 0xe0 && b1 <= 0xe3) {
        *type = label_CODE_JUMP;
        *target = mem + 2 +  * (int8_t *) (mem+1);
    }

    /* immediate-mode (offset-based) call */
    else if(b1 == 0xe8) {
        *type = label_CODE_CALL;
        *target = mem + 5 + * (int32_t *) (mem+1);
    }

    /* short (8-bit) jump */
    else if(b1 == 0xeb) {
        *type = label_CODE_JUMP;
        *target = mem + 2 +  * (int8_t *) (mem+1);
    }

    /* long (32-bit) jump */
    else if(b1 == 0xe9) {
        *type = label_CODE_JUMP;
        *target = mem + 5 + * (int32_t *) (mem+1);
    }

    /* local (8-bit) conditional jump */
    else if(b1 >= 0x70 && b1 <= 0x7f) {
        *type = label_CODE_JUMP;
        *target = mem + 2 +  * (int8_t *) (mem+1);
    }

    /* near (32-bit) conditional jump */
    else if(b1 == 0x0f && b2 >= 0x80 && b2 <= 0x8f) {
        *type = label_CODE_JUMP;
        *target = mem + 6 + * (int32_t *) (mem+2);
    }

    else {
        *type = label_NONE;
        *target = NULL;
    }
}

void dis_pass1(void) {
    int i, len, ir = 0;
    uint8_t *mem, *target, *vtable, *vmem;
    label_type_t type, vtype;
    char line[256];
    segment_t *s;

    printf("dis1: disassembly, pass 1 - finding obvious labels\n");

    /* code segments */
    for(i = 0; o->image.segment[i].name != NULL; i++) {
        if(o->image.segment[i].type != seg_CODE) continue;

        printf("dis1: processing segment '%s' (size 0x%x)\n", o->image.segment[i].name, o->image.segment[i].size);

        /* find the first relocation in this segment */
        for(; ir < o->nreloc; ir++)
            if(o->reloc[ir].mem >= o->image.segment[i].start)
                break;
                
        /* loop the entire segment */
        mem = o->image.segment[i].start;
        while(mem < o->image.segment[i].start + o->image.segment[i].size) {
            /* get length of this instruction */
            len = disasm(mem, line, sizeof(line), 32, 0, 1, 0);
            if(len == 0)
                len = eatbyte(mem, line, sizeof(line));

            if (o->verbose)
                printf("  %s\n", line);

            /* this instruction in where the current relocation got applied */
            if(ir < o->nreloc && mem + len > o->reloc[ir].mem &&
               o->reloc[ir].mem >= o->image.segment[i].start &&
               o->reloc[ir].mem < o->image.segment[i].end) {

                target = o->reloc[ir].target;
                ir++;

                /* identifying what type of data the target points to */
                s = image_seg_find(target);
                if(s == NULL) {
                    if(o->verbose)
                        printf("    target %p (reloc at %p) is not in a segment!\n", target, o->reloc[ir].mem);
                    mem += len;
                    continue;
                }

                /* if it points to a data or bss segment, its data */
                if(s->type == seg_BSS)
                    type = label_BSS;
                else if(s->type == seg_DATA)
                    type = label_DATA;

                /* r/m32 instructions are pointing at a vector table */
                if((vtype = _vtable_access(mem))) {
                    vtable = target;
                    if(type & label_BSS)
                        type = label_BSS_VTABLE;
                    else
                        type = label_DATA_VTABLE;
                }

                /* some normal memory access */
                else if(_mem_access(mem)) {
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
                    printf("\n           reloc: 0x%x\n", o->reloc[ir].mem);
                    printf("          offset: 0x%x\n", mem);
                    printf("          target: 0x%x (%s)\n", target, s->name);
                    printf("     instruction: %s\n", line);
                    printf("           bytes:");

                    for(i = -5; i <= len + 5; i++) {
                        if(i == 0)
                            printf("\033[1;33m");
                        printf(" 0x%x", mem[i]);
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
                _target_extract(mem, &target, &type);

            if (target == NULL) {
                if (o->verbose)
                    printf("    no target\n");
                mem += len;
                continue;
            }

            /* add the label (as long as its in a segment) */
            s = image_seg_find(target);
            if(s == NULL) {
                if(o->verbose)
                    printf("    target %p (mem %p) is not in a segment!\n", target, mem);
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

                    ref_insert(mem, target);

                    /* done making label */
                    type = label_NONE;

                    /* possible second relocation on this instruction */
                    if(ir < o->nreloc && mem + len > o->reloc[ir].mem &&
                       o->reloc[ir].mem >= o->image.segment[i].start &&
                       o->reloc[ir].mem < o->image.segment[i].end) {

                        target = o->reloc[ir].target;
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
                            printf("    target %p (reloc at %p) is not in a segment!\n", target, o->reloc[ir].mem);
                    }
                }
            }

            /* found a vector table, process that too */
            if(vtable != NULL) {
                if(o->verbose)
                    printf("    vector table found at %p\n", vtable);

                vmem = o->reloc[ir].mem - 4;

                /* keep going as long as relocations happen every dword */
                while(o->reloc[ir].mem == vmem + 4) {
                    s = image_seg_find(o->reloc[ir].target);

                    if(s == NULL || s->type == seg_BSS)
                        type = label_BSS;

                    else if(vtype & label_CODE && s->type == seg_DATA)
                        type = label_DATA;

                    else
                        type = vtype;

                    /* add the label */
                    label_insert(o->reloc[ir].target, type, s);

                    ref_insert(o->reloc[ir].mem, o->reloc[ir].target);

                    /* next reloc, next vector */
                    ir++;
                    vmem += 4;
                }

                ir--;
                vtable = NULL;
            }

            /* next instruction */
            mem += len;
            
            /* progress report */
            if(o->verbose)
                if(((mem - o->image.segment[i].start) & 0xfffff000) != ((mem - o->image.segment[i].start + len) & 0xfffff000))
                    printf("  processed 0x%x bytes\n", mem + len - o->image.segment[i].start);
        }
    }

    label_print_count("dis1");
}

int dis_pass2(int n) {
    label_t *l;
    int nl, i, ir = 0, len;
    uint8_t *mem, *target, *vtable = NULL, *vmem;
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

        mem = l[i].target;

        /* skip relocs forward to this label */
        for(; ir < o->nreloc; ir++)
            if(o->reloc[ir].mem >= mem)
                break;

        /* disassemble from this label to the next */
        while(1) {
            len = disasm(mem, line, sizeof(line), 32, 0, 1, 0);
            if(len == 0)
                len = eatbyte(mem, line, sizeof(line));

            if (o->verbose)
                printf("  %s\n", line);

            /* bail if we've gone past the next label */
            if(mem + len > l[i].seg->end || (i < nl - 1 && mem + len > l[i + 1].target))
                break;

            /* this instruction in where the current relocation got applied */
            if(ir < o->nreloc && mem + len > o->reloc[ir].mem &&
               o->reloc[ir].mem >= l[i].seg->start &&
               o->reloc[ir].mem < l[i].seg->end) {
                target = o->reloc[ir].target;
                ir++;

                /* identifying what type of data the target points to */
                s = image_seg_find(target);
                if(s == NULL) {
                    if(o->verbose)
                        printf("    target %p (reloc at %p) is not in a segment!\n", target, o->reloc[ir].mem);
                    mem += len;
                    continue;
                }

                /* if it points to a data or bss segment, its data */
                if(s->type == seg_BSS)
                    type = label_BSS;
                else if(s->type == seg_DATA)
                    type = label_DATA;

                /* r/m32 instructions are pointing at a vector table */
                if((vtype = _vtable_access(mem))) {
                    vtable = target;
                    if(type & label_BSS)
                        type = label_BSS_VTABLE;
                    else
                        type = label_DATA_VTABLE;
                }

                /* some normal memory access */
                else if(_mem_access(mem)) {
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
                    printf("\n           reloc: 0x%x\n", o->reloc[ir].mem);
                    printf("          offset: 0x%x\n", mem);
                    printf("          target: 0x%x (%s)\n", target, s->name);
                    printf("     instruction: %s\n", line);
                    printf("           bytes:");

                    for(i = -5; i <= len + 5; i++) {
                        if(i == 0)
                            printf("\033[1;33m");
                        printf(" 0x%x", mem[i]);
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
                _target_extract(mem, &target, &type);

            if (target == NULL) {
                if (o->verbose)
                    printf("    no target\n");
                mem += len;
                continue;
            }

            /* add the label (as long as its in a segment) */
            s = image_seg_find(target);
            if(s == NULL) {
                if(o->verbose)
                    printf("    target %p (offset %p) is not in a segment!\n", target, mem);
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
                    ref_insert(mem, target);

                    /* done making label */
                    type = label_NONE;

                    /* possible second relocation on this instruction */
                    if(ir < o->nreloc && mem + len > o->reloc[ir].mem &&
                       o->reloc[ir].mem >= l[i].seg->start &&
                       o->reloc[ir].mem < l[i].seg->end) {
                        target = o->reloc[ir].target;
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
                            printf("    target %p (reloc at %p) is not in a segment!\n", target, o->reloc[ir].mem);
                    }
                }
            }

            /* found a vector table, process that too */
            if(vtable != NULL) {
                if(o->verbose)
                    printf("    vector table found at %p\n", vtable);

                vmem = o->reloc[ir].mem - 4;

                /* keep going as long as relocations happen every dword */
                while(o->reloc[ir].mem == vmem + 4) {
                    s = image_seg_find(o->reloc[ir].target);

                    if(s == NULL || s->type == seg_BSS)
                        type = label_BSS;

                    else if(vtype & label_CODE && s->type == seg_DATA)
                        type = label_DATA;

                    else
                        type = vtype;

                    /* add the label */
                    label_insert(o->reloc[ir].target, type, s);

                    /* and a reference to it */
                    ref_insert(o->reloc[ir].mem, o->reloc[ir].target);

                    /* next reloc, next vector */
                    ir++;
                    vmem += 4;
                }

                ir--;
                vtable = NULL;
            }

            /* next instruction */
            mem += len;
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
    uint8_t *mem;
    char line[256], line2[256], *pos, *num, *rest;
    label_t *l;

    printf("dis3: disassembly, pass 3 - full disassembly and output\n");

    fprintf(f, "\nSECTION .text\n");

    /* loop the code labels */
    for(i = 0; i < nlabel; i++) {
        if(!(label[i].type & label_CODE)) continue;

        /* output the label */
        if (label[i].name)
            fprintf(f, "\n\n%s:                     ; off = %x\n\n", label[i].name, (uint32_t) (label[i].target - label[i].seg->start));
        else if((label[i].type & label_CODE_CALL) == label_CODE_CALL)
            fprintf(f, "\n\nCALL_%06d:                  ; off = %x\n\n", label[i].num, (uint32_t) (label[i].target - label[i].seg->start));
        else if((label[i].type & label_CODE_JUMP) == label_CODE_JUMP)
            fprintf(f, "\n\nJUMP_%06d:                  ; off = %x\n\n", label[i].num, (uint32_t) (label[i].target - label[i].seg->start));

        /*
        if ((label[i].type & label_CODE_JUMP) == label_CODE_JUMP && label[i].num == 2)
            asm("int3");
        */

        mem = label[i].target;

        while(1) {
            /* get to the nearest reference from this instruction */
            for(; j < nref; j++)
                if(ref[j].mem >= mem)
                    break;

            /* get a line */
            len = disasm(mem, line, sizeof(line), 32, 0, 1, 0);
            if(len == 0)
                len = eatbyte(mem, line, sizeof(line));

            /* bail if we've gone past the next label */
            if(mem + len > label[i].seg->end || (i < nlabel - 1 && mem + len > label[i + 1].target))
                break;

            /* turn memory-location-looking arguments into labels */
            pos = line;
            while((num = strstr(pos, "0x")) != NULL) {
                uint8_t *target;

                errno = 0;
                if (num[-1] == '-')
                    target = (uint8_t *) -strtoul(num, &rest, 16);
                else
                    target = (uint8_t *) strtoul(num, &rest, 16);
                if (errno) {
                    fprintf(stderr, "dis3: can't convert string starting '%s' to pointer: %s\n", num, strerror(errno));
                    abort();
                }

                l = label_find(target);
                if(l == NULL) {
                    label_type_t type;

                    /* hackishly handle local jumps */
                    _target_extract(mem, &target, &type);

                    if (type == label_CODE_JUMP)
                        l = label_find(target);

                    if (l == NULL) {
                        pos = rest;
                        continue;
                    }
                }

                if (num[-1] == '-')
                    num[-1] = '+';

                for(ti = 0; ti < ref[j].ntarget; ti++)
                    if(l->target == ref[j].target[ti])
                        break;

                if(ti == ref[j].ntarget) {
                    pos = rest;
                    continue;
                }

                *num = '\0';

                if (l->name)
                    sprintf(line2, "%s%s", line, l->name);
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

            mem += len;
        }

        /* progress report */
        if(o->verbose)
            if(i % 100 == 0)
                printf("  processed %d labels\n", i);
    }
}
