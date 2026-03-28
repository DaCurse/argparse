#include <stdio.h>

#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"

int main(int argc, char **argv)
{
    ArgParse_Context ctx = {0};
    char **foo = argp_ctx_str(&ctx, "f", "foo", .description = "Foo");
    char **bar = argp_ctx_str_default(&ctx, "default value", "b", "bar", "Bar");

    // Can bind arguments to user defined variables
    char *baz;
    argp_ctx_str_var(&ctx, &baz, .long_opt = "baz", .description = "Baz");

    if (!argp_ctx_parse(&ctx, argc, argv)) {
        fprintf(stderr, "ERROR: %s\n", argp_ctx_error(&ctx));
        fprintf(stderr, "Options:\n");
        argp_ctx_print_options(&ctx, stderr);
        argp_ctx_free(&ctx);
        return 1;
    }

    printf("foo=%s\n", *foo);
    printf("bar=%s\n", *bar);
    printf("baz=%s\n", baz);

    printf("rest (%d):", ctx.rest_argc);
    for (int i = 0; i < ctx.rest_argc; i++) {
        printf(" %s", ctx.rest_argv[i]);
    }
    printf("\n");

    argp_ctx_free(&ctx);
    return 0;
}
