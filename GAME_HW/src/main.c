#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "global.h"
#include "log.h"
#include "server.h"

int main(int argc, char **argv)
{
    // TODO: Parse the options with getopt (see PRINT_USAGE and the
    // PRINT_ERROR_* messages in global.h), ignore SIGPIPE, call log_init,
    // check the map, then server_init, PRINT_LISTENING, server_run, and
    // server_destroy. Keep this file to argument parsing.
    (void)argc;
    (void)argv;
    return EXIT_FAILURE;
}
