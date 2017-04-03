template<typename T>
T add(T x, T y)
{
    return x + y;
}

int add(int x)
{
    return add(int(3), x);
}


template<typename T>
struct A
{
    static T foo(T x) { return x; }
    static T foo() { return 0; }
    T bar(T x) const { return x; }
};

int toto(const A<double> & a)
{
    return a.bar(12.) + a.foo(13.);
}

int fib(int n)
{
    return (n == 0) ? 0 : fib(n - 1) + fib(n - 2);
}

class B
{
public:

    B() { }
    B(const B &) = delete;
    
    template<typename T>
    float oof(float x, A<T> & a) const { fib(1); return x + a.foo(0); }    
};

template<typename T, typename U>
struct C
{
    static U foo(T x) { return x; }
};

template<typename T>
struct C<T, int>
{
    static int foo(T x) { return x + 1; }
};


int main(int argc, char ** argv)
{
    int x = 1;
    int y = 3;
    B b;
    A<char> a;
    a.foo(0);
    C<char, int> c1;
    C<short, long> c2;

    fib(25);
    
    return add(x, y) + A<int>::foo(x) + b.oof(12., a) + c2.foo(x);
}
