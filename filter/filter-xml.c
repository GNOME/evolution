/* -*- Mode: C; c-file-style: "linux"; indent-tabs-mode: t; c-basic-offset: 8; -*- */

#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include "filter-arg-types.h"
#include "filter-xml.h"

struct token_tab {
	char *name;
	enum filter_xml_token token;
};

struct token_tab token_table[] = {
	{ "action", FILTER_XML_ACTION },
	{ "address", FILTER_XML_ADDRESS },
	{ "code", FILTER_XML_CODE },
	{ "description", FILTER_XML_DESC },
	{ "except", FILTER_XML_EXCEPT },
	{ "folder", FILTER_XML_FOLDER },
	{ "match", FILTER_XML_MATCH },
	{ "name", FILTER_XML_NAME },
	{ "option", FILTER_XML_OPTION },
	{ "optionrule", FILTER_XML_OPTIONRULE },
	{ "optionset", FILTER_XML_OPTIONSET },
	{ "optionvalue", FILTER_XML_OPTIONVALUE },
	{ "receive", FILTER_XML_RECEIVE },
	{ "rule", FILTER_XML_RULE },
	{ "ruleset", FILTER_XML_RULESET },
	{ "send", FILTER_XML_SEND },
	{ "source", FILTER_XML_SOURCE },
	{ "text", FILTER_XML_TEXT },
};

/* convert a name to a token value */
static int
tokenise(const char *name)
{
	int i;
	int len = sizeof(token_table)/sizeof(token_table[0]);

	if (name) {
		for (i=0;i<len;i++) {
			if (strcmp(name, token_table[i].name) == 0)
				return token_table[i].token;
		}
	}
	return -1;
}

static char *
detokenise(int token)
{
	int i;
	int len = sizeof(token_table)/sizeof(token_table[0]);

	if (token>=0) {
		for (i=0;i<len;i++) {
			if (token_table[i].token == token)
				return token_table[i].name;
		}
	}
	return "<unknown>";
}


xmlNodePtr find_node(xmlNodePtr start, char *name)
{
	printf("trying to find node '%s'\n", name);
	while (start && strcmp(start->name, name))
		start = start->next;
	printf("node = %p\n", start);
	return start;
}

xmlNodePtr find_node_attr(xmlNodePtr start, char *name, char *attrname, char *attrvalue)
{
	xmlNodePtr node;
	char *s;

	printf("looking for node named %s with attribute %s=%s\n", name, attrname, attrvalue);

	while (	start && (start = find_node(start, name)) ) {
		s = xmlGetProp(start, attrname);
		printf("   comparing '%s' to '%s'\n", s, attrvalue);
		if (s && !strcmp(s, attrvalue))
			break;
		start = start->next;
	}
	return start;
}

static int
find_arg(FilterArg *a, char *name)
{
	printf("finding, is %s = %s?\n", a->name, name);
	return strcmp(a->name, name);
}

static int
find_rule(struct filter_rule *a, char *name)
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
		printf("vartype = %s\n", detokenise(d->vartype));
		printf("varname = %s\n", d->varname);
		if (d->vartype !=-1 && d->varname
		    && (list = g_list_find_custom(l->args, d->varname, (GCompareFunc) find_arg))
		    && (txt = arg_text(list->data))) {
		} else {
			txt = d->data;
		}
		g_string_append(l->str, txt);
		break;
	default:
		printf("WARN: unknown desc text type '%s' = %s\n", detokenise(d->type), d->data);
		goto dotext;
	}
}

static char *
description_text(GList *description, GList *args)
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
		printf("vartype = %s\n", detokenise(d->vartype));
		printf("varname = %s\n", d->varname);
		free = FALSE;
		if (d->vartype !=-1 && d->varname) {
			char *link;
			list = g_list_find_custom(l->args, d->varname, (GCompareFunc) find_arg);
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
		printf("WARN: unknown desc text type '%s' = %s\n", detokenise(d->type), d->data);
		goto dotext;
	}
}

static void
description_html_write(GList *description, GList *args, GtkHTML *html, GtkHTMLStreamHandle *stream)
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


