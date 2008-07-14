#include "opsoup.h"

void data_output(FILE *f) {
    int i, j, s, nc, is, nl, dbc, len;
    uint32_t off, end;
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
        fprintf(f, "DATA_%06d:                  ; off = %x\n", label[i].num, label[i].target);

        off = label[i].target;

        /* where is the next label (or end of segment)? */
        if(i == nlabel - 1 || label[i].seg != label[i + 1].seg)
            end = label[i].seg->coff + label[i].seg->size;
        else
            end = label[i + 1].target;

        /* look for strings */
        s = 0;
        if(o.image.core[end - 1] == 0x0) {
            nc = 0;
            for(j = off; j < end - 1; j++)
                if(o.image.core[j] != 0x0 && (o.image.core[j] < 0x20 || o.image.core[j] > 0x7e))
                    nc++;

            if((end - off) >> 4 >= nc) {
                s = 1;
            }
        }

        if(label[i].type & label_VTABLE) {
            len = (end - off) & 0xfffffffc;

            while(len > 0) {
                l = label_find(* (uint32_t *) (o.image.core + off));
                if(l != NULL) {
                    if((l->type & label_CODE_ENTRY) == label_CODE_ENTRY)
                        fprintf(f, "    dd ENTRY\n");
                    else if((l->type & label_CODE_CALL) == label_CODE_CALL)
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
                    fprintf(f, "    db 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", o.image.core[off], o.image.core[off + 1], o.image.core[off + 2], o.image.core[off + 3]);

                off += 4; len -= 4;
            }
        }

        else {
            dbc = 0;
            nl = 1;
            is = 0;

            while(off < end) {
                if(nl)
                    fprintf(f, "    db ");

                if(s && o.image.core[off] >= 0x20 && o.image.core[off] <= 0x7e && o.image.core[off] != 0x27) {
                    if(nl)
                        fputc(0x27, f);
                    else if(!is)
                        fprintf(f, ", '");

                    fputc(o.image.core[off], f);

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

                    fprintf(f, "0x%02x", o.image.core[off]);

                    nl = 0;

                    dbc++;
                    if(dbc == 8)
                        nl = 1;

                    if(s && (o.image.core[off] == 0xa || o.image.core[off] == 0xd) && !(off < end - 1 && o.image.core[off + 1] == (o.image.core[off] == 0xd ? 0xa : 0xd)))
                        nl = 1;
                }

                if(nl || off == end - 1) {
                    if(is)
                        fputc(0x27, f);
                    fputc('\n', f);
                    dbc = 0;
                    is = 0;
                }

                off++;
            }
        }


        /* progress report */
        if(o.verbose)
            if(i % 100 == 0)
                printf("  processed %d labels\n", i);
    }
}

void data_bss_output(FILE *f) {
    int i;
    uint32_t off, end;

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
        fprintf(f, "BSS_%06d:                   ; off = %x\n", label[i].num, label[i].target);

        off = label[i].target;

        /* where is the next label (or end of segment)? */
        if(i == nlabel - 1 || label[i].seg != label[i + 1].seg)
            end = label[i].seg->coff + label[i].seg->size;
        else
            end = label[i + 1].target;

        /* easy */
        fprintf(f, "    resb 0x%x\n", end - off);

        /* progress report */
        if(o.verbose)
            if(i % 100 == 0)
                printf("  processed %d labels\n", i);
    }
}
