/* parser.c — Token stream -> AST, with name resolution and typing.
 *
 * Student-editable module.
 *
 * Tokens are pulled from the lexer on demand; cur/peek cache the two live
 * tokens for fast lookahead.
 */
#include "svc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ----- node construction ----- */

static AstNode *new_node(NodeKind k) {
    AstNode *n = calloc(1, sizeof *n);
    n->kind = k;
    return n;
}

static void list_push(AstNode *n, AstNode *child) {
    n->list = realloc(n->list, (n->nlist + 1) * sizeof(AstNode *));
    n->list[n->nlist++] = child;
}

/* ----- token cursor ----- */

static void advance(Parser *P) {
    P->cur  = P->peek;
    P->peek = lexer_next(P->lx);       /* cache the freshly-lexed token */
}

static int check(Parser *P, TokKind k)  { return P->cur->kind == k; }

static int accept(Parser *P, TokKind k) {
    if (check(P, k)) { advance(P); return 1; }
    return 0;
}

static void perr(Parser *P, const char *msg) {
    if (!P->had_error)
        fprintf(stderr, "%s:%d: parse error: %s (got token %d)\n",
                P->fname, P->cur->line, msg, P->cur->kind);
    P->had_error = 1;
}

static void expect(Parser *P, TokKind k, const char *msg) {
    if (!accept(P, k)) perr(P, msg);
}

/* ----- types ----- */

static Type tok_to_type(TokKind k) {
    if (k == T_KW_INT)    return TY_INT;
    if (k == T_KW_DOUBLE) return TY_DOUBLE;
    return TY_VOID;
}

static Type parse_scalar_type(Parser *P) {
    Type t = tok_to_type(P->cur->kind);
    if (t == TY_VOID) { perr(P, "expected type"); return TY_INT; }
    advance(P);
    return t;
}

/* wrap `e` in a conversion so its value has type `target` */
static AstNode *coerce(AstNode *e, Type target) {
    if (!e || e->type == target) return e;
    if (e->type == TY_INT && target == TY_DOUBLE) {
        AstNode *c = new_node(N_I2D); c->a = e; c->type = TY_DOUBLE; return c;
    }
    if (e->type == TY_DOUBLE && target == TY_INT) {
        AstNode *c = new_node(N_D2I); c->a = e; c->type = TY_INT; return c;
    }
    return e; /* array/void: no conversion */
}

/* ----- function signature pre-scan ----- */

static FuncSig *find_sig(Parser *P, const char *name, int *idx_out) {
    /* sig names are strdup'd in prescan (a separate intern table), so compare
     * by content rather than by interned-pointer identity. */
    for (int i = 0; i < P->nsigs; i++)
        if (strcmp(P->sigs[i].name, name) == 0) { if (idx_out) *idx_out = i; return &P->sigs[i]; }
    return NULL;
}

/* One throwaway lexer pass to collect top-level function headers so that
 * forward calls resolve. */
static void prescan(Parser *P, const char *src, size_t len) {
    Lexer lx;
    lexer_init(&lx, src, len);
    Token *t = lexer_next(&lx);
    while (t->kind != T_EOF) {
        Type ret = tok_to_type(t->kind);
        if (ret != TY_VOID) {
            Token *nm = lexer_next(&lx);
            TokKind nmkind = nm->kind;
            const char *fname = nm->sval;
            Token *lp = lexer_next(&lx);
            if (nmkind == T_IDENT && lp->kind == T_LPAREN) {
                Type params[32]; int np = 0;
                Token *p = lexer_next(&lx);
                while (p->kind != T_RPAREN && p->kind != T_EOF) {
                    Type pt = tok_to_type(p->kind);
                    if (pt != TY_VOID && np < 32) {
                        Token *pn = lexer_next(&lx);       /* param name */
                        if (pn->kind == T_LBRACKET) {      /* array param */
                            lexer_next(&lx);               /* ']' */
                            pt = (pt == TY_INT) ? TY_ARR_INT : TY_ARR_DOUBLE;
                        }
                        params[np++] = pt;
                    }
                    p = lexer_next(&lx);
                    if (p->kind == T_COMMA) p = lexer_next(&lx);
                }
                P->sigs = realloc(P->sigs, (P->nsigs + 1) * sizeof(FuncSig));
                FuncSig *s = &P->sigs[P->nsigs++];
                s->name = strdup(fname);   /* survives lexer_free below */
                s->ret = ret;
                s->nparams = np;
                s->params = malloc(np * sizeof(Type));
                memcpy(s->params, params, np * sizeof(Type));
            }
        }
        t = lexer_next(&lx);
    }
    lexer_free(&lx);
}

