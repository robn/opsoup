#include "opsoup.h"

/* find a o->ref, return its index */
static int _ref_find_ll(uint8_t *mem) {
    ref_t *rf = o->ref;
    int nrf = o->nref, abs = 0, i = 1;

    if(rf == NULL)
        return -1;

    while(!(i == 0 && (nrf == 0 || nrf == 1))) {
        /* cut the list in half */
        i = nrf >> 1;
        
        /* found it, return */
        if(rf[i].mem == mem)
            return abs + i;

        /* search the bottom half */
        if(rf[i].mem > mem)
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
ref_t *ref_insert(uint8_t *mem, uint8_t *target) {
    int i, ti;

    i = _ref_find_ll(mem);
    if(i < 0) {
        /* not found, search forward for the insertion point */
        i = -i - 1;
        for(; i < o->nref && o->ref[i].mem < mem; i++);

        /* make space if necessary */
        if(o->nref == o->sref) {
            o->sref += 1024;
            o->ref = (ref_t *) realloc(o->ref, sizeof(ref_t) * o->sref);
        }

        if(i < o->nref)
            memmove(&(o->ref[i + 1]), &(o->ref[i]), sizeof(ref_t) * (o->nref - i));

        o->nref++;

        /* fill it out */
        o->ref[i].mem = mem;
        o->ref[i].ntarget = 0;
    }

    /* if we've already got this target on this reference, just return it */
    for(ti = 0; ti < o->ref[i].ntarget; ti++)
        if(o->ref[i].target[ti] == target)
            return &(o->ref[i]);

    /* fail if we can't add any more targets */
    if(o->ref[i].ntarget == MAX_REF_TARGET) {
        if(o->verbose)
            printf("  more than %d targets for label ref %p\n", MAX_REF_TARGET, mem);
        return &(o->ref[i]);
    }

    /* add the target */
    o->ref[i].target[o->ref[i].ntarget] = target;
    o->ref[i].ntarget++;

    return &(o->ref[i]);
}
