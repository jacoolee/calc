#include "arithmetic_expression.hh"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ends_with(const char *str, const char *suffix)
{
    size_t len = strlen(str);
    size_t suffix_len = strlen(suffix);

    return len >= suffix_len &&
        strcmp(str + len - suffix_len, suffix) == 0;
}

char *read_file(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    char *str = (char*)malloc(size + 1);
    if (!str) {
        fclose(fp);
        return NULL;
    }

    size_t n = fread(str, 1, size, fp);
    str[n] = '\0';

    fclose(fp);
    return str;
}

char *file_str = NULL;

void cleanup(void) {
    free(file_str);
}

int main(int argc, char *argv[])
{
    if (argc == 1 || strcmp(argv[1], "") == 0) {
        fprintf(stderr, "Usage: calc math_expression | file.al\n");
        return 0;
    }

    atexit(cleanup);

    char* arg1 = argv[1];
    int debug = argc>2?(int(*argv[2])==48?0:1):0;

    char *str = NULL;
    if (ends_with(arg1, ".al")) {
        file_str = read_file(arg1);
        str = file_str;
    } else {
        str = arg1;
    }

    ArithmeticExpression ari_exp(str, 0, 0, debug);
    ari_exp.parse();

    return 0;
}