/* ----- expressions ----- */

static AstNode *parse_expr(Parser *P);

static AstNode *parse_primary(Parser *P) {
    Token *t = P->cur;
    if (t->kind == T_INT_LIT) {
        AstNode *n = new_node(N_INTLIT); n->ival = t->ival; n->type = TY_INT;
        advance(P); return n;
    }
    if (t->kind == T_DBL_LIT) {
        AstNode *n = new_node(N_DBLLIT); n->dval = t->dval; n->type = TY_DOUBLE;
        advance(P); return n;
    }
    if (t->kind == T_LPAREN) {
        advance(P);
        AstNode *e = parse_expr(P);
        expect(P, T_RPAREN, "expected ')'");
        return e;
    }
    if (t->kind == T_IDENT) {
        const char *name = t->sval;
        advance(P);
        if (check(P, T_LPAREN)) {                 /* call */
            advance(P);
            int fidx = -1;
            FuncSig *sig = find_sig(P, name, &fidx);
            AstNode *call = new_node(N_CALL);
            call->name = name;
            call->func_index = fidx;
            call->type = sig ? sig->ret : TY_INT;
            int argi = 0;
            while (!check(P, T_RPAREN) && !check(P, T_EOF)) {
                AstNode *arg = parse_expr(P);
                if (sig && argi < sig->nparams)
                    arg = coerce(arg, sig->params[argi]);
                list_push(call, arg);
                argi++;
                if (!accept(P, T_COMMA)) break;
            }
            expect(P, T_RPAREN, "expected ')' after arguments");
            if (!sig) perr(P, "call to undeclared function");
            return call;
        }
        /* variable, possibly indexed */
        SymSlot *sym = symtab_lookup(&P->sym, name);
        if (!sym) { perr(P, "use of undeclared variable"); }
        AstNode *var = new_node(N_VAR);
        var->name = name;
        var->slot = sym ? sym->slot : 0;
        var->type = sym ? sym->type : TY_INT;
        if (check(P, T_LBRACKET)) {               /* array index */
            advance(P);
            AstNode *idx = coerce(parse_expr(P), TY_INT);
            expect(P, T_RBRACKET, "expected ']'");
            AstNode *ix = new_node(N_INDEX);
            ix->a = var; ix->b = idx;
            ix->type = elem_type(var->type);
            return ix;
        }
        return var;
    }
    perr(P, "expected expression");
    AstNode *n = new_node(N_INTLIT); n->type = TY_INT;
    advance(P);
    return n;
}

static AstNode *parse_unary(Parser *P) {
    if (check(P, T_MINUS)) {
        advance(P);
        AstNode *e = parse_unary(P);
        AstNode *n = new_node(N_UNOP);
        n->op = T_MINUS; n->a = e; n->type = e->type;
        return n;
    }
    return parse_primary(P);
}

static AstNode *make_binop(TokKind op, AstNode *a, AstNode *b) {
    int is_cmp = (op == T_LT || op == T_GT || op == T_LE ||
                  op == T_GE || op == T_EQ || op == T_NE);
    Type common = (a->type == TY_DOUBLE || b->type == TY_DOUBLE)
                  ? TY_DOUBLE : TY_INT;
    a = coerce(a, common);
    b = coerce(b, common);
    AstNode *n = new_node(N_BINOP);
    n->op = op; n->a = a; n->b = b;
    n->type = is_cmp ? TY_INT : common;
    return n;
}

static AstNode *parse_mul(Parser *P) {
    AstNode *a = parse_unary(P);
    while (check(P, T_STAR) || check(P, T_SLASH)) {
        TokKind op = P->cur->kind; advance(P);
        a = make_binop(op, a, parse_unary(P));
    }
    return a;
}

static AstNode *parse_add(Parser *P) {
    AstNode *a = parse_mul(P);
    while (check(P, T_PLUS) || check(P, T_MINUS)) {
        TokKind op = P->cur->kind; advance(P);
        a = make_binop(op, a, parse_mul(P));
    }
    return a;
}

