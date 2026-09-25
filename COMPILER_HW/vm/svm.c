/* svm.c — the Simple VM: loads and executes a .bvm bytecode file.
 *
 * Shipped to students as an opaque prebuilt binary.  A stack machine with a
 * tagged value model (int / double / array-ref).  Observable output comes from
 * the `print` builtin; the exit status is 0 on success.
 *
 * Build:  cc -O2 -o svm vm/svm.c
 * Run:    ./svm out.bvm
 */
#include "../include/opcodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ----- value model ----- */

typedef enum { V_INT, V_DBL, V_REF } VType;

typedef struct Arr { long len; struct Val *data; } Arr;

typedef struct Val {
    VType t;
    union { long i; double d; Arr *ref; } u;
} Val;

static Val vint(long i) { Val v; v.t = V_INT; v.u.i = i; return v; }
static Val vdbl(double d){ Val v; v.t = V_DBL; v.u.d = d; return v; }
static double as_d(Val v){ return v.t == V_INT ? (double)v.u.i : v.u.d; }

/* ----- loaded module ----- */

typedef struct { int tag; long i; double d; } LConst;
typedef struct {
    char           name[64];
    int            nparams, nlocals;
    unsigned char *code;
    long           code_len;
} LFunc;
typedef struct {
    LConst *consts; long nconsts;
    LFunc  *funcs;  long nfuncs;
    long    entry;
} Prog;

static void die(const char *msg) { fprintf(stderr, "svm: %s\n", msg); exit(1); }

/* ----- little-endian readers over a byte cursor ----- */

typedef struct { unsigned char *p, *end; } Cur;

static unsigned rd_u16(Cur *c) {
    if (c->p + 2 > c->end) die("truncated bytecode");
    unsigned v = c->p[0] | (c->p[1] << 8); c->p += 2; return v;
}
static long rd_i32(Cur *c) {
    if (c->p + 4 > c->end) die("truncated bytecode");
    unsigned v = c->p[0] | (c->p[1]<<8) | (c->p[2]<<16) | ((unsigned)c->p[3]<<24);
    c->p += 4; return (int)v;
}
static unsigned long rd_u32(Cur *c) { return (unsigned long)(uint32_t)rd_i32(c); }
static long rd_i64(Cur *c) {
    if (c->p + 8 > c->end) die("truncated bytecode");
    unsigned long v = 0;
    for (int i = 0; i < 8; i++) v |= (unsigned long)c->p[i] << (8*i);
    c->p += 8; return (long)v;
}

static Prog load_prog(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) die("cannot open bytecode file");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *buf = malloc(sz);
    if (fread(buf, 1, sz, fp) != (size_t)sz) die("short read on bytecode");
    fclose(fp);

    Cur c = { buf, buf + sz };
    if (c.p + 4 > c.end ||
        c.p[0]!=BVM_MAGIC0 || c.p[1]!=BVM_MAGIC1 ||
        c.p[2]!=BVM_MAGIC2 || c.p[3]!=BVM_MAGIC3)
        die("bad magic (not a .bvm file)");
    c.p += 4;

    Prog prog;
    memset(&prog, 0, sizeof prog);
    prog.nconsts = rd_u32(&c);
    prog.consts = malloc(prog.nconsts * sizeof(LConst) + 1);
    for (long i = 0; i < prog.nconsts; i++) {
        int tag = *c.p; c.p += 8;                 /* tag + 7 pad */
        long raw = rd_i64(&c);
        prog.consts[i].tag = tag;
        if (tag == CONST_INT) prog.consts[i].i = raw;
        else { double d; memcpy(&d, &raw, 8); prog.consts[i].d = d; }
    }

    prog.nfuncs = rd_u32(&c);
    prog.entry  = rd_u32(&c);
    prog.funcs  = malloc(prog.nfuncs * sizeof(LFunc));
    for (long i = 0; i < prog.nfuncs; i++) {
        LFunc *f = &prog.funcs[i];
        unsigned nlen = rd_u16(&c);
        memset(f->name, 0, sizeof f->name);
        memcpy(f->name, c.p, nlen < 63 ? nlen : 63); c.p += nlen;
        f->nparams = rd_u16(&c);
        f->nlocals = rd_u16(&c);
        f->code_len = rd_u32(&c);
        f->code = c.p; c.p += f->code_len;
    }
    return prog;
}

/* ----- execution ----- */

static Prog P;

static Val run_func(long fidx, Val *args, int argc);

