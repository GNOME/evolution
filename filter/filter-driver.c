
#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include "filter-arg-types.h"
#include "filter-xml.h"
#include "filter-sexp.h"

extern int filter_find_arg(FilterArg *a, char *name);

/*
  splices ${cc} lines into a single string
*/
int
expand_variables(GString *out, char *source, GList *args, int index)
{
	GList *argl;
	FilterArg *arg;
	char *name= alloca(32);
	char *start, *end, *newstart, *tmp;
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
		argl = g_list_find_custom(args, name, filter_find_arg);
		if (argl) {
			arg = argl->data;
			tmp = g_strdup_printf("%.*s%s", newstart-start, start, filter_arg_get_value_as_string(arg, index));
			printf("appending: %s\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);
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

	g_string_append(s, "(and ");
	optionl = op->options;
	while (optionl) {
		struct filter_optionrule *or = optionl->data;
		if (or->rule->type == FILTER_XML_MATCH) {
			GList *argl;
			int max=1, count;
			int i;

			/* find out how many values we have in each arg (rule
			   is repeated that many times for each arg) */
			argl = or->args;
			while (argl) {
				arg = argl->data;
				count = filter_arg_get_count(arg);
				if (count>=max && max>1) {
					g_warning("Rule '%s' has too many multi-valued values, ignored", or->rule->name);
					goto next_rule;
				}
				if (count>max) {
					max = count;
				}
				argl = g_list_next(argl);
			}
			g_string_append(s, "(or ");
			for (i=0;i<max;i++) {
				expand_variables(s, or->rule->code, or->args, i);
			}
			g_string_append(s, ") ");
		}
	next_rule:
		optionl = g_list_next(optionl);
	}
#if 0
	optionl = op->options;
	while (optionl) {
		struct filter_optionrule *or = optionl->data;
		if (or->rule->type == FILTER_XML_EXCEPT) {
			g_string_append(s, " (except \"");
			g_string_append(s, or->rule->name);
			g_string_append(s, "\" ");
			g_string_append(s, or->rule->code);
			g_string_append(s, " ) ");
		}
		optionl = g_list_next(optionl);
	}
#endif
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

int main(int argc, char **argv)
{
	FilterSEXP *f;
	FilterSEXPResult *r;
	GList *rules, *options;
	xmlDocPtr doc, out, optionset, filteroptions;
	GString *s;

	gnome_init("Test", "0.0", argc, argv);

	doc = xmlParseFile("filterdescription.xml");
	rules = load_ruleset(doc);
	options = load_optionset(doc, rules);
	out = xmlParseFile("saveoptions.xml");
	options = load_optionset(out, rules);

	s = g_string_new("");
	expand_filter_option(s, options->data);
	g_string_append(s, "");

	printf("total rule = '%s'\n", s->str);

	f = filter_sexp_new();
	filter_sexp_add_variable(f, 0, "sender", NULL);
	filter_sexp_add_variable(f, 0, "receipient", NULL);
	filter_sexp_add_variable(f, 0, "folder", NULL);

	/* simple functions */
	filter_sexp_add_function(f, 0, "header-get", NULL, NULL);
	filter_sexp_add_function(f, 0, "header-contains", NULL, NULL);
	filter_sexp_add_function(f, 0, "copy-to", NULL, NULL);

	filter_sexp_add_ifunction(f, 0, "set", NULL, NULL);

	/* control functions */
	filter_sexp_add_ifunction(f, 0, "match-all", NULL, NULL);
	filter_sexp_add_ifunction(f, 0, "match", NULL, NULL);
	filter_sexp_add_ifunction(f, 0, "action", NULL, NULL);
	filter_sexp_add_ifunction(f, 0, "except", NULL, NULL);

	filter_sexp_input_text(f, s->str, strlen(s->str));
	filter_sexp_parse(f);
	
}
