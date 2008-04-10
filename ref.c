#include "decompile.h"

ref_t *ref = NULL;
int nref = 0, sref = 0;

/* find a ref, return its index */
static int _ref_find_ll(u32 off) {
    ref_t *rf = ref;
    int nrf = nref, abs = 0, i = 1;

    if(rf == NULL)
        return -1;

    while(!(i == 0 && (nrf == 0 || nrf == 1))) {
        /* cut the list in half */
        i = nrf >> 1;
        
        /* found it, return */
        if(rf[i].off == off)
            return abs + i;

        /* search the bottom half */
        if(rf[i].off > off)
            nrf = i;

        /* search the top half */
        else {
            rf = &(rf[i]);
            nrf -= i;
            abs += i;
        }
    }

    return - (abs + i + 1);
}

/* add a ref to the array */
ref_t *ref_insert(u32 off, u32 target) {
    int i, ti;

    i = _ref_find_ll(off);
    if(i < 0) {
        /* not found, search forward for the insertion point */
        i = -i - 1;
        for(; i < nref && ref[i].off < off; i++);

        /* make space if necessary */
        if(nref == sref) {
            sref += 1024;
            ref = (ref_t *) realloc(ref, sizeof(ref_t) * sref);
        }

        if(i < nref)
            memmove(&(ref[i + 1]), &(ref[i]), sizeof(ref_t) * (nref - i));

        nref++;

        /* fill it out */
        ref[i].off = off;
        ref[i].ntarget = 0;
    }

    /* if we've already got this target on this reference, just return it */
    for(ti = 0; ti < ref[i].ntarget; ti++)
        if(ref[i].target[ti] == target)
            return &(ref[i]);

    /* fail if we can't add any more targets */
    if(ref[i].ntarget == MAX_REF_TARGET) {
        if(verbose)
            printf("  more than %d targets for label ref 0x%x\n", MAX_REF_TARGET, off);
        return &(ref[i]);
    }

    /* add the target */
    ref[i].target[ref[i].ntarget] = target;
    ref[i].ntarget++;

    return &(ref[i]);
}
