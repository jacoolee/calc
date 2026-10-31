x=0;
print(x);
function f1() {
    x:=1;
    print(x);

    if 1 {
        x:=2;
        print(x);

        function f2() {
            x:=3;
            print(x);
        }

        f2();
        print(x);

    }
    print(x);
}

f1();
print(x);
