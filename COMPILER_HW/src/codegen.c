/* codegen.c — AST -> instruction list.
 *
 * PROVIDED, CORRECT, READ-ONLY FOR STUDENTS.
 * This file is checksummed by the autograder; do not modify it.
 *
 * Emits a flat Instr array per function.  Jumps carry the *instruction index*
 * of their target; emit.c resolves those to relative byte offsets.  All static
 * type decisions (int/double promotion, truncation) were made in the parser
 * and appear as explicit N_I2D / N_D2I nodes, so this pass is purely
 * structural.
 */
#include "svc.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int emit_i(Func *f, int op, long arg, int arg2) {
    if (f->ncode == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 32;
        f->code = realloc(f->code, f->cap * sizeof(Instr));
    }
    f->code[f->ncode].op   = op;
    f->code[f->ncode].arg  = arg;
    f->code[f->ncode].arg2 = arg2;
    return f->ncode++;
}
#define EMIT0(f,op)      emit_i((f),(op),0,0)
#define EMIT1(f,op,a)    emit_i((f),(op),(a),0)

static int binop_opcode(TokKind op) {
    switch (op) {
        case T_PLUS:  return OP_ADD;
        case T_MINUS: return OP_SUB;
        case T_STAR:  return OP_MUL;
        case T_SLASH: return OP_DIV;
        case T_LT:    return OP_LT;
        case T_GT:    return OP_GT;
        case T_LE:    return OP_LE;
        case T_GE:    return OP_GE;
        case T_EQ:    return OP_EQ;
        case T_NE:    return OP_NE;
        default:      return OP_HALT;   /* unreachable */
    }
}

static void gen_expr(Module *m, Func *f, AstNode *n);

static void push_int(Module *m, Func *f, long v) {
    if (v >= INT_MIN && v <= INT_MAX)
        EMIT1(f, OP_PUSHI, v);
    else
        EMIT1(f, OP_PUSHC, const_intern_int(m, v));
}

static void gen_expr(Module *m, Func *f, AstNode *n) {
    switch (n->kind) {
    case N_INTLIT: push_int(m, f, n->ival); break;
    case N_DBLLIT: EMIT1(f, OP_PUSHC, const_intern_double(m, n->dval)); break;
    case N_VAR:    EMIT1(f, OP_LOADL, n->slot); break;
    case N_INDEX:
        gen_expr(m, f, n->a);           /* array ref */
        gen_expr(m, f, n->b);           /* index     */
        EMIT0(f, OP_ALOAD);
        break;
    case N_I2D: gen_expr(m, f, n->a); EMIT0(f, OP_I2D); break;
    case N_D2I: gen_expr(m, f, n->a); EMIT0(f, OP_D2I); break;
    case N_UNOP:
        gen_expr(m, f, n->a);
        EMIT0(f, OP_NEG);
        break;
    case N_BINOP:
        gen_expr(m, f, n->a);
        gen_expr(m, f, n->b);
        EMIT0(f, binop_opcode(n->op));
        break;
    case N_CALL:
        for (int i = 0; i < n->nlist; i++)
            gen_expr(m, f, n->list[i]);
        emit_i(f, OP_CALL, n->func_index, n->nlist);
        break;
    default: break;
    }
}

static void gen_stmt(Module *m, Func *f, AstNode *n) {
    if (!n) return;
    switch (n->kind) {
    case N_BLOCK:
        for (int i = 0; i < n->nlist; i++) gen_stmt(m, f, n->list[i]);
        break;
    case N_DECL:
        if (n->type == TY_ARR_INT || n->type == TY_ARR_DOUBLE) {
            gen_expr(m, f, n->b);              /* length */
            EMIT0(f, OP_NEWARR);
            EMIT1(f, OP_STOREL, n->slot);
        } else {
            if (n->a) gen_expr(m, f, n->a);
            else if (n->type == TY_DOUBLE)
                EMIT1(f, OP_PUSHC, const_intern_double(m, 0.0));
            else
                EMIT1(f, OP_PUSHI, 0);
            EMIT1(f, OP_STOREL, n->slot);
        }
        break;
    case N_ASSIGN:
        if (n->a->kind == N_INDEX) {
            gen_expr(m, f, n->a->a);          /* array ref */
            gen_expr(m, f, n->a->b);          /* index     */
            gen_expr(m, f, n->b);             /* value     */
            EMIT0(f, OP_ASTORE);
        } else {
            gen_expr(m, f, n->b);
            EMIT1(f, OP_STOREL, n->a->slot);
        }
        break;
    case N_IF: {
        gen_expr(m, f, n->a);
        int jz = EMIT1(f, OP_JZ, 0);
        gen_stmt(m, f, n->b);
        if (n->c) {
            int jmp = EMIT1(f, OP_JMP, 0);
            f->code[jz].arg = f->ncode;       /* else target */
            gen_stmt(m, f, n->c);
            f->code[jmp].arg = f->ncode;      /* end target  */
        } else {
            f->code[jz].arg = f->ncode;
        }
        break;
    }
    case N_WHILE: {
        int top = f->ncode;
        gen_expr(m, f, n->a);
        int jz = EMIT1(f, OP_JZ, 0);
        gen_stmt(m, f, n->b);
        EMIT1(f, OP_JMP, top);
        f->code[jz].arg = f->ncode;
        break;
    }
    case N_RETURN:
        if (n->a) { gen_expr(m, f, n->a); EMIT0(f, OP_RET); }
        else      { EMIT1(f, OP_PUSHI, 0); EMIT0(f, OP_RET); }
        break;
    case N_PRINT:
        gen_expr(m, f, n->a);
        EMIT0(f, OP_PRINT);
        break;
    case N_EXPRSTMT:
        gen_expr(m, f, n->a);
        EMIT0(f, OP_POP);                     /* discard the call's result */
        break;
    default: break;
    }
}

Module *codegen(AstNode *program) {
    Module *m = calloc(1, sizeof *m);
    m->entry = -1;

    for (int i = 0; i < program->nlist; i++) {
        AstNode *fn = program->list[i];
        if (m->nfuncs == m->fcap) {
            m->fcap = m->fcap ? m->fcap * 2 : 8;
            m->funcs = realloc(m->funcs, m->fcap * sizeof(Func));
        }
        Func *f = &m->funcs[m->nfuncs];
        memset(f, 0, sizeof *f);
        strncpy(f->name, fn->name, sizeof(f->name) - 1);
        f->nparams = fn->nlist;               /* params live in fn->list */
        f->nlocals = fn->func_index;          /* stashed local count     */

        gen_stmt(m, f, fn->a);                /* body */
        EMIT1(f, OP_PUSHI, 0);                /* fall-through safety net */
        EMIT0(f, OP_RET);

        if (strcmp(f->name, "main") == 0) m->entry = m->nfuncs;
        m->nfuncs++;
    }
    return m;
}

void module_free(Module *m) {
    if (!m) return;
    for (int i = 0; i < m->nfuncs; i++) free(m->funcs[i].code);
    free(m->funcs);
    free(m->consts);
    free(m);
}
