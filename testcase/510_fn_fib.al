function fib(n) {
    if n < 2 {
        return n;
    }
    return fib(n-1) + fib(n-2);
}

n = 30;
r = fib(n);

printf("fib(");
printf(n);
printf(") = ");
print(r);

