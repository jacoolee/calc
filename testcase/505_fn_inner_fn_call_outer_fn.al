function outer() {
    print("in outer");
}

function f() {
    print("in f");

    function fi() {
        print("in fi");
        outer();
    }

    fi();
}

f();
