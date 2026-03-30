#include "global.h"

uint64_t hash(uint64_t n) {
    n = (n ^ (n >> 30)) * 0xbf58476d1ce4e5b9ull;
    n = (n ^ (n >> 27)) * 0x94d049bb133111ebull;
    n = n ^ (n >> 31);
    return n;
}