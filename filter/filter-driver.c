
#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include "filter-arg-types.h"
#include "filter-xml.h"
#include "e-sexp.h"
#include "filter-format.h"

#include "camel.h"
#include "camel-mbox-folder.h"
#include "camel-mbox-parser.h"
#include "camel-mbox-utils.h"
#include "camel-mbox-summary.h"
#include "camel-log.h"
#include "camel-exception.h"
#include "camel-folder-summary.h"

extern int filter_find_arg(FilterArg *a, char *name);

struct exec_context {
	GHashTable *globals;	/* global variables */

	GList *matches;		/* all messages which match current rule */

	GList *deleted;		/* messages to be deleted */
	GHashTable *terminated;	/* messages for which processing is terminated */
	GHashTable *processed;	/* all messages that were processed in some way */

	CamelSession *session;
	CamelStore *store;
	CamelFolder *folder;	/* temporary input folder */
	CamelException *ex;
};

/*

  foreach rule
    find matches

  foreach action
    get all matches

 */

/*
  splices ${cc} lines into a single string
*/
int
expand_variables(GString *out, char *source, GList *args, GHashTable *globals)
{
	GList *argl;
	FilterArg *arg;
	char *name= alloca(32);
	char *start, *end, *newstart, *tmp, *val;
	int namelen=32;
	int len=0;
	int ok = 0;

	printf("expanding %s\n", source);

	start = source;
	while ( (newstart = strstr(start, "${"))
		&& (end = strstr(newstart+2, "}")) ) {
		len = end-newstart-2;
		if (len+1>namelen) {
			namelen = (len+1)*2;
			name = alloca(namelen);
		}
		memcpy(name, newstart+2, len);
		name[len] = 0;
		printf("looking for name '%s'\n", name);
		argl = g_list_find_custom(args, name, (GCompareFunc) filter_find_arg);
		if (argl) {
			int i, count;

			tmp = g_strdup_printf("%.*s", newstart-start, start);
			printf("appending: %s\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);

			arg = argl->data;
			count = filter_arg_get_count(arg);
			for (i=0;i<count;i++) {
				printf("appending '%s'\n", filter_arg_get_value_as_string(arg, i));
				g_string_append(out, " \"");
				g_string_append(out, filter_arg_get_value_as_string(arg, i));
				g_string_append(out, "\"");
			}
		} else if ( (val = g_hash_table_lookup(globals, name)) ) {
			tmp = g_strdup_printf("%.*s", newstart-start, start);
			printf("appending: %s\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);
			g_string_append(out, " \"");
			g_string_append(out, val);
			g_string_append(out, "\"");
		} else {
			ok = 1;
			tmp = g_strdup_printf("%.*s", end-start+1, start);
			printf("appending: '%s'\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);
		}
		start = end+1;
	}
	g_string_append(out, start);

	return ok;
}

/*
  build an expression for the filter
*/
static void
expand_filter_option(GString *s, GString *action, struct filter_option *op)
{
	GList *optionl;
	FilterArg *arg;
	GHashTable *globals;

	globals = g_hash_table_new(g_str_hash, g_str_equal);

	g_hash_table_insert(globals, "self-email", "mzucchi@dehaa.sa.gov.au");

	g_string_append(s, "(and ");
	optionl = op->options;
	while (optionl) {
		struct filter_optionrule *or = optionl->data;
		if (or->rule->type == FILTER_XML_MATCH
		    || or->rule->type == FILTER_XML_EXCEPT) {
			if (or->args) {
				arg = or->args->data;
				if (arg) {
					printf("arg = %s\n", arg->name);
				}
			}
			expand_variables(s, or->rule->code, or->args, globals);
		}
		optionl = g_list_next(optionl);
	}

	g_string_append(s, ")");

	g_string_append(action, "(begin ");
	optionl = op->options;
	while (optionl) {
		struct filter_optionrule *or = optionl->data;
		if (or->rule->type == FILTER_XML_ACTION) {
			expand_variables(action, or->rule->code, or->args, globals);
			g_string_append(action, " ");
		}
		optionl = g_list_next(optionl);
	}
	g_string_append(action, ")");

	printf("combined rule '%s'\n", s->str);
	printf("combined action '%s'\n", action->str);
}

struct filter_optionrule *
find_optionrule(struct filter_option *option, char *name)
{
	GList *optionrulel;
	struct filter_optionrule *or;
	
	optionrulel = option->options;
	while (optionrulel) {
		or = optionrulel->data;
		if (!strcmp(or->rule->name, name)) {
			return or;
		}
		optionrulel = g_list_next(optionrulel);
	}
	return NULL;
}

static ESExpResult *
do_delete(struct _ESExp *f, int argc, struct _ESExpResult **argv, struct exec_context *x)
{
	GList *m;

	printf("doing delete\n");
	m = x->matches;
	while (m) {
		printf(" %s\n", m->data);
		x->deleted = g_list_append(x->deleted, g_strdup(m->data));
		m = m->next;
	}
	return NULL;
}

static ESExpResult *
do_forward(struct _ESExp *f, int argc, struct _ESExpResult **argv, struct exec_context *x)
{
	GList *m;

	printf("doing forward on the following messages:\n");
	m = x->matches;
	while (m) {
		printf(" %s\n", m->data);
		m = m->next;
	}
	return NULL;
}

static ESExpResult *
do_copy(struct _ESExp *f, int argc, struct _ESExpResult **argv, struct exec_context *x)
{
	GList *m;
	int i;

	printf("doing copy on the following messages to:");
	for (i=0;i<argc;i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			char *folder = argv[i]->value.string;
			CamelFolder *outbox;

			/* FIXME: this might have to find another store, based on
			   the folder as a url??? */
			printf("opening outpbox %s\n", folder);
			outbox = camel_store_get_folder (x->store, folder, x->ex);
			if (!camel_folder_exists(outbox, x->ex)) {
				camel_folder_create(outbox, x->ex);
			}
			
			camel_folder_open (outbox, FOLDER_OPEN_WRITE, x->ex);

			m = x->matches;
			while (m) {
				CamelMimeMessage *mm;

				printf("appending message %s\n", m->data);

				mm = camel_folder_get_message_by_uid(x->folder, m->data, x->ex);
				camel_folder_append_message(outbox, mm, x->ex);
				gtk_object_unref((GtkObject *)mm);

				printf(" %s\n", m->data);
				m = m->next;
			}
			camel_folder_close (outbox, FALSE, x->ex);
		}
	}

	return NULL;
}

static ESExpResult *
do_stop(struct _ESExp *f, int argc, struct _ESExpResult **argv, struct exec_context *x)
{
	GList *m;

	printf("doing stop on the following messages:\n");
	m = x->matches;
	while (m) {
		printf(" %s\n", m->data);
		g_hash_table_insert(x->terminated, g_strdup(m->data), (void *)1);
		m = m->next;
	}
	return NULL;
}

static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "delete", (ESExpFunc *)do_delete, 0 },
	{ "forward-to", (ESExpFunc *)do_forward, 0 },
	{ "copy-to", (ESExpFunc *)do_copy, 0 },
	{ "stop", (ESExpFunc *)do_stop, 0 },
};

