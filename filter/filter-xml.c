/* -*- Mode: C; c-file-style: "linux"; indent-tabs-mode: t; c-basic-offset: 8; -*- */

/* Load save filter descriptions/options from an xml file */

#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include "filter-arg-types.h"
#include "filter-xml.h"

#define d(x)

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

static int
tokenise_xmlfreeprop(const char *name)
{
	int ret = -1;
	ret = tokenise(name);
	free(name);
	return ret;
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


static xmlNodePtr
find_node(xmlNodePtr start, char *name)
{
	d(printf("trying to find node '%s'\n", name));
	while (start && strcmp(start->name, name))
		start = start->next;
	d(printf("node = %p\n", start));
	return start;
}

static xmlNodePtr
find_node_attr(xmlNodePtr start, char *name, char *attrname, char *attrvalue)
{
	xmlNodePtr node;
	char *s;

	d(printf("looking for node named %s with attribute %s=%s\n", name, attrname, attrvalue));

	while (	start && (start = find_node(start, name)) ) {
		s = xmlGetProp(start, attrname);
		d(printf("   comparing '%s' to '%s'\n", s, attrvalue));
		if (s && !strcmp(s, attrvalue)) {
			free(s);
			break;
		}
		free(s);
		start = start->next;
	}
	return start;
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
			d(printf(" **** node name = %s var name = %s  var type = %s\n", node->name, varname, detokenise(vartype)));
			list = g_list_append(list, desc);
			d(printf("appending '%s'\n", node->content));
			newtype = type;
			newvartype = -1;
			newvarname = NULL;
		} else {
			newtype = tokenise(node->name);
			newvartype = tokenise_xmlfreeprop(xmlGetProp(node, "type"));
			newvarname = xmlGetProp(node, "name");
		}
		n = node->childs;
		while (n) {
			d(printf("adding child '%s'\n", n->name));
			list = g_list_concat(list, load_desc(n, newtype, newvartype, newvarname));
			n = n->next;
		}
		if (newvarname)
			free(newvarname);
		node = node->next;
	}
	return list;
}

