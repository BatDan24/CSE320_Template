// exercises functions, recursion, arithmetic, control flow, arrays, doubles
int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

double avg(int a[], int n) {
    int i;
    double sum;
    sum = 0.0;
    i = 0;
    while (i < n) {
        sum = sum + a[i];
        i = i + 1;
    }
    return sum / n;
}

int main() {
    int i;
    i = 0;
    while (i < 10) {
        print(fib(i));
        i = i + 1;
    }

    int xs[5];
    xs[0] = 10;
    xs[1] = 20;
    xs[2] = 30;
    xs[3] = 40;
    xs[4] = 50;
    print(avg(xs, 5));

    double r;
    r = 7 / 2;        // int division -> 3, then coerced to double 3.0
    print(r);
    r = 7.0 / 2;      // double division -> 3.5
    print(r);
    return 0;
}