static AstNode *parse_expr(Parser *P) {   /* comparison, lowest precedence */
    AstNode *a = parse_add(P);
    while (check(P, T_LT) || check(P, T_GT) || check(P, T_LE) ||
           check(P, T_GE) || check(P, T_EQ) || check(P, T_NE)) {
        TokKind op = P->cur->kind; advance(P);
        a = make_binop(op, a, parse_add(P));
    }
    return a;
}

/* ----- statements ----- */

static AstNode *parse_block(Parser *P);

static int is_type_kw(TokKind k) { return k == T_KW_INT || k == T_KW_DOUBLE; }

static AstNode *parse_decl(Parser *P) {
    Type base = parse_scalar_type(P);
    if (!check(P, T_IDENT)) { perr(P, "expected declarator name"); }
    const char *name = P->cur->sval;
    advance(P);

    AstNode *d = new_node(N_DECL);
    d->name = name;

    if (accept(P, T_LBRACKET)) {                  /* array declaration */
        AstNode *size = coerce(parse_expr(P), TY_INT);
        expect(P, T_RBRACKET, "expected ']'");
        d->type = (base == TY_INT) ? TY_ARR_INT : TY_ARR_DOUBLE;
        d->b = size;
    } else {
        d->type = base;
        if (accept(P, T_ASSIGN))
            d->a = coerce(parse_expr(P), base);
    }
    d->slot = P->nlocals++;
    symtab_insert(&P->sym, name, d->type, d->slot);
    expect(P, T_SEMI, "expected ';' after declaration");
    return d;
}

static AstNode *parse_lvalue_stmt(Parser *P) {
    /* IDENT ('[' expr ']')? '=' expr ';'  OR  expr ';' (call) */
    const char *name = P->cur->sval;
    if (P->peek->kind == T_LPAREN) {              /* expression statement (call) */
        AstNode *e = parse_expr(P);
        AstNode *s = new_node(N_EXPRSTMT); s->a = e;
        expect(P, T_SEMI, "expected ';'");
        return s;
    }
    SymSlot *sym = symtab_lookup(&P->sym, name);
    if (!sym) perr(P, "assignment to undeclared variable");
    advance(P);                                   /* consume IDENT */

    AstNode *asn = new_node(N_ASSIGN);
    AstNode *var = new_node(N_VAR);
    var->name = name;
    var->slot = sym ? sym->slot : 0;
    var->type = sym ? sym->type : TY_INT;

    if (accept(P, T_LBRACKET)) {                  /* indexed store */
        AstNode *idx = coerce(parse_expr(P), TY_INT);
        expect(P, T_RBRACKET, "expected ']'");
        AstNode *ix = new_node(N_INDEX);
        ix->a = var; ix->b = idx;
        ix->type = elem_type(var->type);
        asn->a = ix;
        expect(P, T_ASSIGN, "expected '='");
        asn->b = coerce(parse_expr(P), ix->type);
    } else {
        asn->a = var;
        expect(P, T_ASSIGN, "expected '='");
        asn->b = coerce(parse_expr(P), var->type);
    }
    expect(P, T_SEMI, "expected ';' after assignment");
    return asn;
}

static AstNode *parse_stmt(Parser *P) {
    if (check(P, T_LBRACE))   return parse_block(P);
    if (is_type_kw(P->cur->kind)) return parse_decl(P);

    if (accept(P, T_KW_IF)) {
        AstNode *n = new_node(N_IF);
        expect(P, T_LPAREN, "expected '(' after if");
        n->a = coerce(parse_expr(P), TY_INT);
        expect(P, T_RPAREN, "expected ')'");
        n->b = parse_stmt(P);
        if (accept(P, T_KW_ELSE)) n->c = parse_stmt(P);
        return n;
    }
    if (accept(P, T_KW_WHILE)) {
        AstNode *n = new_node(N_WHILE);
        expect(P, T_LPAREN, "expected '(' after while");
        n->a = coerce(parse_expr(P), TY_INT);
        expect(P, T_RPAREN, "expected ')'");
        n->b = parse_stmt(P);
        return n;
    }
    if (accept(P, T_KW_RETURN)) {
        AstNode *n = new_node(N_RETURN);
        if (!check(P, T_SEMI)) n->a = parse_expr(P);   /* coerced in parse_func */
        expect(P, T_SEMI, "expected ';' after return");
        return n;
    }
    if (accept(P, T_KW_PRINT)) {
        AstNode *n = new_node(N_PRINT);
        expect(P, T_LPAREN, "expected '(' after print");
        n->a = parse_expr(P);
        expect(P, T_RPAREN, "expected ')'");
        expect(P, T_SEMI, "expected ';'");
        return n;
    }
    if (check(P, T_IDENT)) return parse_lvalue_stmt(P);

    perr(P, "expected statement");
    advance(P);
    return new_node(N_BLOCK);
}

