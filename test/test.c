#include <time.h>
#include <math.h>
#include "test.h"
#include "all_tests.h"

#define TST_STAT_SIZE 4
/* chars use in ticker. This is usually too fast to see */
static char status[TST_STAT_SIZE] = { '|', '/', '-', '\\' };

/* last char output to ticker */
static int curr_char;

/* The following variables control the running of the tests and can be set via
 * the command line. See USAGE below.
 *
 * verbose:    true if you want verbose messages in the error diagnostics
 * exclude:    true if you want to exclude tests entered via the command line
 * quiet:      true if you don't want to display the ticker. This is useful if
 *             you are writing the error diagnostics to a file
 * force:      true if you want tests to run to the end. Otherwise they will
 *             stop once an Assertion fails.
 * list_tests: true if you want to list all tests available
 */
static bool verbose = false;
static bool exclude = false;
static bool quiet = false;
static bool force = false;
static bool list_tests = false;

/* holds the list of tests specified on the command line. This will either be
 * included or excluded if '-x' is set */
static char **testlist = NULL;

/* statistics */
static int t_cnt = 0;/* number of tests run */
static int a_cnt = 0;/* number of assertions */
static int f_cnt = 0;/* number of failures */

/* This is the size of the error diagnostics buffer. If you are getting too
 * many errors and the end of the buffer is being reached you can set it here.
 */
#define MAX_MSG_SIZE 100000
static char msg_buf[MAX_MSG_SIZE] = "";
static char tmp_buf[1000] = "";

/* Check to see if +testname+ was specified on the command line */
static bool find_test_name(const char *testname)
{
    int i;
    for (i = 0; testlist[i] != NULL; i++) {
        if (!strcmp(testlist[i], testname)) {
            return true;
        }
    }
    return false;
}

/* Determine if the test should be run at all */
static bool should_test_run(const char *testname)
{
    int found = 0;
    if (list_tests == true) {
        /* when listing tests, don't run any of them */
        return false;
    }
    if (testlist == NULL) {
        /* If no tests where specified, run them all. Same goes even if
         * exclude is true */
        return true;
    }
    found = find_test_name(testname);
    if ((found && !exclude) || (!found && exclude)) {
        return true;
    }
    return false;
}

static void reset_status(void)
{
    curr_char = 0;
}

/* Basically this just deletes the previous character and replaces it with the
 * next character in the ticker array. If the tests run slowly enough it will
 * look like a spinning bar. */
static void update_status(void)
{
    if (!quiet) {
        curr_char = (curr_char + 1) % TST_STAT_SIZE;
        fprintf(stdout, "\b%c", status[curr_char]);
        fflush(stdout);
    }
}


/*
 * Ends the test suite by writing SUCCESS or FAILED at the end of the test
 * results in the diagnostics. */
static void end_suite(tst_suite * suite)
{
    if (suite != NULL) {
        sub_suite *last = suite->tail;
        if (!quiet) {
            fprintf(stdout, "\b");
            fflush(stdout);
        }
        if (last->failed == 0) {
            fprintf(stdout, "SUCCESS\n");
            fflush(stdout);
        }
        else {
            fprintf(stdout, "FAILED %d of %d\n", last->failed,
                    last->num_test);
            fflush(stdout);
        }
    }
}

