
#include "camel-test.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <camel/camel.h>

/* well i dunno, doesn't seem to be in the headers but hte manpage mentions it */
/* a nonportable checking mutex for glibc, not really needed, just validates
   the test harness really */
# ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
static pthread_mutex_t lock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
# else
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
# endif
#define CAMEL_TEST_LOCK pthread_mutex_lock(&lock)
#define CAMEL_TEST_UNLOCK pthread_mutex_unlock(&lock)
#define CAMEL_TEST_ID (pthread_self())

static int setup;
static int ok;

struct _stack {
	struct _stack *next;
	int fatal;
	char *what;
};

/* per-thread state */
struct _state {
	char *test;
	int nonfatal;
	struct _stack *state;
};

static GHashTable *info_table;

int camel_test_verbose;

static void
dump_action(int id, struct _state *s, void *d)
{
	struct _stack *node;
	
	printf("\nThread %d:\n", id);
	
	node = s->state;
	if (node) {
		printf("Current action:\n");
		while (node) {
			printf("\t%s%s\n", node->fatal?"":"[nonfatal]", node->what);
			node = node->next;
		}
	}
	printf("\tTest: %s\n", s->test);
}

static void die(int sig)
{
	static int indie = 0;

	if (!indie) {
		indie = 1;
		printf("\n\nReceived fatal signal %d\n", sig);
		g_hash_table_foreach(info_table, (GHFunc)dump_action, 0);
		
		if (camel_test_verbose > 2) {
			printf("Attach debugger to pid %d to debug\n", getpid());
			sleep(1000);
		}
	}

	_exit(1);
}

static struct _state *
current_state(void)
{
	struct _state *info;

	if (info_table == NULL)
		info_table = g_hash_table_new(0, 0);

	info = g_hash_table_lookup(info_table, (void *)CAMEL_TEST_ID);
	if (info == NULL) {
		info = g_malloc0(sizeof(*info));
		g_hash_table_insert(info_table, (void *)CAMEL_TEST_ID, info);
	}
	return info;
}
	

void camel_test_init(int argc, char **argv)
{
	struct stat st;
	char *path;
	int i;
	
	setup = 1;
	
	/* yeah, we do need ot thread init, even though camel isn't compiled with enable threads */
	g_thread_init (NULL);
	
	path = g_strdup_printf ("/tmp/camel-test");
	if (mkdir (path, 0700) == -1 && errno != EEXIST)
		abort ();
	
	if (stat (path, &st) == -1)
		abort ();
	
	if (!S_ISDIR (st.st_mode) || access (path, R_OK | W_OK | X_OK) == -1)
		abort ();
	
	camel_init (path, FALSE);
	g_free (path);
	
	camel_type_init ();
	
	info_table = g_hash_table_new(0, 0);
	
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
	struct _state *s;

	CAMEL_TEST_LOCK;

	s = current_state();

	if (!setup)
		camel_test_init(0, 0);

	ok = 1;

	s->test = g_strdup(what);

	if (camel_test_verbose > 0) {
		printf("Test: %s ... ", what);
		fflush(stdout);
	}

	CAMEL_TEST_UNLOCK;
}

void camel_test_push(const char *what, ...)
{
	struct _stack *node;
	va_list ap;
	char *text;
	struct _state *s;

	CAMEL_TEST_LOCK;

	s = current_state();

	va_start(ap, what);
	text = g_strdup_vprintf(what, ap);
	va_end(ap);

	if (camel_test_verbose > 3)
		printf("Start step: %s\n", text);

	node = g_malloc(sizeof(*node));
	node->what = text;
	node->next = s->state;
	node->fatal = 1;
	s->state = node;

	CAMEL_TEST_UNLOCK;
}

void camel_test_pull(void)
{
	struct _stack *node;
	struct _state *s;

	CAMEL_TEST_LOCK;

	s = current_state();

	g_assert(s->state);

	if (camel_test_verbose > 3)
		printf("Finish step: %s\n", s->state->what);

	node = s->state;
	s->state = node->next;
	if (!node->fatal)
		s->nonfatal--;
	g_free(node->what);
	g_free(node);

	CAMEL_TEST_UNLOCK;
}

/* where to set breakpoints */
void camel_test_break(void);

void camel_test_break(void)
{
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
	char *text;
	struct _state *s;

	CAMEL_TEST_LOCK;

	s = current_state();

	text = g_strdup_vprintf(why, ap);

	if ((s->nonfatal == 0 && camel_test_verbose > 0)
	    || (s->nonfatal && camel_test_verbose > 1)) {
		printf("Failed.\n%s\n", text);
		camel_test_break();
	}

	g_free(text);

	if ((s->nonfatal == 0 && camel_test_verbose > 0)
	    || (s->nonfatal && camel_test_verbose > 2)) {
		g_hash_table_foreach(info_table, (GHFunc)dump_action, 0);
	}

	if (s->nonfatal == 0) {
		exit(1);
	} else {
		ok=0;
		if (camel_test_verbose > 1) {
			printf("Known problem (ignored):\n");
			dump_action(CAMEL_TEST_ID, s, 0);
		}
	}

	CAMEL_TEST_UNLOCK;
}

void camel_test_nonfatal(const char *what, ...)
{
	struct _stack *node;
	va_list ap;
	char *text;
	struct _state *s;

	CAMEL_TEST_LOCK;

	s = current_state();

	va_start(ap, what);
	text = g_strdup_vprintf(what, ap);
	va_end(ap);

	if (camel_test_verbose > 3)
		printf("Start nonfatal: %s\n", text);

	node = g_malloc(sizeof(*node));
	node->what = text;
	node->next = s->state;
	node->fatal = 0;
	s->nonfatal++;
	s->state = node;

	CAMEL_TEST_UNLOCK;
}

void camel_test_fatal(void)
{
	camel_test_pull();
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

