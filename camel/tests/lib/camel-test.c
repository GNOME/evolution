
#include "camel-test.h"

#include <stdio.h>
#include <signal.h>

struct _stack {
	struct _stack *next;
	char *what;
};

static int setup;
static struct _stack *state;
static struct _stack *nonfatal;
static int ok;

int camel_test_verbose;

static void die(int sig)
{
	static int indie = 0;
	struct _stack *node;

	if (!indie) {
		indie = 1;
		printf("\n\nReceived fatal signal %d\n", sig);
		node = state;
		if (node) {
			printf("Current action:\n");
			while (node) {
				printf("\t%s\n", node->what);
				node = node->next;
			}
		}
	}

	_exit(1);
}

void camel_test_init(int argc, char **argv)
{
	void camel_init(void);
	int i;

	setup = 1;

	camel_init();

	/* yeah, we do need ot thread init, even though camel isn't compiled with enable threads */
	g_thread_init(NULL);

	signal(SIGSEGV, die);
	signal(SIGABRT, die);

	/* default, just say what, how well we did, unless fail, then abort */
	camel_test_verbose = 1;

	for (i=0;i<argc;i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'v':
				camel_test_verbose = strlen(argv[i]);
				break;
			case 'q':
				camel_test_verbose = 0;
				break;
			}
		}
	}
}

void camel_test_start(const char *what)
{
	if (!setup)
		camel_test_init(0, 0);

	ok = 1;

	if (camel_test_verbose > 0) {
		printf("Test: %s ... ", what);
		fflush(stdout);
	}
}

void camel_test_push(const char *what, ...)
{
	struct _stack *node;
	va_list ap;
	char *text;

	va_start(ap, what);
	text = g_strdup_vprintf(what, ap);
	va_end(ap);

	if (camel_test_verbose > 3)
		printf("Start step: %s\n", text);

	node = g_malloc(sizeof(*node));
	node->what = text;
	node->next = state;
	state = node;
}

void camel_test_pull(void)
{
	struct _stack *node;

	g_assert(state);

	if (camel_test_verbose > 3)
		printf("Finish step: %s\n", state->what);

	node = state;
	state = node->next;
	g_free(node->what);
	g_free(node);
}

void camel_test_fail(const char *why, ...)
{
	va_list ap;

	va_start(ap, why);
	camel_test_failv(why, ap);
	va_end(ap);
}

void camel_test_failv(const char *why, va_list ap)
{
	struct _stack *node;
	char *text;

	text = g_strdup_vprintf(why, ap);

	if ((nonfatal == NULL && camel_test_verbose > 0)
	    || (nonfatal && camel_test_verbose > 1)) {
		printf("Failed.\n%s\n", text);
	}

	g_free(text);

	if ((nonfatal == NULL && camel_test_verbose > 0)
	    || (nonfatal && camel_test_verbose > 2)) {
		node = state;
		if (node) {
			printf("Current action:\n");
			while (node) {
				printf("\t%s\n", node->what);
				node = node->next;
			}
		}
	}

	if (nonfatal == NULL) {
		exit(1);
	} else {
		ok=0;
		if (camel_test_verbose > 1) {
			printf("Known problem (ignored): %s\n", nonfatal->what);
		}
	}
}

void camel_test_nonfatal(const char *why, ...)
{
	struct _stack *node;
	va_list ap;
	char *text;

	va_start(ap, why);
	text = g_strdup_vprintf(why, ap);
	va_end(ap);

	if (camel_test_verbose>3)
		printf("Start nonfatal: %s\n", text);

	node = g_malloc(sizeof(*node));
	node->what = text;
	node->next = nonfatal;
	nonfatal = node;
}

void camel_test_fatal(void)
{
	struct _stack *node;

	g_assert(nonfatal);

	if (camel_test_verbose>3)
		printf("Finish nonfatal: %s\n", nonfatal->what);

	node = nonfatal;
	nonfatal = node->next;
	g_free(node->what);
	g_free(node);
}

void camel_test_end(void)
{
	if (camel_test_verbose > 0) {
		if (ok)
			printf("Ok\n");
		else
			printf("Partial success\n");
	}

	fflush(stdout);
}




/* compare strings, ignore whitespace though */
int string_equal(const char *a, const char *b)
{
	const char *ap, *bp;

	ap = a;
	bp = b;

	while (*ap && *bp) {
		while (*ap == ' ' || *ap == '\n' || *ap == '\t')
			ap++;
		while (*bp == ' ' || *bp == '\n' || *bp == '\t')
			bp++;

		a = ap;
		b = bp;

		while (*ap && *ap != ' ' && *ap != '\n' && *ap != '\t')
			ap++;
		while (*bp && *bp != ' ' && *bp != '\n' && *bp != '\t')
			bp++;

		if (ap - a != bp - a
		    && ap - 1 > 0
		    && memcmp(a, b, ap-a) != 0) {
			return 0;
		}
	}

	return 1;
}

