a=1;
b=1;

x = 0;

printf("G: ");
print(x);

if a {
    x = 1;
    printf("a: ");
    print(x);

    if (b) {
        x := 2;

        printf("b: ");
        print(x);

        if 1 {
            x := 3;

            printf("b:1: ");
            print(x);
        }

    } else {
        x := 4;

        printf("b else: ");
        print(x);
    }

    printf("in a: ");
    print(x);

} else {
    printf("a else: ");
    print(x);
}

printf("G: ");
print(x);
