#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    ARGPARSE_TYPE_STRING,
    ARGPARSE_TYPE_BOOL,
} ArgParse_Type;

typedef union {
    const char *string;
} ArgParse_Value;

typedef struct {
    const char *short_opt;
    const char *long_opt;
    const char *description;
} ArgParse_ArgSpec;

typedef struct {
    ArgParse_ArgSpec spec;
    ArgParse_Type type;
    bool has_default;
    ArgParse_Value default_value;
    bool owns_dest;
    void *dest;
    bool is_parsed;
} ArgParse_Arg;

typedef struct {
    ArgParse_Arg *items;
    size_t count;
    size_t capacity;
} ArgParse_Args;

typedef struct {
    const char *program_name;
    ArgParse_Args args;
    bool is_parsed;
    const ArgParse_Arg *current_arg;
    int rest_argc;
    char **rest_argv;
    char error[256];
} ArgParse_Context;

extern ArgParse_Context g_argp_ctx;

#define argp_str(...) argp_ctx_str(&g_argp_ctx, __VA_ARGS__)
#define argp_str_default(default_value, ...)                                   \
    argp_ctx_str_default(&g_argp_ctx, default_value, __VA_ARGS__)
#define argp_str_var(dest, ...) argp_ctx_str_var(&g_argp_ctx, dest, __VA_ARGS__)
#define argp_str_default_var(dest, default_value, ...)                         \
    argp_ctx_str_default_var(&g_argp_ctx, dest, default_value, __VA_ARGS__)

#define argp_bool(...) argp_ctx_bool(&g_argp_ctx, __VA_ARGS__)
#define argp_bool_var(dest, ...)                                               \
    argp_ctx_bool_var(&g_argp_ctx, dest, __VA_ARGS__)

bool argp_parse(int argc, char **argv);
const char *argp_error();
int argp_rest_argc();
char **argp_rest_argv();
void argp_print_options(FILE *stream);
void argp_free();

#define argp_ctx_str(ctx, ...)                                                 \
    argp_ctx_str_ex((ctx), (ArgParse_ArgSpec){__VA_ARGS__})
#define argp_ctx_str_default(ctx, default_value, ...)                          \
    argp_ctx_str_default_ex((ctx),                                             \
                            (ArgParse_ArgSpec){__VA_ARGS__},                   \
                            (default_value))
#define argp_ctx_str_var(ctx, dest, ...)                                       \
    argp_ctx_str_var_ex((ctx), (ArgParse_ArgSpec){__VA_ARGS__}, (dest))
#define argp_ctx_str_default_var(ctx, dest, default_value, ...)                \
    argp_ctx_str_default_var_ex((ctx),                                         \
                                (ArgParse_ArgSpec){__VA_ARGS__},               \
                                (dest),                                        \
                                (default_value))

#define argp_ctx_bool(ctx, ...)                                                \
    argp_ctx_bool_ex((ctx), (ArgParse_ArgSpec){__VA_ARGS__})
#define argp_ctx_bool_var(ctx, dest, ...)                                      \
    argp_ctx_bool_var_ex((ctx), (ArgParse_ArgSpec){__VA_ARGS__}, (dest))

char **argp_ctx_str_ex(ArgParse_Context *ctx, ArgParse_ArgSpec spec);
char **argp_ctx_str_default_ex(ArgParse_Context *ctx,
                               ArgParse_ArgSpec spec,
                               const char *default_value);
bool argp_ctx_str_var_ex(ArgParse_Context *ctx,
                         ArgParse_ArgSpec spec,
                         char **dest);
bool argp_ctx_str_default_var_ex(ArgParse_Context *ctx,
                                 ArgParse_ArgSpec spec,
                                 char **dest,
                                 const char *default_value);

bool *argp_ctx_bool_ex(ArgParse_Context *ctx, ArgParse_ArgSpec spec);
bool argp_ctx_bool_var_ex(ArgParse_Context *ctx,
                          ArgParse_ArgSpec spec,
                          bool *dest);

bool argp_ctx_parse(ArgParse_Context *ctx, int argc, char **argv);
const char *argp_ctx_error(const ArgParse_Context *ctx);
void argp_ctx_print_options(const ArgParse_Context *ctx, FILE *stream);
void argp_ctx_free(ArgParse_Context *ctx);

ArgParse_Arg *argp_ctx_register(ArgParse_Context *ctx,
                                ArgParse_ArgSpec spec,
                                ArgParse_Type type,
                                void *dest);
ArgParse_Arg *argp_ctx_register_default(ArgParse_Context *ctx,
                                        ArgParse_ArgSpec spec,
                                        ArgParse_Type type,
                                        ArgParse_Value default_value,
                                        void *dest);

