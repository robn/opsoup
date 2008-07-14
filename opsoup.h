#ifndef OPSOUP_H
#define OPSOUP_H 1

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "nasmlib.h"
#include "disasm.h"
#include "sync.h"


/* unsigned types */
typedef unsigned int    u32;
typedef unsigned short  u16;
typedef unsigned char   u8;

/* signed types */
typedef int             s32;
typedef short           s16;
typedef char            s8;


/* set to 1 to get more logging */
extern int              verbose;


/* segments */

/* segment types */
typedef enum {
    seg_NONE = 0,
    seg_CODE,
    seg_DATA,
    seg_BSS,
    seg_IMPORT,
    seg_RELOC
} segment_type_t;

/* segment data */
typedef struct segment_st {
    char            *name;          /* segment name, for printing */
    segment_type_t  type;           /* type */
    u32             coff;           /* core offset (ie memory location where segment starts) */
    u32             size;           /* size (in core) */
    u32             foff;           /* file offset (ie file location to load segment from) */
    u32             cend;           /* end of segment (calculated by image_load()) */
} segment_t;

extern u8           *core;
extern segment_t    segment[];

int         image_load(void);
segment_t   *image_seg_find(u32 off);

/* relocations */

typedef struct reloc_st {
    u32             off;            /* memory location of the relocation */
    u32             target;         /* where the relocated address points to (ie a label target) */
} reloc_t;

extern reloc_t  *reloc;
extern int      nreloc;

void            reloc_apply(void);


/* labels */
 
/* label types */
typedef enum {
    label_NONE          = 0x0,

    label_VTABLE        = 0x1,      /* vtable mask */

    label_RELOC         = 0x10,     /* relocation target */

    label_DATA          = 0x20,     /* data item */
    label_DATA_VTABLE   = 0x21,     /* jump/call vector table */

    label_BSS           = 0x40,     /* uninitialised data item */
    label_BSS_VTABLE    = 0x41,     /* uninitialised jump/call vector table */

    label_CODE          = 0x80,     /* code item */
    label_CODE_JUMP     = 0x82,     /* jump point ("jmp" instruction target) */
    label_CODE_CALL     = 0x84,     /* call point ("call" instruction target) */
    label_CODE_ENTRY    = 0x88,     /* entry point */

    label_IMPORT        = 0x100     /* vector to imported function */
} label_type_t;

/* extra magic for import labels */
typedef struct import_st {
    char            *dllname;       /* name of the dll this symbol is in */
    char            *symbol;        /* symbol name */
    int             hint;           /* symbol hint for if the symbol name isn't present */
} import_t;

/* label data */
typedef struct label_st {
    u32             target;         /* where this label is located */
    label_type_t    type;           /* its type */
    segment_t       *seg;           /* segment the label is in, for convenience */

    int             num;            /* label number (generated by label_number()) */

    import_t        import;         /* import fu */
} label_t;

extern label_t  *label;
extern int      nlabel;

label_t         *label_find(u32 target);
label_t         *label_insert(u32 target, label_type_t type, segment_t *s);
void            label_remove(u32 target);
void            label_ref_check(void);
void            label_reloc_upgrade(void);
int             label_print_count(char *str);
void            label_number(void);


/* label references */

/* max possible targets per offset */
#define MAX_REF_TARGET (4)

/* reference data */
typedef struct ref_st {
    u32             off;                    /* location of the reference (eg instruction that uses the label */
    u32             target[MAX_REF_TARGET]; /* location of target labels (one instruction can use more than one label) */    
    int             ntarget;                /* number of targets */
} ref_t;

extern ref_t    *ref;
extern int      nref;

ref_t           *ref_insert(u32 source, u32 target);


/* imports */
void    import_process(void);
void    import_output(FILE *f);
void    import_stub(FILE *f);


/* disassembly */
void    dis_pass1(void);
int     dis_pass2(int n);
void    dis_pass3(FILE *f);


/* data "disassembly" */
void    data_output(FILE *f);
void    data_bss_output(FILE *f);

#endif
