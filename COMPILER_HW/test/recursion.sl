int fact(int n) { if (n < 2) return 1; return n * fact(n - 1); }
int gcd(int a, int b) { while (b != 0) { int t; t = b; b = a - (a / b) * b; a = t; } return a; }
int main() {
    print(fact(6));     // 720
    print(gcd(48, 36)); // 12
    return 0;
}