#ifdef ARGPARSE_IMPLEMENTATION

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define ARGPARSE_INITIAL_CAPACITY (8)
#ifndef ARGPARSE_MAX_OPTION_LEN
#define ARGPARSE_MAX_OPTION_LEN (64)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ARGPARSE_PRINTF_LIKE(fmt_index, first_arg)                             \
    __attribute__((format(printf, fmt_index, first_arg)))
#else
#define ARGPARSE_PRINTF_LIKE(fmt_index, first_arg)
#endif

ArgParse_Context g_argp_ctx = {0};

// Private API
static ArgParse_Arg *argp__append(ArgParse_Args *args, ArgParse_Arg arg);
static void *argp__alloc_dest(ArgParse_Type type);
static void
argp__option_name(const ArgParse_Arg *arg, char *buffer, size_t buffer_size);
static void argp__set_errorf(ArgParse_Context *ctx, const char *fmt, ...)
    ARGPARSE_PRINTF_LIKE(2, 3);
static void argp__clear_error(ArgParse_Context *ctx);
static bool
argp__set_value(ArgParse_Context *ctx, ArgParse_Arg *arg, const char *value);
static bool argp__apply_default(ArgParse_Context *ctx, ArgParse_Arg *arg);
static void argp__print_default(FILE *stream, const ArgParse_Arg *arg);
static const char *argp__default_varname(const ArgParse_Arg *arg);

bool argp_parse(int argc, char **argv)
{
    return argp_ctx_parse(&g_argp_ctx, argc, argv);
}

const char *argp_error() { return argp_ctx_error(&g_argp_ctx); }

int argp_rest_argc()
{
    assert(g_argp_ctx.is_parsed);
    return g_argp_ctx.rest_argc;
}

char **argp_rest_argv()
{
    assert(g_argp_ctx.is_parsed);
    return g_argp_ctx.rest_argv;
}

void argp_print_options(FILE *stream)
{
    argp_ctx_print_options(&g_argp_ctx, stream);
}

void argp_free() { argp_ctx_free(&g_argp_ctx); }

static ArgParse_Arg *argp__append(ArgParse_Args *args, ArgParse_Arg arg)
{
    if (args->count >= args->capacity) {
        size_t new_capacity = args->capacity == 0 ? ARGPARSE_INITIAL_CAPACITY
                                                  : args->capacity * 2;
        ArgParse_Arg *new_items =
            realloc(args->items, new_capacity * sizeof(*args->items));
        if (!new_items) return NULL;

        args->items = new_items;
        args->capacity = new_capacity;
    }

    ArgParse_Arg *new_arg = &args->items[args->count++];
    *new_arg = arg;
    return new_arg;
}

static void *argp__alloc_dest(ArgParse_Type type)
{
    switch (type) {
    case ARGPARSE_TYPE_STRING: return calloc(1, sizeof(char *));
    case ARGPARSE_TYPE_BOOL: return calloc(1, sizeof(bool));
    }

    assert(0 && "UNREACHABLE: ArgumentType");
    return NULL;
}

static void
argp__option_name(const ArgParse_Arg *arg, char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return;
    }

    if (!arg) {
        snprintf(buffer, buffer_size, "<unknown>");
        return;
    } else if (arg->spec.long_opt) {
        snprintf(buffer, buffer_size, "--%s", arg->spec.long_opt);
    } else if (arg->spec.short_opt) {
        snprintf(buffer, buffer_size, "-%s", arg->spec.short_opt);
    } else {
        snprintf(buffer, buffer_size, "<unnamed>");
    }
}

static void argp__set_errorf(ArgParse_Context *ctx, const char *fmt, ...)
{
    va_list args;
    char base_error[200];
    char option[sizeof(ctx->error) - sizeof(base_error) - 1];

    if (!ctx) return;

    va_start(args, fmt);
    vsnprintf(base_error, sizeof(base_error), fmt, args);
    va_end(args);

    if (ctx->current_arg) {
        argp__option_name(ctx->current_arg, option, sizeof(option));
        snprintf(ctx->error, sizeof(ctx->error), "%s: %s", option, base_error);
    } else {
        snprintf(ctx->error, sizeof(ctx->error), "%s", base_error);
    }
}

static void argp__clear_error(ArgParse_Context *ctx)
{
    if (!ctx) return;
    ctx->error[0] = '\0';
}

const char *argp_ctx_error(const ArgParse_Context *ctx)
{
    if (!ctx) return "";
    return ctx->error;
}

