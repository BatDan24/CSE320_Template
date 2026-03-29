/*
 * === DO NOT MODIFY THIS FILE ===
 * During testing, we will replace this file with our
 * own. You can create your own header files if necessary.
 * You have been warned. 
 * === DO NOT MODIFY THIS FILE ===
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdlib.h>
#include <stdint.h>

typedef uint64_t MUTATOR_STATE;
typedef struct input * INPUT;

INPUT make_input(const char *input_str);

void free_input(INPUT input);

size_t input_len(INPUT input);

const char *input_str(INPUT input);

MUTATOR_STATE input_mutator_state(INPUT input);

MUTATOR_STATE input_set_state(INPUT input, MUTATOR_STATE state);

MUTATOR_STATE input_state_step(INPUT input);

#endif