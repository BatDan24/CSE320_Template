/* emit.c — instruction list -> .bvm binary; owns the constant pool.
 *
 * PROVIDED, CORRECT, READ-ONLY FOR STUDENTS.
 * This file is checksummed by the autograder; do not modify it.
 *
 * Two passes per function: pass 1 assigns a byte offset to every instruction,
 * pass 2 serialises, converting each jump's target instruction index into a
 * signed byte offset relative to the instruction that follows the jump.
 */
#include "svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- constant pool ----- */

int const_intern_int(Module *m, long v) {
    for (int i = 0; i < m->nconsts; i++)
        if (m->consts[i].tag == CONST_INT && m->consts[i].i == v) return i;
    if (m->nconsts == m->ccap) {
        m->ccap = m->ccap ? m->ccap * 2 : 16;
        m->consts = realloc(m->consts, m->ccap * sizeof(Const));
    }
    m->consts[m->nconsts].tag = CONST_INT;
    m->consts[m->nconsts].i   = v;
    m->consts[m->nconsts].d   = 0;
    return m->nconsts++;
}

int const_intern_double(Module *m, double v) {
    for (int i = 0; i < m->nconsts; i++)
        if (m->consts[i].tag == CONST_DOUBLE && m->consts[i].d == v) return i;
    if (m->nconsts == m->ccap) {
        m->ccap = m->ccap ? m->ccap * 2 : 16;
        m->consts = realloc(m->consts, m->ccap * sizeof(Const));
    }
    m->consts[m->nconsts].tag = CONST_DOUBLE;
    m->consts[m->nconsts].i   = 0;
    m->consts[m->nconsts].d   = v;
    return m->nconsts++;
}

/* ----- little-endian writers ----- */

static void put_u16(FILE *fp, unsigned v) {
    fputc(v & 0xff, fp);
    fputc((v >> 8) & 0xff, fp);
}
static void put_u32(FILE *fp, unsigned long v) {
    for (int i = 0; i < 4; i++) fputc((v >> (8 * i)) & 0xff, fp);
}
static void put_i32(FILE *fp, long v) { put_u32(fp, (unsigned long)(int)v); }
static void put_i64(FILE *fp, long v) {
    for (int i = 0; i < 8; i++) fputc(((unsigned long)v >> (8 * i)) & 0xff, fp);
}
static void put_f64(FILE *fp, double d) {
    long bits; memcpy(&bits, &d, 8); put_i64(fp, bits);
}

/* ----- instruction sizing ----- */

static int operand_bytes(int op) {
    switch (op) {
    case OP_PUSHC: case OP_PUSHI: case OP_JMP: case OP_JZ: return 4;
    case OP_LOADL: case OP_STOREL:                         return 2;
    case OP_CALL:                                          return 3;
    default:                                               return 0;
    }
}
static int instr_bytes(int op) { return 1 + operand_bytes(op); }

/* ----- serialise one function's code, returns malloc'd buffer + length ----- */

static unsigned char *encode_func(Func *f, size_t *out_len) {
    int *off = malloc(f->ncode * sizeof(int));
    int total = 0;
    for (int i = 0; i < f->ncode; i++) { off[i] = total; total += instr_bytes(f->code[i].op); }

    /* write into a memory FILE so we reuse the LE helpers */
    unsigned char *buf = malloc(total > 0 ? total : 1);
    FILE *fp = fmemopen(buf, total > 0 ? total : 1, "wb");

    for (int i = 0; i < f->ncode; i++) {
        Instr *in = &f->code[i];
        fputc(in->op, fp);
        switch (in->op) {
        case OP_PUSHC: put_u32(fp, (unsigned long)in->arg); break;
        case OP_PUSHI: put_i32(fp, in->arg); break;
        case OP_LOADL: case OP_STOREL: put_u16(fp, (unsigned)in->arg); break;
        case OP_CALL:  put_u16(fp, (unsigned)in->arg); fputc(in->arg2 & 0xff, fp); break;
        case OP_JMP: case OP_JZ: {
            int target = (int)in->arg;                 /* instruction index */
            int here_next = off[i] + instr_bytes(in->op);
            put_i32(fp, off[target] - here_next);
            break;
        }
        default: break;
        }
    }
    fclose(fp);
    free(off);
    *out_len = (size_t)total;
    return buf;
}

int emit_module(Module *m, const char *out_path) {
    FILE *fp = fopen(out_path, "wb");
    if (!fp) return -1;

    fputc(BVM_MAGIC0, fp); fputc(BVM_MAGIC1, fp);
    fputc(BVM_MAGIC2, fp); fputc(BVM_MAGIC3, fp);

    put_u32(fp, (unsigned long)m->nconsts);
    for (int i = 0; i < m->nconsts; i++) {
        fputc(m->consts[i].tag, fp);
        for (int k = 0; k < 7; k++) fputc(0, fp);        /* pad to 8-byte value */
        if (m->consts[i].tag == CONST_INT) put_i64(fp, m->consts[i].i);
        else                               put_f64(fp, m->consts[i].d);
    }

    put_u32(fp, (unsigned long)m->nfuncs);
    put_u32(fp, (unsigned long)(m->entry < 0 ? 0 : m->entry));

    for (int i = 0; i < m->nfuncs; i++) {
        Func *f = &m->funcs[i];
        size_t clen;
        unsigned char *code = encode_func(f, &clen);

        size_t nlen = strlen(f->name);
        put_u16(fp, (unsigned)nlen);
        fwrite(f->name, 1, nlen, fp);
        put_u16(fp, (unsigned)f->nparams);
        put_u16(fp, (unsigned)f->nlocals);
        put_u32(fp, (unsigned long)clen);
        fwrite(code, 1, clen, fp);
        free(code);
    }

    fclose(fp);
    return 0;
}
