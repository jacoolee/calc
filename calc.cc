#include "arithmetic_expression.hh"

int main(int argc, char *argv[])
{
    if (argc == 1 || strcmp(argv[1], "") == 0) {
        printf("Usage: calc math_expression\n");
        return 0;
    }

    int x = argc>2?(int(*argv[2])==48?0:1):0;
    // printf("x=%d\n", x);

    ArithmeticExpression ari_exp(argv[1], x);
    if (! ari_exp.parse()) {
        printf("failed to parse \"%s\"\n", ari_exp.getArithmeticExpression().c_str());
        return -1;
    }

    double val=0;
    ari_exp.getExpressionValue(val);
    printf("%g\n", val);

    return 0;
}
