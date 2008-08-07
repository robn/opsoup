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
        o->label[i].num = 0;

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

void label_ref_check(void) {
    int i, j;
    // segment_t *s;

    for(i = 0; i < o->nref; i++)
        for(j = 0; j < o->ref[i].ntarget; j++)
            (label_find(o->ref[i].target[j]))->num++;
    
    i = 0;
    while(i < o->nlabel) {
        //if(o->label[i].type & label_CODE && o->label[i].num == 0 && o->label[i].type != label_CODE_ENTRY) {
        if(o->label[i].num == 0) {
            if(i < o->nlabel - 1)
                memmove(&(o->label[i]), &(o->label[i + 1]), sizeof(label_t) * (o->nlabel - i + 1));
            o->nlabel--;
        }
        else
            i++;
    }

    /*
    for(i = 0; i < o->nlabel; i++)
        if(o->label[i].type & label_CODE && o->label[i].num == 0 && o->label[i].type != label_CODE_ENTRY) {
            s = image_seg_find(o->label[i].target);
            if(s == NULL || s->type == seg_DATA)
                o->label[i].type = label_BSS;
            else
                o->label[i].type = label_DATA;
        }
    */
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

    label_print_count("label");
}

int label_print_count(char *str) {
    int changed = 0;

    printf("%s: %d labels added, %d upgraded (%d total)\n", str, o->added, o->upgraded, o->nlabel);

    if(o->added != 0 || o->upgraded != 0)
        changed = 1;

    o->added = o->upgraded = 0;

    return changed;
}

/* number o->labels */
void label_number(void) {
    int i, cc = 0, jc = 0, dc = 0, bc = 0;

    for(i = 0; i < o->nlabel; i++) {
        if((o->label[i].type & label_CODE_CALL) == label_CODE_CALL) {
            o->label[i].num = cc;
            cc++;
        }

        else if((o->label[i].type & label_CODE_JUMP) == label_CODE_JUMP) {
            o->label[i].num = jc;
            jc++;
        }

        else if(o->label[i].type & label_BSS) {
            o->label[i].num = bc;
            bc++;
        }
        
        else if(o->label[i].type & label_DATA) {
            o->label[i].num = dc;
            dc++;
        }
    }
    
    printf("labels: call %d jump %d data %d bss %d\n", cc, jc, dc, bc);
}

void label_print_unused(void) {
    int i;

    for (i = 0; i < o->nlabel; i++)
        if (o->label[i].count == 0)
            printf("  unused label '%s' at %p\n", o->label[i].name, o->label[i].target);
}
