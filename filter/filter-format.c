/* -*- Mode: C; c-file-style: "linux"; indent-tabs-mode: t; c-basic-offset: 8; -*- */

#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include "filter-arg-types.h"
#include "filter-xml.h"

/* FIXME: remove static, this is defined in filter-xml */
static int
filter_find_rule(struct filter_rule *a, char *name)
{
	printf("finding, is %s = %s?\n", a->name, name);
	return strcmp(a->name, name);
}

static int
filter_find_arg(FilterArg *a, char *name)
{
	printf("finding, is %s = %s?\n", a->name, name);
	return strcmp(a->name, name);
}

static int display_order[] = { FILTER_XML_MATCH, FILTER_XML_ACTION, FILTER_XML_EXCEPT };

static struct filter_option *
option_clone(struct filter_option *source)
{
	struct filter_option *dest = g_malloc0(sizeof(*dest));
	GList *loptions;
	struct filter_optionrule *old, *new;

	dest->type = source->type;
	dest->description = source->description;
	loptions = dest->options;
	while (loptions) {
		old = loptions->data;
		new = g_malloc0(sizeof(*new));
		new->rule = old->rule;
		/* FIXME: need to copy any args as well!!! */
		dest->options = g_list_append(dest->options, new);
		loptions = g_list_next(loptions);
	}
	return dest;
}



struct description_decode_lambda {
	GString *str;
	GList *args;
	GtkHTML *html;
	GtkHTMLStreamHandle *stream;
};

static char *
arg_text(FilterArg *arg)
{
	char *out = NULL;
	GList *value, *next;
	GString *str;

	value = arg->values;

	printf("getting text from arg %s\n", arg->name);

	if (value == NULL)
		return NULL;

	str = g_string_new("");
	filter_arg_write_text(arg, str);
	out = str->str;
	g_string_free(str, FALSE);
	return out;
}

static void
description_decode_text(struct filter_desc *d, struct description_decode_lambda *l)
{
	GList *list;
	char *txt;

	switch (d->type) {
	case FILTER_XML_TEXT:
	case FILTER_XML_DESC:
	dotext:
		printf("appending '%s'\n", d->data);
		/* printf("vartype = %s\n", detokenise(d->vartype)); */
		printf("varname = %s\n", d->varname);
		if (d->vartype !=-1 && d->varname
		    && (list = g_list_find_custom(l->args, d->varname, (GCompareFunc) filter_find_arg))
		    && (txt = arg_text(list->data))) {
		} else {
			txt = d->data;
		}
		g_string_append(l->str, txt);
		break;
	default:
		printf("WARN: unknown desc text type '%d' = %s\n", d->type, d->data);
		goto dotext;
	}
}

char *
filter_description_text(GList *description, GList *args)
{
	char *txt;
	struct description_decode_lambda l;

	printf("\ndecoding ...\n");

	l.str = g_string_new("");
	l.args = args;
	g_list_foreach(description, (GFunc) description_decode_text, &l);

	printf("string is '%s'\n", l.str->str);

	txt = l.str->str;
	g_string_free(l.str, FALSE);

	return txt;	
}

static void
html_write(GtkHTML *html, GtkHTMLStreamHandle *stream, char *s)
{
	printf("appending html '%s'\n", s);
	gtk_html_write(html, stream, s, strlen(s));
}


static void
description_decode_html(struct filter_desc *d, struct description_decode_lambda *l)
{
	GList *list;
	char *txt, *end;
	int free;

	switch (d->type) {
	case FILTER_XML_TEXT:
	case FILTER_XML_DESC:
	dotext:
		printf("appending '%s'\n", d->data);
		/*printf("vartype = %s\n", detokenise(d->vartype));*/
		printf("varname = %s\n", d->varname);
		free = FALSE;
		if (d->vartype !=-1 && d->varname) {
			char *link;
			list = g_list_find_custom(l->args, d->varname, (GCompareFunc) filter_find_arg);
			end = "</a>";
			if (list) {
				txt = arg_text(list->data);
				if (txt == NULL)
					txt = d->data;
				else
					free = TRUE;
				link = g_strdup_printf("<a href=\"arg:%p %p\">", d, list->data);
			} else {
				printf("cannot find arg '%s'\n", d->varname);
				link = g_strdup_printf("<a href=\"arg:%p %p\">", d, NULL);
				txt = d->data;
			}
			html_write(l->html, l->stream, link);
			g_free(link);
		} else {
			txt = d->data;
			end = NULL;
		}
		html_write(l->html, l->stream, txt);
		if (end) {
			html_write(l->html, l->stream, end);
		}
		if (free)
			g_free(txt);
		break;
	default:
		/*printf("WARN: unknown desc text type '%s' = %s\n", detokenise(d->type), d->data);*/
		goto dotext;
	}
}

void
filter_description_html_write(GList *description, GList *args, GtkHTML *html, GtkHTMLStreamHandle *stream)
{
	char *txt;
	struct description_decode_lambda l;

	printf("\ndecoding ...\n");

	l.str = NULL;
	l.args = args;
	l.html = html;
	l.stream = stream;
	g_list_foreach(description, (GFunc) description_decode_html, &l);
}

#ifdef TESTER
int main(int argc, char **argv)
{
	GList *rules, *options;
	xmlDocPtr doc, out, optionset, filteroptions;

	gnome_init("Test", "0.0", argc, argv);

	doc = xmlParseFile("filterdescription.xml");

	rules = load_ruleset(doc);
	options = load_optionset(doc, rules);

	out = xmlParseFile("saveoptions.xml");
	options = load_optionset(doc, rules);

	while (options) {
		printf("applying a rule ...\n");
		filterme(options->data);
		options = g_list_next(options);
	}

#if 0
	out = xmlNewDoc("1.0");
	optionset = save_optionset(out, options);
	filteroptions = xmlNewDocNode(out, NULL, "filteroptions", NULL);
	xmlAddChild(filteroptions, optionset);
	xmlDocSetRootElement(out, filteroptions);
	xmlSaveFile("saveoptions.xml", out);
#endif
	return 0;
}
#endif