tst_suite *tst_add_suite(tst_suite * suite, const char *suite_name_full)
{
    sub_suite *subsuite;
    char *p;
    const char *suite_name;
    curr_char = 0;

    /* Suite will be the last suite that was added to the suite list or NULL
     * if this is the first call to tst_add_suite. We should only end the
     * suite if we actually ran it */
    if (suite && suite->tail && !suite->tail->not_run) {
        end_suite(suite);
    }

    /* Create a new subsuite */
    subsuite = malloc(sizeof(sub_suite));
    subsuite->num_test = 0;
    subsuite->failed = 0;
    subsuite->next = NULL;

    /* We may want to strip the file extension and file path from
     * suite_name_full depending on __FILE__ expansion
     */
    suite_name = strrchr(suite_name_full, '/');
    if (suite_name) {
        suite_name++;
    }
    else {
        suite_name = suite_name_full;
    }
    /* strip extension */
    p = strrchr(suite_name, '.');
    if (!p) {
        p = strrchr(suite_name, '\0');
    }
    subsuite->name = memcpy(calloc(p - suite_name + 1, 1),
                            suite_name, p - suite_name);

    /* If we are listing tests, just write the name of the test */
    if (list_tests) {
        fprintf(stdout, "%s\n", subsuite->name);
    }

    /* subsuite->not_run is false as we run the test by default */
    subsuite->not_run = false;

    if (suite == NULL) {
        /* This is the first call to tst_add_suite so we need to create the
         * suite */
        suite = malloc(sizeof(*suite));
        suite->head = subsuite;
        suite->tail = subsuite;
    }
    else {
        /* Add the current suite to the tail of the suite linked list */
        suite->tail->next = subsuite;
        suite->tail = subsuite;
    }

    if (!should_test_run(subsuite->name)) {
        /* if should_test_run returned false then don't run the test */
        subsuite->not_run = true;
        return suite;
    }

    reset_status();
    fprintf(stdout, "  %-20s:  ", subsuite->name);
    update_status();
    fflush(stdout);

    return suite;
}

void tst_run_test_with_name(tst_suite * ts, test_func f, void *value,
                            char *func_name)
{
    tst_case tc;
    sub_suite *ss;

    t_cnt++;

    if (!should_test_run(ts->tail->name)) {
        return;
    }
    ss = ts->tail;

    tc.failed = false;
    tc.suite = ss;
    tc.name = func_name;

    ss->num_test++;
    update_status();

    f(&tc, value);

    if (tc.failed) {
        ss->failed++;
        if (quiet) {
          printf("F");
        }
        else {
          printf("\bF_");
        }
    }
    else {
        if (quiet) {
          printf(".");
        }
        else {
          printf("\b._");
        }
    }
    update_status();
}

static int report(tst_suite * suite)
{
    int count = 0;
    sub_suite *dptr;

    if (suite && suite->tail && !suite->tail->not_run) {
        end_suite(suite);
    }

    if (list_tests) {
        return 0;
    }

    for (dptr = suite->head; dptr; dptr = dptr->next) {
        count += dptr->failed;
    }


    printf("\n%d tests, %d assertions, %d failures\n\n", t_cnt, a_cnt, f_cnt);
    if (count == 0) {
        printf("All tests passed.\n");
        return 0;
    }

    dptr = suite->head;
    fprintf(stdout, "%-15s\t\tTotal\tFail\tFailed %%\n", "Failed Tests");
    fprintf(stdout, "===================================================\n");
    while (dptr != NULL) {
        if (dptr->failed != 0) {
            double percent =
                ((double) dptr->failed / (double) dptr->num_test);
            fprintf(stdout, "%-15s\t\t%5d\t%4d\t%6.2f%%\n", dptr->name,
                    dptr->num_test, dptr->failed, percent * 100);
        }
        dptr = dptr->next;
    }
    fprintf(stdout, "===================================================\n");
    fprintf(stdout, "\n%s\n", msg_buf);
    return 1;
}

static const char *curr_err_func = "";

void Tmsg(const char *fmt, ...)
{
    va_list args;

    if (verbose) {
        va_start(args, fmt);
        vsprintf(tmp_buf, fmt, args);
        va_end(args);
        if (strlen(tmp_buf) + strlen(msg_buf) < MAX_MSG_SIZE) {
            sprintf(msg_buf + strlen(msg_buf), "\t%s\n", tmp_buf);
        }
        else {
            fprintf(stderr, "msg_buf full: %s\n", tmp_buf);
            fflush(stderr);
        }
    }
}

void vTmsg(const char *fmt, va_list args)
{
    if (verbose) {
        vsprintf(tmp_buf, fmt, args);
        if (strlen(tmp_buf) + strlen(msg_buf) < MAX_MSG_SIZE) {
            sprintf(msg_buf + strlen(msg_buf), "\t%s\n", tmp_buf);
        }
        else {
            fprintf(stderr, "msg_buf full: %s", tmp_buf);
            fflush(stderr);
        }
    }
}

