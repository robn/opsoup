#include "opsoup.h"

/* find a o->label, return its index */
static int _label_find_ll(uint8_t *target) {
    label_t *l = o->label;
    int nl = o->nlabel, abs = 0, i = 1;

    if(l == NULL)
        return -1;

    while(!(i == 0 && (nl == 0 || nl == 1))) {
        /* cut the list in half */
        i = nl >> 1;
        
        /* found it, return */
        if(l[i].target == target)
            return abs + i;

        /* search the bottom half */
        if(l[i].target > target)
            nl = i;

        /* search the top half */
        else {
            l = &(l[i]);
            nl -= i;
            abs += i;
        }
    }

    return - (abs + i + 1);
}

/* find a o->label and return it */
label_t *label_find(uint8_t *target) {
    int i;

    i = _label_find_ll(target);
    if(i < 0)
        return NULL;

    return &(o->label[i]);
}

/* add a o->label to the array */
label_t *label_insert(uint8_t *target, label_type_t type, segment_t *s) {
    int i;

    i = _label_find_ll(target);
    if(i < 0) {
        /* not found, search forward for the insertion point */
        i = -i - 1;
        for(; i < o->nlabel && o->label[i].target < target; i++);

        /* make space if necessary */
        if(o->nlabel == o->slabel) {
            o->slabel += 1024;
            o->label = (label_t *) realloc(o->label, sizeof(label_t) * o->slabel);
        }

        if(i < o->nlabel)
            memmove(&(o->label[i + 1]), &(o->label[i]), sizeof(label_t) * (o->nlabel - i));

        o->nlabel++;
        o->added++;

        /* fill it out */
        o->label[i].target = target;
        o->label[i].type = type;
        o->label[i].seg = s;
        o->label[i].count = 0;
        o->label[i].name = NULL;

        if (o->verbose)
            printf("    added label %p type %02x\n", target, type);
    }

    else if((type & ~0xf) > (o->label[i].type & ~0xf)) {
        if(o->verbose)
            printf("    upgrading %p (%s) from %02x to %02x\n", target, o->label[i].name ? o->label[i].name : "(anon)", o->label[i].type, type);
        o->label[i].type = type;
        o->upgraded++;
    }
    
    else if (o->verbose)
        printf ("    reused label %p (%s) type %02x\n", o->label[i].target, o->label[i].name ? o->label[i].name : "(anon)", o->label[i].type);

    return &(o->label[i]);
}

/* drop a o->label */
void label_remove(uint8_t *target) {
    int i;

    i = _label_find_ll(target);
    if(i < 0)
        return;

    if(i < o->nlabel - 1)
        memmove(&(o->label[i]), &(o->label[i + 1]), sizeof(label_t) * (o->nlabel - i + 1));
    o->nlabel--;
}

void label_reloc_upgrade(void) {
    int i;

    printf("label: upgrading unused reloc labels\n");

    for(i = 0; i < o->nlabel; i++) {
        if(!(o->label[i].type & label_RELOC)) continue;
        if(o->verbose)
            printf("  upgrading %p (%s) from %02x to %02x\n", o->label[i].target, o->label[i].name ? o->label[i].name : "(anon)", o->label[i].type, label_DATA);
        o->label[i].type = label_DATA;
        o->upgraded++;
    }

    label_print_upgraded("label");
}

int label_print_upgraded(char *str) {
    int changed = 0;

    printf("%s: %d labels added, %d upgraded (%d total)\n", str, o->added, o->upgraded, o->nlabel);

    if(o->added != 0 || o->upgraded != 0)
        changed = 1;

    o->added = o->upgraded = 0;

    return changed;
}

void label_gen_names(void) {
    int i, n = 0;
    label_t *l;

    for (i = 0; i < o->nlabel; i++) {
        if (o->label[i].name != NULL) continue;

        l = &o->label[i];

        l->name = malloc(64);

        if((l->type & label_CODE_CALL) == label_CODE_CALL)
            sprintf(l->name, "_OPSOUP_CALL_%06d", n);
        else if((l->type & label_CODE_JUMP) == label_CODE_JUMP)
            sprintf(l->name, "_OPSOUP_JUMP_%06d", n);
        else if(l->type & label_BSS)
            sprintf(l->name, "_OPSOUP_BSS_%06d", n);
        else if(l->type & label_DATA)
            sprintf(l->name, "_OPSOUP_DATA_%06d", n);

        n++;
    }
}

void label_print_count(void) {
    int i, cc = 0, jc = 0, dc = 0, bc = 0;

    for(i = 0; i < o->nlabel; i++) {
        if((o->label[i].type & label_CODE_CALL) == label_CODE_CALL)
            cc++;
        else if((o->label[i].type & label_CODE_JUMP) == label_CODE_JUMP)
            jc++;
        else if(o->label[i].type & label_BSS)
            bc++;
        else if(o->label[i].type & label_DATA)
            dc++;
    }
    
    printf("labels: call %d jump %d data %d bss %d\n", cc, jc, dc, bc);
}

void label_print_unused(void) {
    int i;

    for (i = 0; i < o->nlabel; i++)
        if (o->label[i].count == 0)
            printf("  unused label '%s' at %p\n", o->label[i].name, o->label[i].target);
}

void label_extern_output(FILE *f) {
    int i;

    printf("label: writing extern declarations\n");

    for (i = 0; i < o->nlabel; i++) {
        if (!(o->label[i].type & label_EXTERN)) continue;
        fprintf(f, "EXTERN %s\n", o->label[i].name);
    }
}
