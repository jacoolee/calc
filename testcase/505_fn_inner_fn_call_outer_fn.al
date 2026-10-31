function outer() {
    print("in outer");
}

function f() {
    print("in f");

    function outer() {
        print("i am overwritten-outer");
    }

    function fi() {
        print("in fi");
        outer();
    }

    fi();
}

f();
