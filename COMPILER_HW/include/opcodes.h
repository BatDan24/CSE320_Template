#ifndef SVC_OPCODES_H
#define SVC_OPCODES_H

/*
 * .bvm binary format (all multibyte integers little-endian):
 *
 *   magic     : 4 bytes = 'B','V','M','1'
 *   u32         n_consts
 *   consts[]  : n_consts * { u8 tag (0=int,1=double); u8 pad[7]; i64/f64 value }
 *   u32         n_funcs
 *   u32         entry            (index of function "main")
 *   funcs[]   : for each function:
 *                 u16 name_len; char name[name_len];
 *                 u16 nparams;
 *                 u16 nlocals;   (total local slots, params included)
 *                 u32 code_len;  (bytes)
 *                 u8  code[code_len];
 *
 * Instruction encoding: 1 opcode byte followed by 0+ little-endian operands.
 * Jump operands are i32 offsets relative to the byte *after* the operand.
 */

enum Opcode {
    OP_PUSHC,   /* u32 const index                 */
    OP_PUSHI,   /* i32 immediate int               */
    OP_LOADL,   /* u16 local slot                  */
    OP_STOREL,  /* u16 local slot                  */
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_NEG,
    OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE,
    OP_I2D, OP_D2I,
    OP_JMP,     /* i32 rel offset                  */
    OP_JZ,      /* i32 rel offset (pop, jump if 0) */
    OP_CALL,    /* u16 func index, u8 argc         */
    OP_RET,     /* return value on stack           */
    OP_RETV,    /* return void                     */
    OP_NEWARR,  /* pop len, push array ref         */
    OP_ALOAD,   /* pop idx, pop ref, push elem     */
    OP_ASTORE,  /* pop val, pop idx, pop ref       */
    OP_PRINT,   /* pop, print (int or double)      */
    OP_POP,     /* discard top of stack            */
    OP_HALT
};

#define BVM_MAGIC0 'B'
#define BVM_MAGIC1 'V'
#define BVM_MAGIC2 'M'
#define BVM_MAGIC3 '1'

#define CONST_INT    0
#define CONST_DOUBLE 1

#endif /* SVC_OPCODES_H */