static bool
argp__set_value(ArgParse_Context *ctx, ArgParse_Arg *arg, const char *value)
{
    if (!arg) {
        argp__set_errorf(ctx, "parser reached an invalid state");
        return false;
    }

    if (!value) {
        argp__set_errorf(ctx, "option requires a value");
        return false;
    }

    switch (arg->type) {
    case ARGPARSE_TYPE_STRING: {
        char **dest = (char **)arg->dest;

        char *copy = strdup(value);
        if (!copy) {
            argp__set_errorf(ctx, "out of memory");
            return false;
        }

        free(*dest);
        *dest = copy;
        return true;
    }
    case ARGPARSE_TYPE_BOOL:
        argp__set_errorf(ctx, "option does not take a value");
        return false;
    }

    argp__set_errorf(ctx, "unsupported argument type");
    assert(0 && "UNREACHABLE: ArgumentType");
    return false;
}

static bool argp__apply_default(ArgParse_Context *ctx, ArgParse_Arg *arg)
{
    switch (arg->type) {
    case ARGPARSE_TYPE_STRING: {
        const char *value = arg->default_value.string;
        if (!value) value = "";
        return argp__set_value(ctx, arg, value);
    }
    case ARGPARSE_TYPE_BOOL: {
        *(bool *)arg->dest = false;
        return true;
    }
    }

    assert(0 && "UNREACHABLE: ArgumentType");
    return false;
}

ArgParse_Arg *argp_ctx_register(ArgParse_Context *ctx,
                                ArgParse_ArgSpec spec,
                                ArgParse_Type type,
                                void *dest)
{
    if (!ctx) {
        argp__set_errorf(ctx, "parser ctx is NULL");
        return NULL;
    }

    assert((spec.short_opt && spec.short_opt[0] != '\0') ||
           (spec.long_opt && spec.long_opt[0] != '\0'));

    if (spec.short_opt && spec.short_opt[0] == '\0') {
        argp__set_errorf(ctx, "short option name must not be empty");
        return NULL;
    }

    if (spec.long_opt && spec.long_opt[0] == '\0') {
        argp__set_errorf(ctx, "long option name must not be empty");
        return NULL;
    }

    if (!spec.short_opt && !spec.long_opt) {
        argp__set_errorf(ctx, "option must define short_opt or long_opt");
        return NULL;
    }

    assert(!spec.short_opt ||
           strlen(spec.short_opt) <= ARGPARSE_MAX_OPTION_LEN);
    assert(!spec.long_opt || strlen(spec.long_opt) <= ARGPARSE_MAX_OPTION_LEN);

    if (spec.short_opt && strlen(spec.short_opt) > ARGPARSE_MAX_OPTION_LEN) {
        argp__set_errorf(ctx,
                         "short option name exceeds %d characters",
                         ARGPARSE_MAX_OPTION_LEN);
        return NULL;
    }

    if (spec.long_opt && strlen(spec.long_opt) > ARGPARSE_MAX_OPTION_LEN) {
        argp__set_errorf(ctx,
                         "long option name exceeds %d characters",
                         ARGPARSE_MAX_OPTION_LEN);
        return NULL;
    }

    ArgParse_Arg arg = {.spec = spec, .type = type};

    if (!dest) {
        dest = argp__alloc_dest(type);
        if (!dest) {
            argp__set_errorf(ctx, "out of memory");
            return NULL;
        }
        arg.owns_dest = true;
    } else {
        arg.owns_dest = false;
        switch (type) {
        case ARGPARSE_TYPE_STRING: *(char **)dest = NULL; break;
        case ARGPARSE_TYPE_BOOL: *(bool *)dest = false; break;
        }
    }

    arg.dest = dest;

    ArgParse_Arg *new_arg = argp__append(&ctx->args, arg);
    if (!new_arg) {
        argp__set_errorf(ctx, "out of memory");
        if (arg.owns_dest) free(dest);
        return NULL;
    }

    return new_arg;
}

ArgParse_Arg *argp_ctx_register_default(ArgParse_Context *ctx,
                                        ArgParse_ArgSpec spec,
                                        ArgParse_Type type,
                                        ArgParse_Value default_value,
                                        void *dest)
{
    ArgParse_Arg *new_arg = argp_ctx_register(ctx, spec, type, dest);
    if (!new_arg) return NULL;

    new_arg->has_default = true;
    new_arg->default_value = default_value;

    return new_arg;
}

char **argp_ctx_str_ex(ArgParse_Context *ctx, ArgParse_ArgSpec spec)
{
    ArgParse_Arg *arg =
        argp_ctx_register(ctx, spec, ARGPARSE_TYPE_STRING, NULL);
    if (!arg) return NULL;
    return (char **)arg->dest;
}