static AstNode *parse_block(Parser *P) {
    expect(P, T_LBRACE, "expected '{'");
    AstNode *blk = new_node(N_BLOCK);
    scope_push(&P->sym);
    while (!check(P, T_RBRACE) && !check(P, T_EOF) && !P->had_error)
        list_push(blk, parse_stmt(P));
    scope_pop(&P->sym);
    expect(P, T_RBRACE, "expected '}'");
    return blk;
}

/* fix up return-value coercions now that we know the function's return type */
static void coerce_returns(AstNode *n, Type ret) {
    if (!n) return;
    if (n->kind == N_FUNC) return;      /* don't descend into nested (none) */
    if (n->kind == N_RETURN && n->a)
        n->a = coerce(n->a, ret);
    coerce_returns(n->a, ret);
    coerce_returns(n->b, ret);
    coerce_returns(n->c, ret);
    for (int i = 0; i < n->nlist; i++) coerce_returns(n->list[i], ret);
}

static AstNode *parse_func(Parser *P) {
    Type ret = parse_scalar_type(P);
    if (!check(P, T_IDENT)) { perr(P, "expected function name"); return NULL; }
    const char *name = P->cur->sval;
    advance(P);
    expect(P, T_LPAREN, "expected '('");

    AstNode *fn = new_node(N_FUNC);
    /* strdup: this name is read by codegen after the lexer (and its intern
     * table) has been torn down, so the AST must own its own copy. */
    fn->name = strdup(name);
    fn->type = ret;

    P->nlocals = 0;
    symtab_init(&P->sym);

    while (!check(P, T_RPAREN) && !check(P, T_EOF)) {
        Type pt = parse_scalar_type(P);
        const char *pname = P->cur->sval;
        expect(P, T_IDENT, "expected parameter name");
        if (accept(P, T_LBRACKET)) {              /* array parameter */
            expect(P, T_RBRACKET, "expected ']'");
            pt = (pt == TY_INT) ? TY_ARR_INT : TY_ARR_DOUBLE;
        }
        AstNode *param = new_node(N_PARAM);
        param->name = pname;
        param->type = pt;
        param->slot = P->nlocals++;
        symtab_insert(&P->sym, pname, pt, param->slot);
        list_push(fn, param);            /* params stored first in list */
        fn->ival++;                      /* ival = param count */
        if (!accept(P, T_COMMA)) break;
    }
    expect(P, T_RPAREN, "expected ')'");

    AstNode *body = parse_block(P);
    coerce_returns(body, ret);
    fn->a = body;
    fn->dval = 0;
    fn->func_index = P->nlocals;         /* stash local count in func_index */

    symtab_free(&P->sym);
    return fn;
}

/* ----- entry ----- */

AstNode *parse_program(const char *src, size_t len, const char *fname) {
    Parser P;
    memset(&P, 0, sizeof P);
    P.fname = fname ? fname : "<input>";

    prescan(&P, src, len);

    Lexer lx;
    lexer_init(&lx, src, len);
    P.lx = &lx;
    /* prime cur/peek */
    P.cur  = lexer_next(&lx);
    P.peek = lexer_next(&lx);

    AstNode *prog = new_node(N_BLOCK);   /* program container */
    while (!check(&P, T_EOF) && !P.had_error)
        list_push(prog, parse_func(&P));

    int had_error = P.had_error;

    lexer_free(&lx);
    for (int i = 0; i < P.nsigs; i++) { free(P.sigs[i].params); free((char *)P.sigs[i].name); }
    free(P.sigs);

    if (had_error) return NULL;
    return prog;
}