void tst_msg(const char *func, const char *fname, int line_num,
             const char *fmt, ...)
{
    int i;
    va_list args;

    f_cnt++;

    if (verbose) {
        if (strcmp(curr_err_func, (char *) func) != 0) {
            sprintf(tmp_buf, "\n%s\n", func);
            strcat(msg_buf, tmp_buf);
            for (i = (int) strlen(tmp_buf); i > 0; i--) {
                strcat(msg_buf, "=");
            }
            strcat(msg_buf, "\n");
            curr_err_func = func;
        }
        sprintf(tmp_buf, "%3d)\n\t%s:%d\n\t", f_cnt, fname, line_num);
        va_start(args, fmt);
        vsprintf(tmp_buf + strlen(tmp_buf), fmt, args);
        va_end(args);
        if (strlen(tmp_buf) + strlen(msg_buf) < MAX_MSG_SIZE) {
            strcat(msg_buf, tmp_buf);
        }
        else {
            fprintf(stderr, "msg_buf full: %s", tmp_buf);
            fflush(stderr);
        }
    }
}

bool tst_int_equal(int line_num, tst_case *tc, const f_u64 expected,
                   const f_u64 actual)
{
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (expected == actual) {
        return true;
    }

    tc->failed = true;
    tst_msg(tc->name, tc->suite->name, line_num,
            "expected <%d>, but saw <%d>\n", expected, actual);
    return false;
}

bool tst_flt_delta_equal(int line_num, tst_case *tc, const double expected,
                         const double actual, const double delta)
{
    double diff;
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (expected == actual) {
        return true;
    }

    /* account for floating point error */
    diff = (expected - actual) / expected;
    if (abs(diff) < delta) {
        return true;
    }

    tc->failed = true;
    tst_msg(tc->name, tc->suite->name, line_num,
            "expected <%G>, but saw <%G>\n", expected, actual);
    return false;
}

bool tst_flt_equal(int line_num, tst_case *tc, const double expected,
                   const double actual)
{
    return tst_flt_delta_equal(line_num, tc, expected, actual, 0.00001);
}

bool tst_str_equal(int line_num, tst_case *tc, const char *expected,
                   const char *actual)
{
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (expected == NULL && actual == NULL) {
        return true;
    }
    if (expected && actual && strcmp(expected, actual) == 0) {
        return true;
    }

    tc->failed = true;
    tst_msg(tc->name, tc->suite->name, line_num,
            "expected <\"%s\">, but saw <\"%s\">\n", expected, actual);
    return false;
}

bool tst_arr_int_equal(int line_num, tst_case *tc, const int *expected,
                       const int *actual, int size)
{
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (expected == NULL && actual == NULL) {
        return true;
    }
    if (expected && actual) {
        int i;
        for (i = 0; i < size; i++) {
            if (expected[i] != actual[i]) {
                tc->failed = true;
                tst_msg(tc->name, tc->suite->name, line_num,
                        "testing array element %d, expected <%d>,"
                        "but saw <%d>\n",
                        i, expected[i], actual[i]);
            }
        }
    }
    return false;
}

bool tst_arr_str_equal(int line_num, tst_case *tc, char **expected,
                       char **actual, int size)
{
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (expected == NULL && actual == NULL) {
        return true;
    }
    if (expected && actual) {
        int i;
        for (i = 0; i < size; i++) {
            if (strcmp(expected[i], actual[i]) != 0) {
                tc->failed = true;
                tst_msg(tc->name, tc->suite->name, line_num,
                        "testing array element %d, expected "
                        "<\"%s\">, but saw <\"%s\">\n",
                        i, expected[i], actual[i]);
            }
        }
    }
    return false;
}

bool tst_str_nequal(int line_num, tst_case *tc, const char *expected,
                    const char *actual, size_t n)
{
    char *buf1;
    char *buf2;

    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (!strncmp(expected, actual, n)) {
        return true;
    }

    tc->failed = true;

    buf1 = ALLOC_N(char, n + 1);
    buf2 = ALLOC_N(char, n + 1);
    tst_msg(tc->name, tc->suite->name, line_num,
            "expected <\"%s\">, but saw <\"%s\">.\n",
            strncpy(buf1, expected, n), strncpy(buf2, actual, n));
    free(buf1);
    free(buf2);
    return false;
}

