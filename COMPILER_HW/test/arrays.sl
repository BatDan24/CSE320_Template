double sum(double a[], int n) { int i; double s; s = 0.0; i = 0; while (i < n) { s = s + a[i]; i = i + 1; } return s; }
int main() {
    double v[4];
    v[0] = 1.5; v[1] = 2.5; v[2] = 3.0; v[3] = 4.0;
    print(sum(v, 4));   // 11
    return 0;
}
