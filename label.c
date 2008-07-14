#include "opsoup.h"

label_t *label = NULL;
int nlabel = 0, slabel = 0;
int added = 0, upgraded = 0;

/* find a label, return its index */
static int _label_find_ll(uint32_t target) {
    label_t *l = label;
    int nl = nlabel, abs = 0, i = 1;

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

/* find a label and return it */
label_t *label_find(uint32_t target) {
    int i;

    i = _label_find_ll(target);
    if(i < 0)
        return NULL;

    return &(label[i]);
}

/* add a label to the array */
label_t *label_insert(uint32_t target, label_type_t type, segment_t *s) {
    int i;

    i = _label_find_ll(target);
    if(i < 0) {
        /* not found, search forward for the insertion point */
        i = -i - 1;
        for(; i < nlabel && label[i].target < target; i++);

        /* make space if necessary */
        if(nlabel == slabel) {
            slabel += 1024;
            label = (label_t *) realloc(label, sizeof(label_t) * slabel);
        }

        if(i < nlabel)
            memmove(&(label[i + 1]), &(label[i]), sizeof(label_t) * (nlabel - i));

        nlabel++;
        added++;

        /* fill it out */
        label[i].target = target;
        label[i].type = type;
        label[i].seg = s;
        label[i].num = 0;
    }

    else if((type & ~0xf) > (label[i].type & ~0xf)) {
        if(o.verbose)
            printf("  upgrading 0x%x from %02x to %02x\n", target, label[i].type, type);
        label[i].type = type;
        upgraded++;
    }

    return &(label[i]);
}

/* drop a label */
void label_remove(uint32_t target) {
    int i;

    i = _label_find_ll(target);
    if(i < 0)
        return;

    if(i < nlabel - 1)
        memmove(&(label[i]), &(label[i + 1]), sizeof(label_t) * (nlabel - i + 1));
    nlabel--;
}

void label_ref_check(void) {
    int i, j;
    segment_t *s;

    for(i = 0; i < nref; i++)
        for(j = 0; j < ref[i].ntarget; j++)
            (label_find(ref[i].target[j]))->num++;
    
    i = 0;
    while(i < nlabel) {
        //if(label[i].type & label_CODE && label[i].num == 0 && label[i].type != label_CODE_ENTRY) {
        if(label[i].num == 0 && label[i].type != label_CODE_ENTRY) {
            if(i < nlabel - 1)
                memmove(&(label[i]), &(label[i + 1]), sizeof(label_t) * (nlabel - i + 1));
            nlabel--;
        }
        else
            i++;
    }

    /*
    for(i = 0; i < nlabel; i++)
        if(label[i].type & label_CODE && label[i].num == 0 && label[i].type != label_CODE_ENTRY) {
            s = image_seg_find(label[i].target);
            if(s == NULL || s->type == seg_DATA)
                label[i].type = label_BSS;
            else
                label[i].type = label_DATA;
        }
    */
}

void label_reloc_upgrade(void) {
    int i;

    printf("label: upgrading unused reloc labels\n");

    for(i = 0; i < nlabel; i++) {
        if(!(label[i].type & label_RELOC)) continue;
        if(o.verbose)
            printf("  upgrading 0x%x from %02x to %02x\n", label[i].target, label[i].type, label_DATA);
        label[i].type = label_DATA;
        upgraded++;
    }

    label_print_count("label");
}

int label_print_count(char *str) {
    int changed = 0;

    printf("%s: %d labels added, %d upgraded (%d total)\n", str, added, upgraded, nlabel);

    if(added != 0 || upgraded != 0)
        changed = 1;

    added = upgraded = 0;

    return changed;
}

/* number labels */
void label_number(void) {
    int i, cc = 0, jc = 0, dc = 0, bc = 0;

    for(i = 0; i < nlabel; i++) {
        if((label[i].type & label_CODE_CALL) == label_CODE_CALL) {
            label[i].num = cc;
            cc++;
        }

        else if((label[i].type & label_CODE_JUMP) == label_CODE_JUMP) {
            label[i].num = jc;
            jc++;
        }

        else if(label[i].type & label_BSS) {
            label[i].num = bc;
            bc++;
        }
        
        else if(label[i].type & label_DATA) {
            label[i].num = dc;
            dc++;
        }
    }
    
    printf("labels: call %d jump %d data %d bss %d\n", cc, jc, dc, bc);
}