GList *
filter_load_ruleset(xmlDocPtr doc)
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
		
		ruletype = tokenise_xmlfreeprop(xmlGetProp(ruleset, "type"));

		d(printf("ruleset, name = %s\n", ruleset->name));

		while (rule) {

			n = rule->childs;
			r = g_malloc0(sizeof(*r));
			r->type = ruletype;
			r->name = xmlGetProp(rule, "name");

			d(printf(" rule, name = %s\n", r->name));

			while (n) {
				type = tokenise(n->name);
				d(printf("  n, name = %s\n", n->name));
				d(printf("  ncontent = %s\n", n->content));
				d(printf("  childs = %p\n", n->childs));
				if (n->childs) {
					d(printf(" childs content = %s\n", n->childs->content));
				}
				switch(type) {
				case FILTER_XML_CODE:
					r->code = xmlNodeGetContent(n);
					break;
				case FILTER_XML_DESC:
					d(printf(" ** loading description\n"));
					r->description = load_desc(n->childs, type, -1, NULL);
					d(printf(" ** done loading description\n"));
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
	d(printf("finding, is %s = %s?\n", a->name, name));
	return strcmp(a->name, name);
}

int
filter_find_arg(FilterArg *a, char *name)
{
	d(printf("finding, is %s = %s?\n", a->name, name));
	return strcmp(a->name, name);
}

static FilterArg *
load_optionvalue(struct filter_desc *desc, xmlNodePtr node)
{
	xmlNodePtr n;
	int token;
	int lasttoken = -2;
	FilterArg *arg = NULL;

	d(printf("creating arg entry for '%s'\n", desc->varname));

	switch(desc->vartype) {
	case FILTER_XML_ADDRESS:
		arg = filter_arg_address_new(desc->varname);
		break;
	case FILTER_XML_FOLDER:
		arg = filter_arg_folder_new(desc->varname);
		break;
	default:
		d(printf("ok, maybe we're not\n"));
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
filter_load_optionset(xmlDocPtr doc, GList *rules)
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
		return NULL;
	}
	option = find_node(optionset->childs, "option");
	while (option) {
		o = option->childs;
		op = g_malloc0(sizeof(*op));
		d(printf("option = %s\n", o->name));
		d(printf("option, type=%s\n", xmlGetProp(option, "type")));
		op->type = tokenise_xmlfreeprop(xmlGetProp(option, "type"));
		while (o) {
			char *rulestr;

			type = tokenise(o->name);
			switch (type) {
			case FILTER_XML_OPTIONRULE:
				rulestr = xmlGetProp(o, "rule");
				lrule = g_list_find_custom(rules, rulestr, (GCompareFunc) filter_find_rule);
				if (lrule) {
					fr = lrule->data;
					d(printf("found rule : %s\n", fr->name));
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
							if (arg) {
								optionrule->args = g_list_append(optionrule->args, arg);
								d(printf("Adding arg %s\n", arg->name));
							}
						}
						ldesc = g_list_next(ldesc);
					}
				} else {
					/* FIXME: memleak */
					printf("Cannot find rule: %s\n", rulestr);
				}
				free(rulestr);
				break;
			case FILTER_XML_DESC:
				d(printf("loading option descriptiong\n"));
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

xmlNodePtr
filter_write_optionset(xmlDocPtr doc, GList *optionl)
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

/* utility functions */
struct filter_optionrule *
filter_clone_optionrule(struct filter_optionrule *or)
{
	GList *arg;
	struct filter_optionrule *rule;

	rule = g_malloc0(sizeof(*rule));

	rule->rule = or->rule;
	arg = or->args;
	while (arg) {
		FilterArg *new = filter_arg_clone(FILTER_ARG(arg->data));
		gtk_object_set_data(new, "origin", arg->data);
		rule->args = g_list_append(rule->args, new);
		arg = g_list_next(arg);
	}
	return rule;
}

void
filter_clone_optionrule_free(struct filter_optionrule *or)
{
	GList *argl;
	struct filter_optionrule *rule;

	d(printf("---- free optionrule\n"));

	argl = or->args;
	while (argl) {
		gtk_object_unref(GTK_OBJECT(argl->data));
		argl = g_list_next(argl);
	}
	g_list_free(or->args);
	g_free(or);
}

struct filter_optionrule *
filter_optionrule_new_from_rule(struct filter_rule *rule)
{
	struct filter_optionrule *or;
	GList *descl;

	or = g_malloc0(sizeof(*or));

	or->rule = rule;

	descl = rule->description;
	while (descl) {
		struct filter_desc *desc = descl->data;
		if (desc->varname && desc->vartype != -1) {
			FilterArg *arg = NULL;
			switch (desc->vartype) {
			case FILTER_XML_ADDRESS:
				arg = filter_arg_address_new(desc->varname);
				break;
			case FILTER_XML_FOLDER:
				arg = filter_arg_folder_new(desc->varname);
				break;
			}
			if (arg) {
				or->args = g_list_append(or->args, arg);
			}
		}
		descl = g_list_next(descl);
	}
	return or;
}

void
filter_description_free(GList *descl)
{
	GList *node;

	node = descl;
	while (node) {
		GList *next = g_list_next(node);
		struct filter_desc *d = node->data;

		g_free(d->data);
		g_free(d->varname);
		g_free(d);

		node = next;
	}
	g_list_free(descl);
}

void
filter_load_ruleset_free(GList *nodel)
{
	GList *node = nodel;

	while (node) {
		GList *next = g_list_next(node);
		struct filter_rule *r = node->data;

		filter_description_free(r->description);

		/* g_free(r->name); */
		/* g_free(r->code); */

		g_free(r);
		node = next;
	}
	g_list_free(nodel);
}

void
filter_load_optionset_free(GList *optionl)
{
	GList *option = optionl;
	while (option) {
		GList *next = g_list_next(option);
		struct filter_option *fo = option->data;
		GList *optionrule = fo->options;

		while (optionrule) {
			GList *next = g_list_next(optionrule);
			struct filter_optionrule *or = optionrule->data;
			GList *arg = or->args;

			while (arg) {
				gtk_object_unref(arg->data);
				arg = g_list_next(arg);
			}

			g_list_free(or->args);
			g_free(or);
			optionrule = next;
		}
		filter_description_free(fo->description);
		g_list_free(fo->options);
		g_free(fo);
		option = next;
	}
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