static GList *
load_desc(xmlNodePtr node, int type, int vartype, char *varname)
{
	struct filter_desc *desc;
	xmlNodePtr n;
	int newtype;
	int newvartype;
	char *newvarname;
	GList *list = NULL;

	while (node) {
		if (node->content) {
			desc = g_malloc0(sizeof(*desc));
			desc->data = node->content;
			desc->type = type;
			desc->vartype = vartype;
			desc->varname = varname?g_strdup(varname):0;
			printf(" **** node name = %s var name = %s  var type = %s\n", node->name, varname, detokenise(vartype));
			list = g_list_append(list, desc);
			printf("appending '%s'\n", node->content);
			newtype = type;
			newvartype = -1;
			newvarname = NULL;
		} else {
			newtype = tokenise(node->name);
			newvartype = tokenise(xmlGetProp(node, "type"));
			newvarname = xmlGetProp(node, "name");
		}
		n = node->childs;
		while (n) {
			printf("adding child '%s'\n", n->name);
			list = g_list_concat(list, load_desc(n, newtype, newvartype, newvarname));
			n = n->next;
		}
		node = node->next;
	}
	return list;
}

GList *
load_ruleset(xmlDocPtr doc)
{
	xmlNodePtr ruleset, rule, n;
	struct filter_rule *r;
	int type;
	int ruletype;
	GList *rules = NULL;

	g_return_val_if_fail(doc!=NULL, NULL);

	ruleset = find_node(doc->root->childs, "ruleset");

	while (ruleset) {

		rule = ruleset->childs;
		
		ruletype = tokenise(xmlGetProp(ruleset, "type"));

		printf("ruleset, name = %s\n", ruleset->name);

		while (rule) {

			n = rule->childs;
			r = g_malloc0(sizeof(*r));
			r->type = ruletype;
			r->name = xmlGetProp(rule, "name");

			printf(" rule, name = %s\n", r->name);

			while (n) {
				type = tokenise(n->name);
				printf("  n, name = %s\n", n->name);
				printf("  ncontent = %s\n", n->content);
				printf("  childs = %p\n", n->childs);
				if (n->childs) {
					printf(" childs content = %s\n", n->childs->content);
				}
				switch(type) {
				case FILTER_XML_CODE:
					r->code = xmlNodeGetContent(n);
					break;
				case FILTER_XML_DESC:
					printf(" ** loading description\n");
					r->description = load_desc(n->childs, type, -1, NULL);
					printf(" ** done loading description\n");
					description_text(r->description, 0);
					printf(" ** done dumping description\n");
					break;
				default:
					printf("warning, unknown token encountered\n");
					break;
				}
				n = n->next;
			}
			if (r)
				rules = g_list_append(rules, r);
			rule = rule->next;
		}
		ruleset = find_node(ruleset->next, "ruleset");
	}
	return rules;
}

int
filter_find_rule(struct filter_rule *a, char *name)
{
	printf("finding, is %s = %s?\n", a->name, name);
	return strcmp(a->name, name);
}

int
filter_find_arg(FilterArg *a, char *name)
{
	printf("finding, is %s = %s?\n", a->name, name);
	return strcmp(a->name, name);
}

static FilterArg *
load_optionvalue(struct filter_desc *desc, xmlNodePtr node)
{
	xmlNodePtr n;
	int token;
	int lasttoken = -2;
	FilterArg *arg = NULL;
	char *name;

	printf("creating arg entry for '%s'\n", desc->varname);

	switch(desc->vartype) {
	case FILTER_XML_ADDRESS:
		arg = filter_arg_address_new(name);
		break;
	case FILTER_XML_FOLDER:
		arg = filter_arg_folder_new(name);
		break;
	default:
		printf("ok, maybe we're not\n");
		/* unknown arg type, drop it */
		return NULL;
	}

	if (node == NULL)
		return arg;

	filter_arg_values_add_xml(arg, node);

	return arg;
}

/* loads a blank (empty args) optionrule from a rule */
static struct filter_optionrule *
optionrule_new(struct filter_rule *rule)
{
	GList *ldesc;
	struct filter_desc *desc;
	struct filter_optionrule *optionrule;

	optionrule = g_malloc0(sizeof(*optionrule));
	optionrule->rule = rule;

	ldesc = rule->description;
	while (ldesc) {
		desc = ldesc->data;
		if (desc->varname && desc->vartype!=-1) {
			FilterArg *arg;
			arg = load_optionvalue(desc, NULL);
			if (arg)
				optionrule->args = g_list_append(optionrule->args, arg);
		}
		ldesc = g_list_next(ldesc);
	}
	return optionrule;
}

