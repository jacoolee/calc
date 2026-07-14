#include "arithmetic_expression.hh"

int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf("Usage: %s \"math_expression\" ...\n", argv[0]);
        return 0;
    }

    ArithmeticExpression ari_exp(argv[1]);
    if (! ari_exp.parse()) {
        printf("failed to parse \"%s\"\n", ari_exp.getArithmeticExpression().c_str());
        return -1;
    }

    double val=0;
    ari_exp.getExpressionValue(val);
    printf("%g\n", val);

    return 0;
}
