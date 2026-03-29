/*
 * === DO NOT MODIFY THIS FILE ===
 * During testing, we will replace this file with our
 * own. You can create your own header files if necessary.
 * You have been warned. 
 * === DO NOT MODIFY THIS FILE ===
 */

#ifndef RUNNER_H
#define RUNNER_H

#include "input.h"

typedef struct runner * RUNNER;
typedef enum runner_state {
    NO_STATE = -1,
    VALID,
    CRASH,
    TIMEOUT
} RUNNER_STATE;

RUNNER runner_init();

void runner_fini(RUNNER runner);

char *runner_coverage_map(RUNNER runner);

INPUT runner_get_active_input(RUNNER runner);

// FUZZER --> RUNNER
int fuzzer_send_runner_input(RUNNER runner, INPUT input);

char * runner_receive_fuzzer_input(RUNNER runner);

// RUNNER --> FUZZER
int runner_alert_fuzzer(RUNNER runner, RUNNER_STATE state, int data);

RUNNER_STATE fuzzer_attempt_receive_status(RUNNER runner, int *data); 

// runner main loop

int runner_launch(RUNNER runner);

// ----------------------------------------------------------------------------- //

typedef struct runners * RUNNERS;

RUNNERS runners_init(int job_count);

void runners_fini(RUNNERS runners);

int runners_submit_input(RUNNERS runners, INPUT input);

int runners_has_jobs(RUNNERS runners);

int runners_has_active_jobs(RUNNERS runners);

int runners_has_done_jobs(RUNNERS runners);

int runners_has_ready_jobs(RUNNERS runners);

void runners_check_if_jobs_done(RUNNERS runners);

RUNNER runners_process_result(RUNNERS runners, RUNNER_STATE *state, int *data);

int runners_reap(RUNNERS runners);

#endif