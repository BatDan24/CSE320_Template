/* main.c — source loader, fd lifecycle, CLI driver.
 *
 * Student-editable module.
 */
#include "svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define STDIN_CAP (1 << 20)

/* Read the whole source into a freshly malloc'd, NUL-terminated buffer. */
static char *load_source(const char *path, size_t *out_len) {
    if (strcmp(path, "-") == 0) {
        char  *buf = malloc(STDIN_CAP + 1);
        size_t len = read(STDIN_FILENO, buf, STDIN_CAP);
        buf[len] = '\0';
        *out_len = len;
        return buf;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror(path); return NULL; }

    struct stat st;
    fstat(fd, &st);
    char  *buf = malloc(st.st_size + 1);
    size_t len = read(fd, buf, st.st_size);
    buf[len] = '\0';
    *out_len = len;
    close(fd);
    return buf;
}

static char *default_output(const char *in) {
    if (strcmp(in, "-") == 0) return strdup("out.bvm");
    const char *dot = strrchr(in, '.');
    size_t base = dot ? (size_t)(dot - in) : strlen(in);
    char *out = malloc(base + 5);
    memcpy(out, in, base);
    strcpy(out + base, ".bvm");
    return out;
}

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s <input.sl|-> [-o out.bvm]\n", prog);
}

int main(int argc, char **argv) {
    const char *input = NULL;
    char       *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = strdup(argv[++i]);
        } else if (input == NULL) {
            input = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (!input) { usage(argv[0]); return 2; }
    if (!output) output = default_output(input);

    size_t len = 0;
    char *src = load_source(input, &len);
    if (!src) { fprintf(stderr, "svc: could not read '%s'\n", input); free(output); return 1; }

    AstNode *prog = parse_program(src, len, input);
    if (!prog) { fprintf(stderr, "svc: compilation failed\n"); free(src); free(output); return 1; }

    Module *m = codegen(prog);
    if (!m) { fprintf(stderr, "svc: codegen failed\n"); free(src); free(output); return 1; }

    if (emit_module(m, output) != 0) {
        fprintf(stderr, "svc: could not write '%s'\n", output);
        module_free(m); free(src); free(output);
        return 1;
    }

    module_free(m);
    free(src);
    free(output);
    return 0;
}