bool tst_ptr_null(int line_num, tst_case *tc, const void *ptr)
{
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (ptr == NULL) {
        return true;
    }

    tc->failed = true;
    tst_msg(tc->name, tc->suite->name, line_num, "didn't expect <%p>\n", ptr);
    return false;
}

bool tst_ptr_notnull(int line_num, tst_case *tc, const void *ptr)
{
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (ptr != NULL) {
        return true;
    }

    tc->failed = true;
    tst_msg(tc->name, tc->suite->name, line_num, "didn't expect <%p>\n", ptr);
    return false;
}

bool tst_ptr_equal(int line_num, tst_case *tc, const void *expected,
                   const void *actual)
{
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (expected == actual) {
        return true;
    }

    tc->failed = true;
    tst_msg(tc->name, tc->suite->name, line_num,
            "expected <%p>, but saw <%p>\n", expected, actual);
    return false;
}

bool tst_fail(int line_num, tst_case *tc, const char *fmt, ...)
{
    va_list args;
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    tc->failed = true;
    va_start(args, fmt);
    tst_msg(tc->name, tc->suite->name, line_num, fmt, args);
    va_end(args);
    return false;
}

bool tst_assert(int line_num, tst_case *tc, int condition,
                const char *fmt, ...)
{
    va_list args;
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (condition) {
        return true;
    }

    tc->failed = true;
    tst_msg(tc->name, tc->suite->name, line_num, "Assertion failed\n");
    va_start(args, fmt);
    vTmsg(fmt, args);
    va_end(args);
    return false;
}

bool tst_true(int line_num, tst_case *tc, int condition)
{
    a_cnt++;
    update_status();
    if (tc->failed && !force) {
        return true;
    }

    if (condition) {
        return true;
    }

    tc->failed = true;
    tst_msg(tc->name, tc->suite->name, line_num,
            "Condition is false, but expected true\n", tc->suite->name,
            line_num);
    return false;
}

bool tst_not_impl(int line_num, tst_case *tc, const char *message)
{
    a_cnt++;
    update_status();

    tc->suite->not_impl++;
    tst_msg(tc->name, tc->suite->name, line_num, "%s\n", message);
    return false;
}

#ifndef LUCY_HAS_VARARGS
bool Assert(int condition, const char *fmt, ...)
{
    if (condition) {
        return true;
    }
    else {
        va_list args;
        Tmsg("\n  ?)");
        Tmsg("Assertion failed");
        va_start(args, fmt);
        vTmsg(fmt, args);
        va_end(args);
        return false;
    }
}
#endif

#define USAGE "Invalid option: `%s'\n\
  -v verbose\n\
  -x exclude\n\
  -l list tests\n\
  -f force\n\
  -q quiet\n"

int main(int argc, const char *const argv[])
{
    clock_t time_taken = clock();
    int i;
    int rv;
    int list_provided = 0;
    tst_suite *suite = NULL;
    sub_suite *subsuite;

    setprogname("Lucy Test");

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            verbose = true;
            continue;
        }
        if (!strcmp(argv[i], "-x")) {
            exclude = true;
            continue;
        }
        if (!strcmp(argv[i], "-l")) {
            list_tests = true;
            continue;
        }
        if (!strcmp(argv[i], "-f")) {
            force = true;
            continue;
        }
        if (!strcmp(argv[i], "-q")) {
            quiet = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, USAGE, argv[i]);
            exit(1);
        }
        list_provided = 1;
    }

    if (list_provided) {
        /* Waste a little space here, because it is easier than counting the
         * number of tests listed.  Besides it is not going to be much */
        testlist = calloc(argc + 1, sizeof(char *));
        for (i = 1; i < argc; i++) {
            testlist[i - 1] = (char *) argv[i];
        }
    }

    for (i = 0; i < (int)NELEMS(all_tests); i++) {
        suite = all_tests[i].func(suite);
    }

    /* print out the test diagnotics */
    rv = report(suite);
    printf("Finished in %0.3f seconds\n",
           (double) (clock() - time_taken) / CLOCKS_PER_SEC);

    /* free allocated test suites */
    while ((subsuite = suite->head) != NULL) {
        suite->head = subsuite->next;
        free(subsuite->name);
        free(subsuite);
    }
    free(suite);
    free(testlist);

    return rv;
}