GList *
load_optionset(xmlDocPtr doc, GList *rules)
{
	xmlNodePtr optionset, option, o, or;
	struct filter_option *op;
	struct filter_optionrule *optionrule;
	struct filter_rule *fr;
	struct filter_desc *desc;
	int type, token;
	GList *l = NULL;
	GList *lrule;
	GList *ldesc;

	g_return_val_if_fail(doc!=NULL, NULL);

	optionset = find_node(doc->root->childs, "optionset");
	if (optionset == NULL) {
		printf("optionset not found\n");
		return;
	}
	option = find_node(optionset->childs, "option");
	while (option) {
		o = option->childs;
		op = g_malloc0(sizeof(*op));
		printf("option = %s\n", o->name);
		printf("option, type=%s\n", xmlGetProp(option, "type"));
		op->type = tokenise(xmlGetProp(option, "type"));
		while (o) {
			type = tokenise(o->name);
			switch (type) {
			case FILTER_XML_OPTIONRULE:
				lrule = g_list_find_custom(rules, xmlGetProp(o, "rule"), (GCompareFunc) filter_find_rule);
				if (lrule) {
					fr = lrule->data;
					printf("found rule : %s\n", fr->name);
					optionrule = g_malloc0(sizeof(*optionrule));
					optionrule->rule = fr;
					op->options = g_list_append(op->options, optionrule);

					/* scan through all variables required, setup blank variables if they do not exist */
					ldesc = fr->description;
					while (ldesc) {
						desc = ldesc->data;
						if (desc->varname && desc->vartype!=-1) {
							FilterArg *arg;
							/* try and see if there is a setting for this value */
							or = find_node_attr(o->childs, "optionvalue", "name", desc->varname);
							arg = load_optionvalue(desc, or);
							if (arg)
								optionrule->args = g_list_append(optionrule->args, arg);
						}
						ldesc = g_list_next(ldesc);
					}
				} else {
					printf("Cannot find rule: %s\n", xmlGetProp(o, "rule"));
				}
				break;
			case FILTER_XML_DESC:
				printf("loading option descriptiong\n");
				op->description = load_desc(option->childs, type, -1, NULL);
				break;
			}
			o = o->next;
		}
		l = g_list_append(l, op);
		option = find_node(option->next, "option");
	}
	return l;
}

static xmlNodePtr
save_optionset(xmlDocPtr doc, GList *optionl)
{
	xmlNodePtr root, cur, option, optionrule, optionvalue;
	GList *optionrulel, *argl;
	struct filter_optionrule *or;

	root = xmlNewDocNode(doc, NULL, "optionset", NULL);

	/* for all options */
	while (optionl) {
		struct filter_option *op = optionl->data;

		option = xmlNewDocNode(doc, NULL, "option", NULL);
		xmlSetProp(option, "type", detokenise(op->type));

		optionrulel = op->options;
		while (optionrulel) {
			or = optionrulel->data;

			optionrule = xmlNewDocNode(doc, NULL, "optionrule", NULL);
			xmlSetProp(optionrule, "type", detokenise(or->rule->type));
			xmlSetProp(optionrule, "rule", or->rule->name);

			argl = or->args;
			while (argl) {
				FilterArg *arg = argl->data;

				optionvalue = filter_arg_values_get_xml(arg);
				if (optionvalue)
					xmlAddChild(optionrule, optionvalue);

				argl = g_list_next(argl);
			}

			xmlAddChild(option, optionrule);

			optionrulel = g_list_next(optionrulel);
		}

		xmlAddChild(root, option);
		optionl = g_list_next(optionl);
	}

	return root;
}



/*
  build an expression for the filter
*/
static void
filterme(struct filter_option *op)
{
	GList *optionl;
	GString *s;

	s = g_string_new("(if (and ");
	optionl = op->options;
	while (optionl) {
		struct filter_optionrule *or = optionl->data;
		if (or->rule->type == FILTER_XML_MATCH) {
			g_string_append(s, "(match \"");
			g_string_append(s, or->rule->name);
			g_string_append(s, "\" ");
			g_string_append(s, or->rule->code);
			g_string_append(s, ") ");
		}
		optionl = g_list_next(optionl);
	}
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
	g_string_append(s, ") (begin ");
	optionl = op->options;
	while (optionl) {
		struct filter_optionrule *or = optionl->data;
		if (or->rule->type == FILTER_XML_ACTION) {
			g_string_append(s, or->rule->code);
			g_string_append(s, " ");
		}
		optionl = g_list_next(optionl);
	}
	g_string_append(s, "))");
	printf("combined rule '%s'\n", s->str);
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

