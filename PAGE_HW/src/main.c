/* Trace driver: runs a script of guest memory operations against one VM and
 * reports each result, how each access was resolved (TLB hit, TLB miss, page
 * fault), and the page-table and TLB state.
 *
 *   alloc NAME PAGES PERMS    reserve PAGES pages; PERMS is a subset of "rwx"
 *   free NAME                 release NAME's region by its allocation ID
 *   protect NAME PERMS        change NAME's permissions with sf_vm_protect
 *   write TARGET VALUE        sf_vm_write_word
 *   read TARGET [EXPECTED]    sf_vm_read_word; a mismatch makes the run fail
 *   translate TARGET r|w|x    sf_vm_translate (no TLB, no demand paging)
 *   hold N                    take up to N free frames to create memory pressure
 *   release                   return every held frame
 *   dump                      print the page table
 *   tlb                       print the valid TLB entries
 *   flush                     invalidate the whole TLB
 *   frames                    print used/free physical frame ranges
 *   frames on|off             enable/disable a frame map after each command
 *   stats                     print the summary line
 *
 * TARGET is NAME, NAME+OFFSET, or an absolute address. Numbers may be decimal
 * or 0x-prefixed hexadecimal. '#' starts a comment.
 */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pt_inspect.h"
#include "vm.h"

#define MAX_NAMES 64
#define MAX_NAME_LENGTH 31
#define MAX_LINE 512
#define MAX_TOKENS 5

typedef struct {
    char name[MAX_NAME_LENGTH + 1];
    sf_vm_region_t region;
} named_region_t;

typedef struct {
    sf_vm_t vm;
    named_region_t names[MAX_NAMES];
    size_t name_count;
    sf_frame_t held[SF_NUM_FRAME];
    size_t held_count;
    uint64_t accesses;
    size_t line_number;
    bool mismatch;
    bool show_frames;
    bool pretty;
    size_t step;
} driver_t;

static int fail(const driver_t *driver, const char *message, const char *detail)
{
    fprintf(stderr, "line %zu: %s%s%s\n", driver->line_number, message,
            detail != NULL ? ": " : "", detail != NULL ? detail : "");
    return 2;
}

