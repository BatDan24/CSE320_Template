#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "game.h"

/*
 * Text protocol between clients and the server. Every message is one or
 * more lines terminated by '\n' (a '\r' before the '\n' is ignored).
 * Command keywords and directions are case-insensitive. See README.
 */

#define MAX_LINE 256     /* Longest command line, including "\n" */
#define MAX_CHAT_LEN 200 /* Longest SAY text */

typedef enum {
    CMD_EMPTY = 0, /* Blank line: ignored, no reply */
    CMD_JOIN,      /* JOIN <name>   */
    CMD_WATCH,     /* WATCH         */
    CMD_MOVE,      /* MOVE <dir>    */
    CMD_TURN,      /* TURN <dir>    */
    CMD_FIRE,      /* FIRE          */
    CMD_SAY,       /* SAY <text...> */
    CMD_SCORES,    /* SCORES        */
    CMD_STATS,     /* STATS         */
    CMD_PING,      /* PING          */
    CMD_QUIT,      /* QUIT          */
    CMD_BAD_ARGS,  /* Known keyword, wrong arguments */
    CMD_UNKNOWN    /* Unknown keyword */
} command_type_t;

typedef struct {
    command_type_t type;
    dir_t dir;           /* MOVE, TURN */
    char arg[MAX_LINE];  /* JOIN: the name; SAY: the text */
} command_t;

/*
 * Parse one command line (with or without its trailing "\r\n") into cmd.
 * Always fills in cmd->type; never fails.
 *
 * Tokens are separated by one or more spaces/tabs. JOIN takes exactly one
 * token (the name is validated later by game_add_tank). MOVE and TURN take
 * exactly one direction. SAY takes the rest of the line with leading
 * whitespace removed; it must be 1..MAX_CHAT_LEN characters. All other
 * commands take no arguments.
 */
void protocol_parse(const char *line, command_t *cmd);

/* ---------------------------------------------------------------------- */
/* Server -> client messages                                               */
/* ---------------------------------------------------------------------- */

#define MSG_WELCOME "WELCOME %d %d\n"          /* tank id, tick rate */
#define MSG_WATCHING "WATCHING %d\n"           /* tick rate */
#define MSG_OK_MOVE "OK MOVE %d %d %c\n"       /* x, y, dir */
#define MSG_OK_TURN "OK TURN %c\n"             /* dir */
#define MSG_OK_FIRE "OK FIRE\n"
#define MSG_OK_SAY "OK SAY\n"
#define MSG_CHAT "CHAT %s %s\n"                /* name, text */
#define MSG_PONG "PONG\n"
#define MSG_BYE "BYE\n"
#define MSG_ERR "ERR %s\n"                     /* error token */
#define MSG_GAMEOVER "GAMEOVER\n"              /* followed by a score table */
#define MSG_STATS                                                              \
    "STATS tick=%ld players=%d joins=%ld leaves=%ld shots=%ld hits=%ld "     \
    "kills=%ld commands=%ld sessions=%d\n"

/* Error tokens that do not come from game_result_str(). */
#define ERR_UNKNOWN_COMMAND "UNKNOWN_COMMAND"
#define ERR_BAD_ARGS "BAD_ARGS"
#define ERR_LINE_TOO_LONG "LINE_TOO_LONG"
#define ERR_NOT_JOINED "NOT_JOINED"
#define ERR_ALREADY_JOINED "ALREADY_JOINED"

#endif /* PROTOCOL_H */
