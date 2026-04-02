#include <stdio.h>

#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"

int main(int argc, char **argv)
{
    char **foo = argp_str(.long_opt = "foo", .description = "Foo");
    char **bar = argp_str_default("default value", "b", "bar", "Bar", "bar");
    bool *qux = argp_bool(.long_opt = "qux", "Qux?");

    char *baz;
    argp_str_var(&baz, .long_opt = "baz", .description = "Baz");

    if (!argp_parse(argc, argv)) {
        fprintf(stderr, "ERROR: %s\n", argp_error());
        fprintf(stderr, "Options:\n");
        argp_print_options(stderr);
        argp_free();
        return 1;
    }

    printf("foo=%s\n", *foo);
    printf("bar=%s\n", *bar);
    printf("baz=%s\n", baz);
    printf("qux=%d\n", *qux);

    int rest_argc = argp_rest_argc();
    char **rest_argv = argp_rest_argv();
    printf("rest (%d):", rest_argc);
    for (int i = 0; i < rest_argc; i++) {
        printf(" %s", rest_argv[i]);
    }
    printf("\n");

    argp_free();
    return 0;
}
