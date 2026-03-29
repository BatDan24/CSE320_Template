/*
 * === DO NOT MODIFY THIS FILE ===
 * During testing, we will replace this file with our
 * own. You can create your own header files if necessary.
 * You have been warned. 
 * === DO NOT MODIFY THIS FILE ===
 */

#ifndef COVERAGE_MAP_H
#define COVERAGE_MAP_H

typedef struct coverage_map * COVERAGE_MAP;
typedef enum coverage_priority {
    COV_NO_PRIO = -1,
    COV_LOW_PRIO,
    COV_HIGH_PRIO
} COVERAGE_PRIORITY;

COVERAGE_MAP coverage_map_init();

void coverage_map_fini(COVERAGE_MAP map);

COVERAGE_PRIORITY coverage_map_add(COVERAGE_MAP map, char *cov_data);

#endif