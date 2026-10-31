a=0;
function f(a) {
    print("in f");
    print(a);

    function fi(a) {
        print("in fi");
        print(a);
    }

    fi(2);
}

f(a);
print(a);
