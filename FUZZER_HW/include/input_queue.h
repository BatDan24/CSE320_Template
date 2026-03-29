/*
 * === DO NOT MODIFY THIS FILE ===
 * During testing, we will replace this file with our
 * own. You can create your own header files if necessary.
 * You have been warned. 
 * === DO NOT MODIFY THIS FILE ===
 */

#ifndef INPUT_QUEUE_H
#define INPUT_QUEUE_H

#include "input.h"

typedef struct input_queue * INPUT_QUEUE;

INPUT_QUEUE input_queue_init();

void input_queue_fini(INPUT_QUEUE queue);

void enqueue_high_prio_input(INPUT_QUEUE queue, INPUT input);

void enqueue_low_prio_input(INPUT_QUEUE queue, INPUT input);

INPUT dequeue_input(INPUT_QUEUE queue);

#endif