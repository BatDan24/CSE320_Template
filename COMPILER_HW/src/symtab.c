/* symtab.c — open-addressed hash table with a scope stack.
 *
 * Student-editable module.
 *
 * Names are interned char* pointers, so identity == pointer equality and the
 * hash is over the pointer value.  Shadowing is represented by inserting a
 * second entry for the same name at a deeper `depth`; lookup returns the entry
 * with the greatest depth along the probe chain.  On scope exit the entries
 * declared at that depth are removed.
 */
#include "svc.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Sentinel distinct from both NULL (empty) and any real interned pointer. */
static char tombstone_obj;
#define TOMBSTONE ((const char *)&tombstone_obj)

/* Content hash (FNV-1a): deterministic and address-independent. */
static unsigned long hash_ptr(const char *p) {
    unsigned long h = 1469598103934665603UL;
    for (; *p; p++) { h ^= (unsigned char)*p; h *= 1099511628211UL; }
    return h;
}

void symtab_init(Symtab *st) {
    st->cap = 16;
    st->count = 0;
    st->depth = 0;
    st->slots = calloc(st->cap, sizeof(SymSlot));
}

void symtab_free(Symtab *st) {
    free(st->slots);
    st->slots = NULL;
}

void scope_push(Symtab *st) {
    st->depth++;
}

static void rehash(Symtab *st, int newcap) {
    SymSlot *old = st->slots;
    int oldcap = st->cap;
    st->slots = calloc(newcap, sizeof(SymSlot));
    st->cap = newcap;
    st->count = 0;
    for (int i = 0; i < oldcap; i++) {
        if (old[i].name && old[i].name != TOMBSTONE) {
            symtab_insert(st, old[i].name, old[i].type, old[i].slot);
            /* preserve depth (symtab_insert stamps current depth) */
            unsigned long h = hash_ptr(old[i].name);
            for (int p = 0; ; p++) {
                int j = (int)((h + p) % st->cap);
                if (st->slots[j].name == old[i].name && st->slots[j].slot == old[i].slot) {
                    st->slots[j].depth = old[i].depth;
                    break;
                }
            }
        }
    }
    free(old);
}

int symtab_insert(Symtab *st, const char *name, Type ty, int slot) {
    if ((st->count + 1) * 2 >= st->cap)
        rehash(st, st->cap * 2);

    unsigned long h = hash_ptr(name);
    for (int p = 0; ; p++) {
        int j = (int)((h + p) % st->cap);
        const char *k = st->slots[j].name;
        if (k == NULL || k == TOMBSTONE) {
            st->slots[j].name  = name;
            st->slots[j].type  = ty;
            st->slots[j].slot  = slot;
            st->slots[j].depth = st->depth;
            st->count++;
            return slot;
        }
    }
}

SymSlot *symtab_lookup(Symtab *st, const char *name) {
    unsigned long h = hash_ptr(name);
    SymSlot *best = NULL;
    for (int p = 0; ; p++) {
        int j = (int)((h + p) % st->cap);
        const char *k = st->slots[j].name;
        if (k == NULL)
            break;                       /* end of probe chain */
        if (k == name) {
            if (!best || st->slots[j].depth > best->depth)
                best = &st->slots[j];
        }
        /* TOMBSTONE: keep probing */
    }
    return best;
}

void scope_pop(Symtab *st) {
    for (int i = 0; i < st->cap; i++) {
        const char *k = st->slots[i].name;
        if (k && k != TOMBSTONE && st->slots[i].depth > st->depth) {
            st->slots[i].name = TOMBSTONE;   /* retire, keep probe chain intact */
        }
    }
    st->depth--;
}