char **argp_ctx_str_default_ex(ArgParse_Context *ctx,
                               ArgParse_ArgSpec spec,
                               const char *default_value)
{
    ArgParse_Arg *arg =
        argp_ctx_register_default(ctx,
                                  spec,
                                  ARGPARSE_TYPE_STRING,
                                  (ArgParse_Value){.string = default_value},
                                  NULL);
    if (!arg) return NULL;
    return (char **)arg->dest;
}

bool argp_ctx_str_var_ex(ArgParse_Context *ctx,
                         ArgParse_ArgSpec spec,
                         char **dest)
{
    if (!dest) {
        argp__set_errorf(ctx, "destination is NULL");
        return false;
    }

    ArgParse_Arg *arg =
        argp_ctx_register(ctx, spec, ARGPARSE_TYPE_STRING, dest);
    return arg != NULL;
}

bool argp_ctx_str_default_var_ex(ArgParse_Context *ctx,
                                 ArgParse_ArgSpec spec,
                                 char **dest,
                                 const char *default_value)
{
    if (!dest) {
        argp__set_errorf(ctx, "destination is NULL");
        return false;
    }

    ArgParse_Arg *arg =
        argp_ctx_register_default(ctx,
                                  spec,
                                  ARGPARSE_TYPE_STRING,
                                  (ArgParse_Value){.string = default_value},
                                  dest);
    return arg != NULL;
}

bool *argp_ctx_bool_ex(ArgParse_Context *ctx, ArgParse_ArgSpec spec)
{
    ArgParse_Arg *arg = argp_ctx_register(ctx, spec, ARGPARSE_TYPE_BOOL, NULL);
    if (!arg) return NULL;
    arg->has_default = true;
    return (bool *)arg->dest;
}

bool argp_ctx_bool_var_ex(ArgParse_Context *ctx,
                          ArgParse_ArgSpec spec,
                          bool *dest)
{
    if (!dest) {
        argp__set_errorf(ctx, "destination is NULL");
        return false;
    }

    ArgParse_Arg *arg = argp_ctx_register(ctx, spec, ARGPARSE_TYPE_BOOL, dest);
    arg->has_default = true;
    return arg != NULL;
}

static void argp__print_default(FILE *stream, const ArgParse_Arg *arg)
{
    // Print default for bool args is redundant
    if (arg->type == ARGPARSE_TYPE_BOOL || !arg->has_default) return;

    fputs(" (default: ", stream);

    switch (arg->type) {
    case ARGPARSE_TYPE_STRING:
        fprintf(stream,
                "\"%s\"",
                arg->default_value.string ? arg->default_value.string : "");
        break;
    case ARGPARSE_TYPE_BOOL: assert(0 && "UNREACHABLE: bool flags don't print default`");
    }

    fputc(')', stream);
}

void argp_ctx_print_options(const ArgParse_Context *ctx, FILE *stream)
{
    if (!ctx) return;
    if (!stream) stream = stdout;

    // Calculate max line width to align options
    int max_w = 0;
    for (size_t i = 0; i < ctx->args.count; i++) {
        const ArgParse_Arg *arg = &ctx->args.items[i];
        int w = 0;
        if (arg->spec.short_opt && arg->spec.long_opt) {
            w = snprintf(NULL,
                         0,
                         "-%s, --%s",
                         arg->spec.short_opt,
                         arg->spec.long_opt);
        } else if (arg->spec.short_opt) {
            w = snprintf(NULL, 0, "-%s", arg->spec.short_opt);
        } else if (arg->spec.long_opt) {
            w = snprintf(NULL, 0, "--%s", arg->spec.long_opt);
        }

        if (w > max_w) {
            max_w = w;
        }
    }

    // Print each option, padded to align descriptions
    for (size_t i = 0; i < ctx->args.count; i++) {
        const ArgParse_Arg *arg = &ctx->args.items[i];
        char buf[128];
        buf[0] = '\0';

        if (arg->spec.short_opt && arg->spec.long_opt) {
            snprintf(buf,
                     sizeof(buf),
                     "-%s, --%s",
                     arg->spec.short_opt,
                     arg->spec.long_opt);
        } else if (arg->spec.short_opt) {
            snprintf(buf, sizeof(buf), "-%s", arg->spec.short_opt);
        } else if (arg->spec.long_opt) {
            snprintf(buf, sizeof(buf), "--%s", arg->spec.long_opt);
        }

        fprintf(stream, "  %-*s", max_w, buf);
        if (arg->spec.description) {
            fprintf(stream, "  %s", arg->spec.description);
        }
        argp__print_default(stream, arg);
        fputc('\n', stream);
    }
}

