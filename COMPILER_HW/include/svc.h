#ifndef SVC_H
#define SVC_H

#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include "opcodes.h"

/* ------------------------------------------------------------------ */
/* Source types                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    TY_VOID,
    TY_INT,
    TY_DOUBLE,
    TY_ARR_INT,
    TY_ARR_DOUBLE
} Type;

/* element type of an array type, or the type itself if scalar */
static inline Type elem_type(Type t) {
    if (t == TY_ARR_INT)    return TY_INT;
    if (t == TY_ARR_DOUBLE) return TY_DOUBLE;
    return t;
}
static inline int is_double_ty(Type t) { return t == TY_DOUBLE; }

/* ------------------------------------------------------------------ */
/* Lexer                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    T_EOF,
    T_INT_LIT, T_DBL_LIT, T_IDENT,
    T_KW_INT, T_KW_DOUBLE, T_KW_IF, T_KW_ELSE, T_KW_WHILE,
    T_KW_RETURN, T_KW_PRINT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_LBRACKET, T_RBRACKET,
    T_SEMI, T_COMMA,
    T_ASSIGN, T_PLUS, T_MINUS, T_STAR, T_SLASH,
    T_LT, T_GT, T_LE, T_GE, T_EQ, T_NE
} TokKind;

typedef struct {
    TokKind     kind;
    const char *sval;   /* interned identifier text (T_IDENT only) */
    long        ival;   /* T_INT_LIT */
    double      dval;   /* T_DBL_LIT */
    int         line;
} Token;

/* String intern table: one canonical char* per distinct identifier. */
typedef struct InternEntry {
    char               *str;
    struct InternEntry *next;
} InternEntry;

#define INTERN_BUCKETS 1024

typedef struct {
    const char *src;
    size_t      len;
    size_t      pos;
    int         line;

    Token      *toks;   /* grows on demand                             */
    size_t      ntok;
    size_t      cap;

    InternEntry *intern[INTERN_BUCKETS];
} Lexer;

void        lexer_init(Lexer *lx, const char *src, size_t len);
Token      *lexer_next(Lexer *lx);   /* append & return next token   */
const char *intern(Lexer *lx, const char *s, size_t n);
void        lexer_free(Lexer *lx);

/* ------------------------------------------------------------------ */
/* Symbol table (open-addressed, linear probing, scope stack)          */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;   /* interned pointer; NULL = empty, TOMBSTONE = deleted */
    Type        type;
    int         slot;   /* local slot index within the function       */
    int         depth;  /* scope depth at which it was declared        */
} SymSlot;

typedef struct {
    SymSlot *slots;
    int      cap;
    int      count;
    int      depth;     /* current scope depth (0 = function top)      */
} Symtab;

void symtab_init(Symtab *st);
void symtab_free(Symtab *st);
void scope_push(Symtab *st);
void scope_pop(Symtab *st);                         /* removes this depth's entries */
int  symtab_insert(Symtab *st, const char *name, Type ty, int slot);
SymSlot *symtab_lookup(Symtab *st, const char *name);

/* ------------------------------------------------------------------ */
/* AST                                                                 */
/* ------------------------------------------------------------------ */

typedef enum {
    N_FUNC, N_PARAM, N_BLOCK, N_DECL, N_ASSIGN,
    N_IF, N_WHILE, N_RETURN, N_EXPRSTMT, N_PRINT,
    N_BINOP, N_UNOP, N_INTLIT, N_DBLLIT, N_VAR, N_CALL, N_INDEX,
    N_I2D, N_D2I           /* explicit int<->double conversions */
} NodeKind;

typedef struct AstNode AstNode;
struct AstNode {
    NodeKind    kind;
    Type        type;       /* resolved expression / decl type         */
    long        ival;
    double      dval;
    const char *name;       /* interned identifier                     */
    TokKind     op;         /* binop / unop operator                   */

    AstNode    *a, *b, *c;  /* generic children                        */
    AstNode   **list;       /* funcs / params / stmts / args           */
    int         nlist;

    int         slot;       /* resolved local slot (vars, decls)       */
    int         func_index; /* resolved function index (calls)         */
    int         line;
};

/* ------------------------------------------------------------------ */
/* Parser                                                              */
/* ------------------------------------------------------------------ */

typedef struct FuncSig {
    const char *name;
    Type        ret;
    Type       *params;
    int         nparams;
} FuncSig;

typedef struct {
    Lexer    *lx;
    Token    *cur;      /* current token                               */
    Token    *peek;     /* one-token lookahead                         */

    Symtab    sym;
    int       nlocals;  /* running slot counter for the current func   */

    FuncSig  *sigs;     /* pre-scanned function signatures             */
    int       nsigs;

    const char *fname;  /* input filename, for diagnostics             */
    int       had_error;
} Parser;

/* Parse a whole program.  Returns an N_FUNC list node (kind N_BLOCK
 * used as a program container), or NULL on error. */
AstNode *parse_program(const char *src, size_t len, const char *fname);

/* ------------------------------------------------------------------ */
/* Codegen + emit (read-only modules)                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    int  op;
    long arg;   /* const idx / immediate / slot / func idx / jump target */
    int  arg2;  /* CALL argc                                             */
} Instr;

typedef struct {
    int    tag; /* CONST_INT / CONST_DOUBLE */
    long   i;
    double d;
} Const;

typedef struct {
    char   name[64];
    int    nparams;
    int    nlocals;
    Instr *code;
    int    ncode;
    int    cap;
} Func;

typedef struct {
    Const *consts;
    int    nconsts, ccap;
    Func  *funcs;
    int    nfuncs, fcap;
    int    entry;
} Module;

/* codegen.c */
Module *codegen(AstNode *program);
void    module_free(Module *m);

/* emit.c (owns constant pool + binary writer) */
int  const_intern_int(Module *m, long v);
int  const_intern_double(Module *m, double v);
int  emit_module(Module *m, const char *out_path);

#endif /* SVC_H */
