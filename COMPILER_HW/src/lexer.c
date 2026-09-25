/* lexer.c — bytes -> Token[], with string interning.
 *
 * Student-editable module.
 *
 * The token array grows on demand as the parser pulls tokens.
 */
#include "svc.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static unsigned long hash_str(const char *s, size_t n) {
    unsigned long h = 1469598103934665603UL;
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned char)s[i];
        h *= 1099511628211UL;
    }
    return h;
}

const char *intern(Lexer *lx, const char *s, size_t n) {
    unsigned long b = hash_str(s, n) % INTERN_BUCKETS;
    for (InternEntry *e = lx->intern[b]; e; e = e->next) {
        if (strlen(e->str) == n && memcmp(e->str, s, n) == 0)
            return e->str;
    }
    InternEntry *e = malloc(sizeof *e);
    e->str = malloc(n + 1);
    memcpy(e->str, s, n);
    e->str[n] = '\0';
    e->next = lx->intern[b];
    lx->intern[b] = e;
    return e->str;
}

void lexer_init(Lexer *lx, const char *src, size_t len) {
    memset(lx, 0, sizeof *lx);
    lx->src = src;
    lx->len = len;
    lx->pos = 0;
    lx->line = 1;
    lx->cap = 256;                       /* grows by doubling */
    lx->toks = malloc(lx->cap * sizeof(Token));
    lx->ntok = 0;
}

void lexer_free(Lexer *lx) {
    free(lx->toks);
    for (int i = 0; i < INTERN_BUCKETS; i++) {
        InternEntry *e = lx->intern[i];
        while (e) {
            InternEntry *n = e->next;
            free(e->str);
            free(e);
            e = n;
        }
    }
}

static Token *push_tok(Lexer *lx, Token t) {
    if (lx->ntok == lx->cap) {
        lx->cap *= 2;
        lx->toks = realloc(lx->toks, lx->cap * sizeof(Token));
    }
    lx->toks[lx->ntok] = t;
    return &lx->toks[lx->ntok++];
}

static int match_kw(const char *s, size_t n, TokKind *out) {
    struct { const char *w; TokKind k; } kws[] = {
        {"int", T_KW_INT}, {"double", T_KW_DOUBLE}, {"if", T_KW_IF},
        {"else", T_KW_ELSE}, {"while", T_KW_WHILE}, {"return", T_KW_RETURN},
        {"print", T_KW_PRINT},
    };
    for (size_t i = 0; i < sizeof(kws)/sizeof(kws[0]); i++) {
        if (strlen(kws[i].w) == n && memcmp(kws[i].w, s, n) == 0) {
            *out = kws[i].k;
            return 1;
        }
    }
    return 0;
}

Token *lexer_next(Lexer *lx) {
    const char *s = lx->src;
    /* skip whitespace and // comments */
    for (;;) {
        while (lx->pos < lx->len && isspace((unsigned char)s[lx->pos])) {
            if (s[lx->pos] == '\n') lx->line++;
            lx->pos++;
        }
        if (lx->pos + 1 < lx->len && s[lx->pos] == '/' && s[lx->pos+1] == '/') {
            while (lx->pos < lx->len && s[lx->pos] != '\n') lx->pos++;
            continue;
        }
        break;
    }

    Token t;
    memset(&t, 0, sizeof t);
    t.line = lx->line;

    if (lx->pos >= lx->len) {
        t.kind = T_EOF;
        return push_tok(lx, t);
    }

    char c = s[lx->pos];

    /* identifier / keyword */
    if (isalpha((unsigned char)c) || c == '_') {
        size_t start = lx->pos;
        while (lx->pos < lx->len &&
               (isalnum((unsigned char)s[lx->pos]) || s[lx->pos] == '_'))
            lx->pos++;
        size_t n = lx->pos - start;
        TokKind kw;
        if (match_kw(s + start, n, &kw)) {
            t.kind = kw;
        } else {
            t.kind = T_IDENT;
            t.sval = intern(lx, s + start, n);
        }
        return push_tok(lx, t);
    }

    /* number */
    if (isdigit((unsigned char)c)) {
        size_t start = lx->pos;
        int is_dbl = 0;
        while (lx->pos < lx->len && isdigit((unsigned char)s[lx->pos])) lx->pos++;
        if (lx->pos < lx->len && s[lx->pos] == '.') {
            is_dbl = 1;
            lx->pos++;
            while (lx->pos < lx->len && isdigit((unsigned char)s[lx->pos])) lx->pos++;
        }
        char buf[64];
        size_t n = lx->pos - start;
        if (n >= sizeof buf) n = sizeof buf - 1;
        memcpy(buf, s + start, n);
        buf[n] = '\0';
        if (is_dbl) { t.kind = T_DBL_LIT; t.dval = strtod(buf, NULL); }
        else        { t.kind = T_INT_LIT; t.ival = strtol(buf, NULL, 10); }
        return push_tok(lx, t);
    }

    /* operators / punctuation */
    lx->pos++;
    switch (c) {
        case '(': t.kind = T_LPAREN; break;
        case ')': t.kind = T_RPAREN; break;
        case '{': t.kind = T_LBRACE; break;
        case '}': t.kind = T_RBRACE; break;
        case '[': t.kind = T_LBRACKET; break;
        case ']': t.kind = T_RBRACKET; break;
        case ';': t.kind = T_SEMI; break;
        case ',': t.kind = T_COMMA; break;
        case '+': t.kind = T_PLUS; break;
        case '-': t.kind = T_MINUS; break;
        case '*': t.kind = T_STAR; break;
        case '/': t.kind = T_SLASH; break;
        case '<':
            if (lx->pos < lx->len && s[lx->pos] == '=') { lx->pos++; t.kind = T_LE; }
            else t.kind = T_LT;
            break;
        case '>':
            if (lx->pos < lx->len && s[lx->pos] == '=') { lx->pos++; t.kind = T_GE; }
            else t.kind = T_GT;
            break;
        case '=':
            if (lx->pos < lx->len && s[lx->pos] == '=') { lx->pos++; t.kind = T_EQ; }
            else t.kind = T_ASSIGN;
            break;
        case '!':
            if (lx->pos < lx->len && s[lx->pos] == '=') { lx->pos++; t.kind = T_NE; }
            else { fprintf(stderr, "lex error: stray '!' at line %d\n", lx->line); t.kind = T_EOF; }
            break;
        default:
            fprintf(stderr, "lex error: unexpected '%c' at line %d\n", c, lx->line);
            t.kind = T_EOF;
            break;
    }
    return push_tok(lx, t);
}
