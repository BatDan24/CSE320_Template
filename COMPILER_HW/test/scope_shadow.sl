// Bug 3 probe: an inner shadow must not leak past its block.
int main() {
    int x; x = 1;
    {
        int x; x = 2;
        print(x);       // 2 (inner)
    }
    print(x);           // 1 (outer) -- buggy build prints 2
    return 0;
}
