/*
 * === DO NOT MODIFY THIS FILE ===
 * During testing, we will replace this file with our
 * own. You can create your own header files if necessary.
 * You have been warned. 
 * === DO NOT MODIFY THIS FILE ===
 */

#ifndef FUZZER_H
#define FUZZER_H

#include <stdio.h>

#include "runner.h"

int run_fuzzer(FILE *seed_file, int job_count, int input_count, int time_limit, char *target_program[]);

void fzl_init(const char *why);
void fzl_debug(const char *why);
// before writing to pipe
void fzl_sending_input(int runner_id, const char *input, const char *why);
void fzl_runner_init(int runner_id, const char *why);
// after reading from pipe
void fzl_runner_received_input(int runner_id, const char *input, const char *why);
void fzl_runner_launch(int runner_id, char *argv[], const char *why);
// before writing to pipe 
void fzl_runner_sending_status(int runner_id, RUNNER_STATE state, int aux_data, const char *why);
void fzl_runner_fini(int runner_id, const char *why);
// after reading from pipe
void fzl_received_status(int runner_id, RUNNER_STATE state, int aux_data, const char *why);
void fzl_fini(const char *why);

#endif