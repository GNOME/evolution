
/* some utilities for testing */

#include "config.h"

#include <stdlib.h>
#include <glib.h>

void camel_test_failv(const char *why, va_list ap);

/* perform a check assertion */
#define check(x) do {if (!(x)) { camel_test_fail("%s:%d: %s", __FILE__, __LINE__, #x); } } while (0)
/* check with message */
#ifdef  __GNUC__
#define check_msg(x, y, z...) do {if (!(x)) { camel_test_fail("%s:%d: %s\n\t" #y, __FILE__, __LINE__, #x, ##z); } } while (0)
#else
static void check_msg(int truth, char *fmt, ...)
{
	/* no gcc, we lose the condition that failed, nm */
	if (!truth) {
		va_list ap;
		va_start(ap, fmt);
		camel_test_failv(fmt, ap);
		va_end(ap);
	}
}
#endif

#define check_count(object, expected) do { \
	if (CAMEL_OBJECT(object)->ref_count != expected) { \
		camel_test_fail("%s->ref_count != %s\n\tref_count = %d", #object, #expected, CAMEL_OBJECT(object)->ref_count); \
	} \
} while (0)

#define check_unref(object, expected) do { \
	check_count(object, expected); \
	camel_object_unref(CAMEL_OBJECT(object)); \
	if (expected == 1) { \
		object = NULL; \
	} \
} while (0)

#define test_free(mem) (g_free(mem), mem=NULL)

#define push camel_test_push
#define pull camel_test_pull

void camel_test_init(int argc, char **argv);

/* start/finish a new test */
void camel_test_start(const char *what);
void camel_test_end(void);

/* start/finish a new test part */
void camel_test_push(const char *what, ...);
void camel_test_pull(void);

/* fail a test, with a reason why */
void camel_test_fail(const char *why, ...);
void camel_test_failv(const char *why, va_list ap);

/* Set whether a failed test quits.  May be nested, but must be called in nonfatal/fatal pairs  */
void camel_test_nonfatal(const char *why, ...);
void camel_test_fatal(void);

/* utility functions */
/* compare strings, ignore whitespace though */
int string_equal(const char *a, const char *b);
