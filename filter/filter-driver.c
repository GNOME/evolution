
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
			printf("appending: %s\n", tmp);
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
expand_filter_option(GString *s, struct filter_option *op)
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
			expand_variables(s, or->rule->code, or->args, globals);
		}
		optionl = g_list_next(optionl);
	}

	g_string_append(s, ")");
#if 0
	optionl = op->options;
	while (optionl) {
		struct filter_optionrule *or = optionl->data;
		if (or->rule->type == FILTER_XML_ACTION) {
			g_string_append(s, or->rule->code);
			g_string_append(s, " ");
		}
		optionl = g_list_next(optionl);
	}
	g_string_append(s, ")))");
#endif
	printf("combined rule '%s'\n", s->str);
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

int main(int argc, char **argv)
{
	ESExp *f;
	ESExpResult *r;
	GList *rules, *options, *options2;
	xmlDocPtr doc, out, optionset, filteroptions;
	GString *s;

	gnome_init("Test", "0.0", argc, argv);
#if 0
	gdk_rgb_init ();
	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	create_dialogue();
#endif

	doc = xmlParseFile("filterdescription.xml");
	rules = filter_load_ruleset(doc);
	options = filter_load_optionset(doc, rules);
	options2 = options;
	out = xmlParseFile("saveoptions.xml");
	options = filter_load_optionset(out, rules);

#if 0
#if 0
	option_current = options->data;
	fill_rules(list_global, rules, options->data, FILTER_XML_MATCH);
#else
	option_current = NULL;
	fill_options(list_global, options2);
#endif
	gtk_main();

	while (options) {
		struct filter_option *fo = options->data;
		GList *optionrulel;

		optionrulel = fo->options;
		while (optionrulel) {
			struct filter_optionrule *or = optionrulel->data;

			printf("formatting rule: %s\n", or->rule->name);

			/*filter_description_text(or->rule->description, or->args);*/
			filter_description_html_write(or->rule->description, or->args, NULL, NULL);

			optionrulel = g_list_next(optionrulel);
		}
		options = g_list_next(options);
	}

	return 0;
#endif

	s = g_string_new("");
	expand_filter_option(s, options->data);
	g_string_append(s, "");

	printf("total rule = '%s'\n", s->str);

	f = e_sexp_new();
	e_sexp_add_variable(f, 0, "sender", NULL);
	e_sexp_add_variable(f, 0, "receipient", NULL);
	e_sexp_add_variable(f, 0, "folder", NULL);

	/* simple functions */
	e_sexp_add_function(f, 0, "header-get", NULL, NULL);
	e_sexp_add_function(f, 0, "header-contains", NULL, NULL);
	e_sexp_add_function(f, 0, "copy-to", NULL, NULL);

	e_sexp_add_ifunction(f, 0, "set", NULL, NULL);

	/* control functions */
	e_sexp_add_ifunction(f, 0, "match-all", NULL, NULL);
	e_sexp_add_ifunction(f, 0, "match", NULL, NULL);
	e_sexp_add_ifunction(f, 0, "action", NULL, NULL);
	e_sexp_add_ifunction(f, 0, "except", NULL, NULL);

	e_sexp_input_text(f, s->str, strlen(s->str));
	e_sexp_parse(f);
	
}