bool argp_ctx_parse(ArgParse_Context *ctx, int argc, char **argv)
{
    if (!ctx) return false;
    argp__clear_error(ctx);

    if (!argv || argc <= 0 || !argv[0]) {
        argp__set_errorf(ctx, "invalid argv input");
        return false;
    }

    if (ctx->is_parsed) {
        argp__set_errorf(ctx, "parser already used");
        return false;
    }

    ctx->current_arg = NULL;
    ctx->program_name = argv[0];

    for (int i = 1; i < argc; i++) {
        const char *token = argv[i];
        const char *value = NULL;
        const char *name;
        const char *long_eq = NULL;
        size_t name_len = 0;

        // Classify option type
        bool is_long = (token[0] == '-' && token[1] == '-');
        bool is_short =
            (token[0] == '-' && token[1] != '-' && token[1] != '\0');

        // Handle lone "--" as argument stop
        if (is_long && token[2] == '\0') {
            ctx->rest_argc = argc - (i + 1);
            ctx->rest_argv = argv + (i + 1);
            break;
        }

        // Non-option: collect remaining as rest args
        if (!is_long && !is_short) {
            ctx->rest_argc = argc - i;
            ctx->rest_argv = argv + i;
            break;
        }

        // Extract option name and embedded value (e.g. --key=val)
        if (is_long) {
            name = token + 2;
            long_eq = strchr(name, '=');
            if (long_eq) {
                name_len = (size_t)(long_eq - name);
                value = long_eq + 1;
            }
        } else {
            name = token + 1;
        }

        // Reject empty option names
        if ((!is_long && name[0] == '\0') ||
            (is_long && !long_eq && name[0] == '\0') ||
            (is_long && long_eq && name_len == 0)) {
            argp__set_errorf(ctx, "malformed option %s", token);
            return false;
        }

        // Match token against registered options
        for (size_t j = 0; j < ctx->args.count; j++) {
            ArgParse_Arg *arg = &ctx->args.items[j];

            const char *opt =
                is_long ? arg->spec.long_opt : arg->spec.short_opt;
            if (!opt) continue;

            bool match;
            if (is_long && long_eq) {
                match =
                    strncmp(name, opt, name_len) == 0 && opt[name_len] == '\0';
            } else {
                match = strcmp(name, opt) == 0;
            }
            if (!match) continue;

            ctx->current_arg = arg;

            // Consume value from next token if not embedded
            if (arg->type != ARGPARSE_TYPE_BOOL && !value) {
                if (i + 1 >= argc) {
                    argp__set_errorf(ctx, "missing value for option");
                    return false;
                }

                value = argv[++i];
            }

            if (arg->type == ARGPARSE_TYPE_BOOL) {
                *(bool *)arg->dest = true;
            } else {
                if (!argp__set_value(ctx, arg, value)) return false;
            }

            ctx->current_arg = NULL;
            arg->is_parsed = true;

            goto next_token;
        }

        argp__set_errorf(ctx, "unknown option %s", token);
        return false;

    next_token:;
    }

    // Apply defaults and check for required options
    for (size_t i = 0; i < ctx->args.count; i++) {
        ArgParse_Arg *arg = &ctx->args.items[i];

        if (!arg->is_parsed) {
            if (arg->has_default) {
                if (!argp__apply_default(ctx, arg)) return false;
            } else {
                if (arg->spec.long_opt) {
                    argp__set_errorf(ctx,
                                     "missing required option --%s",
                                     arg->spec.long_opt);
                } else if (arg->spec.short_opt) {
                    argp__set_errorf(ctx,
                                     "missing required option -%s",
                                     arg->spec.short_opt);
                }

                return false;
            }
        }
    }

    ctx->current_arg = NULL;
    ctx->is_parsed = true;
    return true;
}

void argp_ctx_free(ArgParse_Context *ctx)
{
    if (!ctx) return;

    for (size_t i = 0; i < ctx->args.count; i++) {
        ArgParse_Arg *arg = &ctx->args.items[i];

        if (arg->type == ARGPARSE_TYPE_STRING && arg->dest) {
            free(*(char **)arg->dest);
            *(char **)arg->dest = NULL;
        }

        if (arg->owns_dest) free(arg->dest);
        arg->dest = NULL;
    }

    free(ctx->args.items);
    ctx->args.items = NULL;
    ctx->args.count = 0;
    ctx->args.capacity = 0;
    ctx->is_parsed = false;
    ctx->current_arg = NULL;
    ctx->error[0] = '\0';
}

#endif // ARGPARSE_IMPLEMENTATION

#endif // ARGPARSE_H
