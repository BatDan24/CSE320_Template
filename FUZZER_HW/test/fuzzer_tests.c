#include <criterion/criterion.h>
#include <criterion/internal/assert.h>

/*
 * Add your test cases here!
 */

Test(fuzzer_tests, hello_world) {
    cr_assert(1 == 2, "Hello world!");
}