static Val do_binop(int op, Val a, Val b) {
    int both_int = (a.t == V_INT && b.t == V_INT);
    switch (op) {
    case OP_ADD: return both_int ? vint(a.u.i + b.u.i) : vdbl(as_d(a) + as_d(b));
    case OP_SUB: return both_int ? vint(a.u.i - b.u.i) : vdbl(as_d(a) - as_d(b));
    case OP_MUL: return both_int ? vint(a.u.i * b.u.i) : vdbl(as_d(a) * as_d(b));
    case OP_DIV:
        if (both_int) {
            if (b.u.i == 0) die("integer division by zero");
            return vint(a.u.i / b.u.i);
        }
        return vdbl(as_d(a) / as_d(b));
    case OP_LT: return vint(both_int ? a.u.i <  b.u.i : as_d(a) <  as_d(b));
    case OP_GT: return vint(both_int ? a.u.i >  b.u.i : as_d(a) >  as_d(b));
    case OP_LE: return vint(both_int ? a.u.i <= b.u.i : as_d(a) <= as_d(b));
    case OP_GE: return vint(both_int ? a.u.i >= b.u.i : as_d(a) >= as_d(b));
    case OP_EQ: return vint(both_int ? a.u.i == b.u.i : as_d(a) == as_d(b));
    case OP_NE: return vint(both_int ? a.u.i != b.u.i : as_d(a) != as_d(b));
    }
    die("bad binop"); return vint(0);
}

#define STACK_MAX 4096

static Val run_func(long fidx, Val *args, int argc) {
    LFunc *f = &P.funcs[fidx];
    Val *locals = calloc(f->nlocals > 0 ? f->nlocals : 1, sizeof(Val));
    for (int i = 0; i < argc && i < f->nlocals; i++) locals[i] = args[i];

    Val st[STACK_MAX];
    int sp = 0;
    #define PUSH(v) do { if (sp >= STACK_MAX) die("stack overflow"); st[sp++] = (v); } while (0)
    #define POP()   (sp > 0 ? st[--sp] : (die("stack underflow"), st[0]))

    Cur c = { f->code, f->code + f->code_len };
    Val ret = vint(0);

    while (c.p < c.end) {
        int op = *c.p++;
        switch (op) {
        case OP_PUSHC: {
            unsigned long k = rd_u32(&c);
            LConst *cn = &P.consts[k];
            PUSH(cn->tag == CONST_INT ? vint(cn->i) : vdbl(cn->d));
            break;
        }
        case OP_PUSHI:  PUSH(vint(rd_i32(&c))); break;
        case OP_LOADL:  PUSH(locals[rd_u16(&c)]); break;
        case OP_STOREL: { unsigned s = rd_u16(&c); locals[s] = POP(); break; }
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        case OP_LT: case OP_GT: case OP_LE: case OP_GE:
        case OP_EQ: case OP_NE: {
            Val b = POP(), a = POP();
            PUSH(do_binop(op, a, b));
            break;
        }
        case OP_NEG: {
            Val a = POP();
            PUSH(a.t == V_INT ? vint(-a.u.i) : vdbl(-a.u.d));
            break;
        }
        case OP_I2D: { Val a = POP(); PUSH(vdbl((double)a.u.i)); break; }
        case OP_D2I: { Val a = POP(); PUSH(vint((long)a.u.d)); break; }
        case OP_JMP: { long off = rd_i32(&c); c.p += off; break; }
        case OP_JZ:  { long off = rd_i32(&c); Val v = POP();
                       if ((v.t == V_INT ? v.u.i : (long)v.u.d) == 0) c.p += off; break; }
        case OP_CALL: {
            long callee = rd_u16(&c);
            int  nargc  = *c.p++;
            Val  cargs[64];
            for (int i = nargc - 1; i >= 0; i--) cargs[i] = POP();
            PUSH(run_func(callee, cargs, nargc));
            break;
        }
        case OP_RET:  ret = POP(); goto done;
        case OP_RETV: ret = vint(0); goto done;
        case OP_NEWARR: {
            Val len = POP();
            long n = len.t == V_INT ? len.u.i : (long)len.u.d;
            if (n < 0) die("negative array length");
            Arr *a = malloc(sizeof(Arr));
            a->len = n;
            a->data = calloc(n > 0 ? n : 1, sizeof(Val));
            Val v; v.t = V_REF; v.u.ref = a;
            PUSH(v);
            break;
        }
        case OP_ALOAD: {
            Val idx = POP(), ref = POP();
            if (ref.t != V_REF) die("index of non-array");
            long i = idx.t == V_INT ? idx.u.i : (long)idx.u.d;
            if (i < 0 || i >= ref.u.ref->len) die("array index out of bounds");
            PUSH(ref.u.ref->data[i]);
            break;
        }
        case OP_ASTORE: {
            Val val = POP(), idx = POP(), ref = POP();
            if (ref.t != V_REF) die("index of non-array");
            long i = idx.t == V_INT ? idx.u.i : (long)idx.u.d;
            if (i < 0 || i >= ref.u.ref->len) die("array index out of bounds");
            ref.u.ref->data[i] = val;
            break;
        }
        case OP_PRINT: {
            Val v = POP();
            if (v.t == V_INT) printf("%ld\n", v.u.i);
            else              printf("%g\n", v.u.d);
            break;
        }
        case OP_POP: (void)POP(); break;
        case OP_HALT: goto done;
        default: die("bad opcode");
        }
    }
done:
    free(locals);
    return ret;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s out.bvm\n", argv[0]); return 2; }
    P = load_prog(argv[1]);
    if (P.entry < 0 || P.entry >= P.nfuncs) die("no main function");
    run_func(P.entry, NULL, 0);
    return 0;
}