static char *
auth_callback(char *prompt, gboolean secret,
	      CamelService *service, char *item,
	      CamelException *ex)
{
	printf ("auth_callback called: %s\n", prompt);
	return NULL;
}

static struct exec_context *
start(void)
{
	struct exec_context *x;
	char *store_url = "mbox:///tmp/evmail";

	x = g_malloc0(sizeof(*x));

	/* just hack up this for now */
	x->ex = camel_exception_new ();
	camel_provider_register_as_module ("../camel/providers/mbox/.libs/libcamelmbox.so.0");
	x->session = camel_session_new (auth_callback);
	x->store = camel_session_get_store (x->session, store_url, x->ex);
	x->folder = camel_store_get_folder (x->store, "Inbox", x->ex);
	camel_folder_open (x->folder, FOLDER_OPEN_READ, x->ex);
	x->terminated = g_hash_table_new(g_str_hash, g_str_equal);
	x->processed = g_hash_table_new(g_str_hash, g_str_equal);
	return x;
}

static void
search_cb(CamelFolder *f, int id, gboolean complete, GList *matches, struct exec_context *x)
{
	printf("appending matches ...\n");
	x->matches =  g_list_concat(x->matches, g_list_copy(matches));
}

int main(int argc, char **argv)
{
	ESExp *f;
	ESExpResult *r;
	GList *rules, *options, *options2;
	xmlDocPtr doc, out;
	GString *s, *a;
	GList *all, *m;
	struct exec_context *x;
	int i;
	ESExp *eval;

	gnome_init("Test", "0.0", argc, argv);
	camel_init();

	doc = xmlParseFile("filterdescription.xml");
	rules = filter_load_ruleset(doc);
	options2 = filter_load_optionset(doc, rules);

	out = xmlParseFile("saveoptions.xml");
	options = filter_load_optionset(out, rules);

	x = start();

	eval = e_sexp_new();
	/* Load in builtin symbols? */
	for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction(eval, 0, symbols[i].name, (ESExpIFunc *)symbols[i].func, x);
		} else {
			e_sexp_add_function(eval, 0, symbols[i].name, symbols[i].func, x);
		}
	}

	while (options) {
		struct filter_option *fo = options->data;
		int id;

		s = g_string_new("");
		a = g_string_new("");
		expand_filter_option(s, a, fo);

		printf("searching expression %s\n", s->str);
		x->matches= NULL;
		id = camel_folder_search_by_expression  (x->folder, s->str, search_cb, x, x->ex);

		/* wait for it to finish */
		camel_folder_search_complete(x->folder, id, TRUE, x->ex);
		
		/* remove uid's for which processing is complete ... */
		m = x->matches;
		while (m) {
			GList *n = m->next;

			/* for all matching id's, so we can work out what to default */
			if (g_hash_table_lookup(x->processed, m->data) == NULL) {
				g_hash_table_insert(x->processed, g_strdup(m->data), (void *)1);
			}

			if (g_hash_table_lookup(x->terminated, m->data)) {
				printf("removing terminated message %s\n", m->data);
				x->matches = g_list_remove_link(x->matches, m);
			}
			m = n;
		}

		printf("applying actions ... '%s'\n", a->str);
		e_sexp_input_text(eval, a->str, strlen(a->str));
		e_sexp_parse(eval);
		r = e_sexp_eval(eval);
		e_sexp_result_free(r);

		g_string_free(s, TRUE);
		g_string_free(a, TRUE);

		g_list_free(x->matches);
		
		options = g_list_next(options);
	}

	/* now apply 'default' rule */
	all = camel_folder_get_uid_list(x->folder, x->ex);
	m = all;
	while (m) {
		char *uid = m->data;
		if (g_hash_table_lookup(x->processed, uid) == NULL) {
			printf("Applying default rule to message %s\n", uid);
		}
		m = m->next;
	}
	g_list_free(all);

	return 0;
}
