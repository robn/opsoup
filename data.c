#include "opsoup.h"

void data_output(FILE *f) {
    int i, s, nc, is, nl, dbc, len;
    uint8_t *mem, *end, *p;
    label_t *l;

    printf("data: writing data section\n");

    fprintf(f, "\n\nSECTION .data\n");

    /* loop the data labels */
    for(i = 0; i < nlabel; i++) {
        if(!(label[i].type & label_DATA)) continue;

        /* output the label */
        if(label[i].type & label_VTABLE)
            fprintf(f, "\n; vector table\n");
        else
            fprintf(f, "\n");
        fprintf(f, "DATA_%06d:                  ; off = %x\n", label[i].num, (uint32_t) (label[i].target - label[i].seg->start));

        mem = label[i].target;

        /* where is the next label (or end of segment)? */
        if(i == nlabel - 1 || label[i].seg != label[i + 1].seg)
            end = label[i].seg->end;
        else
            end = label[i + 1].target;

        /* look for strings */
        s = 0;
        if(end[-1] == 0x0) {
            nc = 0;
            for(p = mem; p < end-1; p++)
                if(*p != 0x0 && (*p < 0x20 || *p > 0x7e))
                    nc++;

            if((end - mem) >> 4 >= nc) {
                s = 1;
            }
        }

        if(label[i].type & label_VTABLE) {
            len = (end - mem) & 0xfffffffc;

            while(len > 0) {
                l = label_find((uint8_t *) * (uint32_t *) mem);
                if(l != NULL) {
                    if((l->type & label_CODE_CALL) == label_CODE_CALL)
                        fprintf(f, "    dd CALL_%06d\n", l->num);
                    else if((l->type & label_CODE_JUMP) == label_CODE_JUMP)
                        fprintf(f, "    dd JUMP_%06d\n", l->num);
                    else if(l->type & label_BSS)
                        fprintf(f, "    dd BSS_%06d\n", l->num);
                    else if(l->type & label_DATA)
                        fprintf(f, "    dd DATA_%06d\n", l->num);
                    else if(l->type & label_IMPORT) {
                        if(l->import.symbol != NULL)
                            fprintf(f, "    dd IMPORT_%s\n", l->import.symbol);
                        else
                            fprintf(f, "    dd IMPORT_%s_%d\n", l->import.dllname, l->import.hint);
                    }
                    else
                        l = NULL;
                }

                if(l == NULL)
                    fprintf(f, "    db 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", mem[0], mem[1], mem[2], mem[3]);

                mem += 4; len -= 4;
            }
        }

        else {
            dbc = 0;
            nl = 1;
            is = 0;

            while(mem < end) {
                if(nl)
                    fprintf(f, "    db ");

                if(s && *mem >= 0x20 && *mem <= 0x7e && *mem != 0x27) {
                    if(nl)
                        fputc(0x27, f);
                    else if(!is)
                        fprintf(f, ", '");

                    fputc(*mem, f);

                    nl = 0;
                    is = 1;
                }

                else {
                    if(is) {
                        fprintf(f, "', ");
                        is = 0;
                    }
                    else if(!nl)
                        fprintf(f, ", ");

                    fprintf(f, "0x%02x", *mem);

                    nl = 0;

                    dbc++;
                    if(dbc == 8)
                        nl = 1;

                    if(s && (*mem == 0xa || *mem == 0xd) && !(mem < end-1 && mem[1] == (*mem == 0xd ? 0xa : 0xd)))
                        nl = 1;
                }

                if(nl || mem == end-1) {
                    if(is)
                        fputc(0x27, f);
                    fputc('\n', f);
                    dbc = 0;
                    is = 0;
                }

                mem++;
            }
        }


        /* progress report */
        if(o->verbose)
            if(i % 100 == 0)
                printf("  processed %d labels\n", i);
    }
}

void data_bss_output(FILE *f) {
    int i;
    uint8_t *mem, *end;

    printf("data: writing bss section\n");

    fprintf(f, "\n\nSECTION .bss\n");

    /* loop the data labels */
    for(i = 0; i < nlabel; i++) {
        if(!(label[i].type & label_BSS)) continue;

        /* output the label */
        if(label[i].type & label_VTABLE)
            fprintf(f, "\n; vector table\n");
        else
            fprintf(f, "\n");
        fprintf(f, "BSS_%06d:                   ; off = %x\n", label[i].num, (uint32_t) (label[i].target - label[i].seg->start));

        mem = label[i].target;

        /* where is the next label (or end of segment)? */
        if(i == nlabel - 1 || label[i].seg != label[i + 1].seg)
            end = label[i].seg->start + label[i].seg->size;
        else
            end = label[i + 1].target;

        /* easy */
        fprintf(f, "    resb 0x%x\n", end - mem);

        /* progress report */
        if(o->verbose)
            if(i % 100 == 0)
                printf("  processed %d labels\n", i);
    }
}