static bool parse_u64(const char *text, uint64_t *out)
{
    if (text[0] == '-' || text[0] == '\0') {
        return false;
    }
    char *end;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 0);
    if (errno != 0 || *end != '\0') {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_word(const char *text, sf_word_t *out)
{
    char *end;
    errno = 0;
    long long value = strtoll(text, &end, 0);
    if (errno != 0 || *end != '\0' || text[0] == '\0') {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_permissions(const char *text, uint64_t *out)
{
    uint64_t permissions = 0;
    for (const char *c = text; *c != '\0'; ++c) {
        switch (*c) {
        case 'r': permissions |= SF_PTE_R; break;
        case 'w': permissions |= SF_PTE_W; break;
        case 'x': permissions |= SF_PTE_X; break;
        default: return false;
        }
    }
    *out = permissions;
    return true;
}

static named_region_t *find_name(driver_t *driver, const char *name)
{
    for (size_t i = 0; i < driver->name_count; ++i) {
        if (strcmp(driver->names[i].name, name) == 0) {
            return &driver->names[i];
        }
    }
    return NULL;
}

static bool valid_name(const char *name)
{
    if (!isalpha((unsigned char)name[0]) || strlen(name) > MAX_NAME_LENGTH) {
        return false;
    }
    for (const char *c = name; *c != '\0'; ++c) {
        if (!isalnum((unsigned char)*c) && *c != '_') {
            return false;
        }
    }
    return true;
}

/* NAME, NAME+OFFSET, or an absolute address. */
static bool parse_target(driver_t *driver, const char *text, sf_va_t *out)
{
    if (isdigit((unsigned char)text[0])) {
        return parse_u64(text, out);
    }
    char name[MAX_NAME_LENGTH + 1];
    const char *plus = strchr(text, '+');
    size_t length = plus != NULL ? (size_t)(plus - text) : strlen(text);
    if (length == 0 || length > MAX_NAME_LENGTH) {
        return false;
    }
    memcpy(name, text, length);
    name[length] = '\0';
    named_region_t *entry = find_name(driver, name);
    if (entry == NULL) {
        return false;
    }
    uint64_t offset = 0;
    if (plus != NULL && !parse_u64(plus + 1, &offset)) {
        return false;
    }
    *out = entry->region.start + offset;
    return true;
}

static void print_summary(const driver_t *driver)
{
    const sf_vm_stats_t *stats = &driver->vm.stats;
    sf_pt_summary_t summary = sf_pt_summarize(&driver->vm);
    if (driver->pretty) {
        printf("  Summary\n"
               "    Accesses: %" PRIu64 " | TLB hits: %" PRIu64
               " | TLB misses: %" PRIu64 " | Page faults: %" PRIu64 "\n"
               "    Allocated frames: %zu | Table frames: %zu | Mappings: %zu\n"
               "    Malformed entries: %zu | Stale TLB entries: %zu\n",
               driver->accesses, stats->tlb_hits, stats->tlb_misses,
               stats->page_faults, sf_pt_allocated_frames(&driver->vm),
               summary.table_frames, summary.mappings, summary.malformed,
               sf_pt_incoherent_tlb_entries(&driver->vm));
        return;
    }
    printf("summary: %" PRIu64 " accesses, %" PRIu64 " TLB hits, %" PRIu64
           " TLB misses, %" PRIu64 " page faults, "
           "%zu frames allocated, %zu table frames, %zu mappings",
           driver->accesses, stats->tlb_hits, stats->tlb_misses,
           stats->page_faults, sf_pt_allocated_frames(&driver->vm),
           summary.table_frames, summary.mappings);
    if (summary.malformed != 0) {
        printf(", %zu MALFORMED entries", summary.malformed);
    }
    size_t incoherent = sf_pt_incoherent_tlb_entries(&driver->vm);
    if (incoherent != 0) {
        printf(", %zu STALE TLB entries", incoherent);
    }
    printf("\n");
}

/* Report allocator ownership, including table, data, and held frames.
 * Ranges are inclusive frame IDs, not byte addresses or virtual regions.
 */
static void print_frames(const driver_t *driver)
{
    const sf_frame_allocator_t *allocator = &driver->vm.frames;
    size_t used = sf_pt_allocated_frames(&driver->vm);
    if (driver->pretty) {
        printf("\n  Physical frames: %zu used | %zu free | %zu total\n",
               used, (size_t)SF_NUM_FRAME - used, (size_t)SF_NUM_FRAME);
        printf("  %-7s %-23s %s\n", "State", "Frame IDs (inclusive)", "Count");
        printf("  ------- ----------------------- -------\n");
    } else {
        printf("frames: %zu used, %zu free (ranges inclusive)\n",
               used, (size_t)SF_NUM_FRAME - used);
    }
    sf_frame_t first = 0;
    while (first < SF_NUM_FRAME) {
        bool allocated = sf_frame_allocator_is_allocated(allocator, first);
        sf_frame_t end = first + 1;
        while (end < SF_NUM_FRAME &&
               sf_frame_allocator_is_allocated(allocator, end) == allocated) {
            end++;
        }
        if (driver->pretty) {
            char range[48];
            if (end - first == 1) {
                snprintf(range, sizeof(range), "%" PRIu64, first);
            } else {
                snprintf(range, sizeof(range), "%" PRIu64 "-%" PRIu64, first, end - 1);
            }
            printf("  %-7s %-23s %" PRIu64 "\n",
                   allocated ? "USED" : "FREE", range, end - first);
        } else {
            printf("  %s %" PRIu64, allocated ? "used" : "free", first);
            if (end - first > 1) {
                printf("-%" PRIu64, end - 1);
            }
            printf(" (%" PRIu64 " %s)\n", end - first,
                   end - first == 1 ? "frame" : "frames");
        }
        first = end;
    }
}

/* How the access was resolved, from the change in the VM's counters. */
static void print_access_marker(const sf_vm_stats_t *before, const sf_vm_t *vm)
{
    if (vm->stats.tlb_hits != before->tlb_hits) {
        printf(" (tlb hit)");
    } else if (vm->stats.tlb_misses != before->tlb_misses) {
        printf(vm->stats.page_faults != before->page_faults
               ? " (tlb miss, page fault)" : " (tlb miss)");
    }
}

static void echo(const driver_t *driver, char **tokens, size_t count)
{
    if (driver->pretty) {
        printf("  Result: ");
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        printf(i == 0 ? "%s" : " %s", tokens[i]);
    }
    printf(": ");
}

static int run_command(driver_t *driver, char **tokens, size_t count)
{
    const char *command = tokens[0];

    if (strcmp(command, "alloc") == 0) {
        uint64_t pages;
        uint64_t permissions;
        if (count != 4 || !valid_name(tokens[1]) || !parse_u64(tokens[2], &pages) ||
            !parse_permissions(tokens[3], &permissions)) {
            return fail(driver, "usage", "alloc NAME PAGES PERMS");
        }
        named_region_t *entry = find_name(driver, tokens[1]);
        if (entry == NULL) {
            if (driver->name_count == MAX_NAMES) {
                return fail(driver, "too many names", NULL);
            }
            entry = &driver->names[driver->name_count++];
            strcpy(entry->name, tokens[1]);
            entry->region = (sf_vm_region_t){0};
        }
        sf_vm_region_t region;
        sf_vm_result_t result = sf_vm_alloc(&driver->vm, (size_t)pages,
                                            permissions, &region);
        echo(driver, tokens, count);
        printf("%s", sf_vm_result_as_cstr(result));
        if (result == SF_VM_OK) {
            char text[4];
            sf_pt_format_permissions(region.permissions, text);
            entry->region = region;
            printf(" id=%" PRIu64 " start=0x%" PRIx64 " pages=%zu perms=%s",
                   region.id, region.start, region.pages, text);
        }
        printf("\n");
        return 0;
    }

    if (strcmp(command, "free") == 0) {
        named_region_t *entry = count == 2 ? find_name(driver, tokens[1]) : NULL;
        if (entry == NULL) {
            return fail(driver, "usage", "free NAME (NAME must have been allocated)");
        }
        sf_vm_result_t result = sf_vm_free(&driver->vm, entry->region.id);
        echo(driver, tokens, count);
        printf("%s\n", sf_vm_result_as_cstr(result));
        return 0;
    }

    if (strcmp(command, "protect") == 0) {
        named_region_t *entry = count == 3 ? find_name(driver, tokens[1]) : NULL;
        uint64_t permissions;
        if (entry == NULL || !parse_permissions(tokens[2], &permissions)) {
            return fail(driver, "usage",
                        "protect NAME PERMS (NAME must have been allocated)");
        }
        sf_vm_result_t result = sf_vm_protect(&driver->vm, entry->region.id,
                                              permissions);
        echo(driver, tokens, count);
        printf("%s", sf_vm_result_as_cstr(result));
        if (result == SF_VM_OK) {
            char text[4];
            sf_pt_format_permissions(permissions, text);
            entry->region.permissions = permissions;
            printf(" perms=%s", text);
        }
        printf("\n");
        return 0;
    }

    if (strcmp(command, "write") == 0) {
        sf_va_t va;
        sf_word_t value;
        if (count != 3 || !parse_target(driver, tokens[1], &va) ||
            !parse_word(tokens[2], &value)) {
            return fail(driver, "usage", "write TARGET VALUE");
        }
        sf_vm_stats_t before = driver->vm.stats;
        driver->accesses++;
        sf_vm_result_t result = sf_vm_write_word(&driver->vm, va, value);
        echo(driver, tokens, count);
        printf("%s", sf_vm_result_as_cstr(result));
        print_access_marker(&before, &driver->vm);
        printf("\n");
        return 0;
    }

    if (strcmp(command, "read") == 0) {
        sf_va_t va;
        sf_word_t expected = 0;
        if ((count != 2 && count != 3) || !parse_target(driver, tokens[1], &va) ||
            (count == 3 && !parse_word(tokens[2], &expected))) {
            return fail(driver, "usage", "read TARGET [EXPECTED]");
        }
        sf_vm_stats_t before = driver->vm.stats;
        driver->accesses++;
        sf_word_t value = 0;
        sf_vm_result_t result = sf_vm_read_word(&driver->vm, va, &value);
        echo(driver, tokens, count);
        printf("%s", sf_vm_result_as_cstr(result));
        if (result == SF_VM_OK) {
            printf(" value=%" PRId64, value);
        }
        print_access_marker(&before, &driver->vm);
        if (count == 3 && (result != SF_VM_OK || value != expected)) {
            printf(" MISMATCH expected %" PRId64, expected);
            driver->mismatch = true;
        }
        printf("\n");
        return 0;
    }

    if (strcmp(command, "translate") == 0) {
        sf_va_t va;
        sf_vm_access_t access;
        if (count != 3 || !parse_target(driver, tokens[1], &va) ||
            strlen(tokens[2]) != 1 || strchr("rwx", tokens[2][0]) == NULL) {
            return fail(driver, "usage", "translate TARGET r|w|x");
        }
        access = tokens[2][0] == 'r' ? SF_VM_ACCESS_READ
               : tokens[2][0] == 'w' ? SF_VM_ACCESS_WRITE : SF_VM_ACCESS_EXECUTE;
        sf_pa_t pa = 0;
        sf_vm_result_t result = sf_vm_translate(&driver->vm, va, access, &pa);
        echo(driver, tokens, count);
        printf("%s", sf_vm_result_as_cstr(result));
        if (result == SF_VM_OK) {
            printf(" pa=0x%" PRIx64 " frame=%" PRIu64, pa, pa / SF_PAGE_SIZE);
        }
        printf("\n");
        return 0;
    }

    if (strcmp(command, "hold") == 0) {
        uint64_t wanted;
        if (count != 2 || !parse_u64(tokens[1], &wanted)) {
            return fail(driver, "usage", "hold N");
        }
        size_t held = 0;
        sf_frame_t frame;
        while (held < wanted && driver->held_count < SF_NUM_FRAME &&
               sf_frame_alloc(&driver->vm, &frame)) {
            driver->held[driver->held_count++] = frame;
            held++;
        }
        echo(driver, tokens, count);
        printf("held %zu frames\n", held);
        return 0;
    }

    if (strcmp(command, "release") == 0) {
        if (count != 1) {
            return fail(driver, "usage", "release");
        }
        size_t released = driver->held_count;
        while (driver->held_count != 0) {
            sf_frame_free(&driver->vm, driver->held[--driver->held_count]);
        }
        echo(driver, tokens, count);
        printf("released %zu frames\n", released);
        return 0;
    }

    if (strcmp(command, "frames") == 0) {
        if (count == 1) {
            print_frames(driver);
            return 0;
        }
        if (count != 2 || (strcmp(tokens[1], "on") != 0 &&
                           strcmp(tokens[1], "off") != 0)) {
            return fail(driver, "usage", "frames [on|off]");
        }
        driver->show_frames = strcmp(tokens[1], "on") == 0;
        echo(driver, tokens, count);
        printf("%s\n", driver->show_frames ? "enabled" : "disabled");
        if (driver->show_frames) {
            print_frames(driver);
        }
        return 0;
    }

    if (strcmp(command, "dump") == 0 && count == 1) {
        sf_pt_dump(&driver->vm, stdout);
        return 0;
    }

    if (strcmp(command, "tlb") == 0 && count == 1) {
        sf_tlb_dump(&driver->vm, stdout);
        return 0;
    }

    if (strcmp(command, "flush") == 0 && count == 1) {
        sf_tlb_flush(&driver->vm.tlb);
        echo(driver, tokens, count);
        printf("TLB flushed\n");
        return 0;
    }

    if (strcmp(command, "stats") == 0 && count == 1) {
        print_summary(driver);
        return 0;
    }

    return fail(driver, "unknown command", command);
}

static int run(driver_t *driver, FILE *input)
{
    char line[MAX_LINE];
    bool summary_is_last = false;
    while (fgets(line, sizeof(line), input) != NULL) {
        driver->line_number++;
        if (strchr(line, '\n') == NULL && !feof(input)) {
            return fail(driver, "line too long", NULL);
        }
        char *comment = strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }
        char *tokens[MAX_TOKENS];
        size_t count = 0;
        for (char *token = strtok(line, " \t\r\n"); token != NULL;
             token = strtok(NULL, " \t\r\n")) {
            if (count == MAX_TOKENS) {
                return fail(driver, "too many arguments", NULL);
            }
            tokens[count++] = token;
        }
        if (count == 0) {
            continue;
        }
        if (driver->pretty) {
            printf("\n------------------------------------------------------------\n");
            printf("Step %zu | Line %zu |", ++driver->step, driver->line_number);
            for (size_t i = 0; i < count; i++) {
                printf(" %s", tokens[i]);
            }
            printf("\n------------------------------------------------------------\n");
            if (comment != NULL) {
                char *note = comment + 1;
                while (isspace((unsigned char)*note)) {
                    note++;
                }
                note[strcspn(note, "\r\n")] = '\0';
                if (*note != '\0') {
                    printf("  Note: %s\n", note);
                }
            }
        }
        int status = run_command(driver, tokens, count);
        if (status != 0) {
            return status;
        }
        if (driver->show_frames && strcmp(tokens[0], "frames") != 0) {
            print_frames(driver);
        }
        summary_is_last = strcmp(tokens[0], "stats") == 0;
    }
    if (!summary_is_last) {
        if (driver->pretty) {
            printf("\nEnd of trace\n");
        }
        print_summary(driver);
    }
    return driver->mismatch ? 1 : 0;
}

int main(int argc, char **argv)
{
    bool pretty = argc == 3 && strcmp(argv[1], "--pretty") == 0;
    if ((!pretty && argc != 2) || (argc == 2 && strcmp(argv[1], "--pretty") == 0)) {
        fprintf(stderr, "Usage: %s [--pretty] TRACE_FILE   (use - for standard input)\n", argv[0]);
        return 2;
    }
    const char *path = argv[pretty ? 2 : 1];
    FILE *input = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
    if (input == NULL) {
        fprintf(stderr, "Could not open '%s'\n", path);
        return 2;
    }

    static driver_t driver;
    driver.pretty = pretty;
    sf_vm_init(&driver.vm);
    int status = run(&driver, input);
    if (input != stdin) {
        fclose(input);
    }
    return status;
}
