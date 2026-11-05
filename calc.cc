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
        // perror("fopen");
        fprintf(stderr, "no such file: %s\n", filename);
        exit(-1);
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
const char *filename = NULL;

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
    int log_level = argc>2? (int)strtol(argv[2], NULL, 10): 4096;
    // fprintf(stderr, "log_level = %d\n", log_level);

    char *str = NULL;
    if (ends_with(arg1, ".al")) {
        filename = arg1;
        file_str = read_file(filename);
        str = file_str;
    } else {
        str = arg1;
    }

    ArithmeticExpression ari_exp(str, 0, 0, {}, {}, CTXT_BGN, log_level, filename);
    ari_exp.parse();

    if (!(log_level & LLL_GEN_INSTRUCT)) {
        Opd *return_value_ptr = ari_exp.getReturnValuePtr();
        if (return_value_ptr != NULL) {
            fprintf(stderr, "%s:%d:1: note: *return_value_ptr=(%d,%d,%s,%g,\"%s\")\n",
                    __FILE__,
                    __LINE__,
                    return_value_ptr->token_idx,
                    return_value_ptr->stmt_count,
                    getOpdTypeStr(return_value_ptr->type),
                    return_value_ptr->num,
                    return_value_ptr->str.c_str()
                );
        // } else {
        //     fprintf(stderr, "%s:%d:1: note: no return value\n",
        //             __FILE__,
        //             __LINE__
        //         );
        }
    }


    return 0;
}
