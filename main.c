#include "opsoup.h"

opsoup_t *o;

int main(int argc, char **argv) {
    opsoup_t ctx;
    int round = 1;
    FILE *f;

    o = &ctx;
    memset(o, 0, sizeof (opsoup_t));

    if(argc == 2 && strcmp(argv[1], "-v") == 0)
        o->verbose = 1;

    if(image_load() != 0)
        return 1;

    init_sync();

    dis_pass1();

    /*
    reloc_apply();

    import_process();

    init_sync();

    dis_pass1();

    while(dis_pass2(round)) {
        nref = 0;
        round++;
    }
    */

    label_reloc_upgrade();

//    label_ref_check();

    label_number();

    /*
    f = fopen(OUTPUT_FILE, "w");
    if(f == NULL) {
        printf("main: couldn't open '" OUTPUT_FILE "' for writing: %s\n", strerror(errno));
        return 1;
    }

    import_output(f);

    dis_pass3(f);

    data_output(f);
    data_bss_output(f);

    fclose(f);

    f = fopen(OUTPUT_STUB, "w");
    if(f == NULL) {
        printf("main: couldn't open '" OUTPUT_STUB "' for writing: %s\n", strerror(errno));
        return 1;
    }

    import_stub(f);

    fclose(f);
    */

    return 0;
}
