#include <string.h>

#include "protocol.h"

/* TODO body is a placeholder. Implement the contract in protocol.h. */

void protocol_parse(const char *line, command_t *cmd)
{
    // TODO: Tokenize a copy of line with strtok_r, match the keyword
    // case-insensitively, and check the arguments.
    (void)line;
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = CMD_UNKNOWN;
}
