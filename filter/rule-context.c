/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <errno.h>
#include <gtk/gtk.h>
#include <gnome.h>

#include "rule-context.h"

#define d(x) x

static int	load(RuleContext *f, const char *system, const char *user);
static int	save(RuleContext *f, const char *user);

static void rule_context_class_init	(RuleContextClass *class);
static void rule_context_init	(RuleContext *gspaper);
static void rule_context_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((RuleContext *)(x))->priv)

struct _RuleContextPrivate {
};

static GtkObjectClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
rule_context_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"RuleContext",
			sizeof(RuleContext),
			sizeof(RuleContextClass),
			(GtkClassInitFunc)rule_context_class_init,
			(GtkObjectInitFunc)rule_context_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(gtk_object_get_type (), &type_info);
	}
	
	return type;
}

static void
rule_context_class_init (RuleContextClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(gtk_object_get_type ());

	object_class->finalize = rule_context_finalise;

	/* override methods */
	class->load = load;
	class->save = save;

	/* signals */

	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
rule_context_init (RuleContext *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));

	o->part_set_map = g_hash_table_new(g_str_hash, g_str_equal);
	o->rule_set_map = g_hash_table_new(g_str_hash, g_str_equal);
}

static void
rule_context_finalise(GtkObject *obj)
{
	RuleContext *o = (RuleContext *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * rule_context_new:
 *
 * Create a new RuleContext object.
 * 
 * Return value: A new #RuleContext object.
 **/
RuleContext *
rule_context_new(void)
{
	RuleContext *o = (RuleContext *)gtk_type_new(rule_context_get_type ());
	return o;
}

void		rule_context_add_part_set(RuleContext *f, const char *setname, int part_type, RCPartFunc append, RCNextPartFunc next)
{
	struct _part_set_map *map;

	map = g_malloc0(sizeof(*map));
	map->type = part_type;
	map->append = append;
	map->next = next;
	map->name = g_strdup(setname);
	g_hash_table_insert(f->part_set_map, map->name, map);
	f->part_set_list = g_list_append(f->part_set_list, map);
	d(printf("adding part set '%s'\n", setname));
}

void		rule_context_add_rule_set(RuleContext *f, const char *setname, int rule_type, RCRuleFunc append, RCNextRuleFunc next)
{
	struct _rule_set_map *map;

	map = g_malloc0(sizeof(*map));
	map->type = rule_type;
	map->append = append;
	map->next = next;
	map->name = g_strdup(setname);
	g_hash_table_insert(f->rule_set_map, map->name, map);
	f->rule_set_list = g_list_append(f->rule_set_list, map);
	d(printf("adding rule set '%s'\n", setname));
}

/**
 * rule_context_set_error:
 * @f: 
 * @error: 
 * 
 * Set the text error for the context, or NULL to clear it.
 **/
void
rule_context_set_error(RuleContext *f, char *error)
{
	g_free(f->error);
	f->error = error;
}

/**
 * rule_context_load:
 * @f: 
 * @system: 
 * @user: 
 * 
 * Load a rule context from a system and user description file.
 * 
 * Return value: 
 **/
int		rule_context_load(RuleContext *f, const char *system, const char *user)
{
	printf("rule_context: loading %s %s\n", system, user);

	return ((RuleContextClass *)((GtkObject *)f)->klass)->load(f, system, user);
}

static int	load(RuleContext *f, const char *system, const char *user)
{
	xmlNodePtr set, rule;
	struct _part_set_map *part_map;
	struct _rule_set_map *rule_map;

	rule_context_set_error(f, NULL);

	d(printf("loading rules %s %s\n", system, user));

	f->system = xmlParseFile(system);
	if (f->system == NULL) {
		rule_context_set_error(f, g_strdup_printf("Unable to load system rules '%s': %s",
							  system, strerror(errno)));
		return -1;
	}
	if (strcmp(f->system->root->name, "filterdescription")) {
		rule_context_set_error(f, g_strdup_printf("Unable to load system rules '%s': Invalid format",
							  system));
		xmlFreeDoc(f->system);
		f->system = NULL;
		return -1;
	}
	/* doesn't matter if this doens't exist */
	f->user = xmlParseFile(user);

	/* now parse structure */
	/* get rule parts */
	set = f->system->root->childs;
	while (set) {
		d(printf("set name = %s\n", set->name));
		part_map = g_hash_table_lookup(f->part_set_map, set->name);
		if (part_map) {
			d(printf("loading parts ...\n"));
			rule = set->childs;
			while (rule) {
				if (!strcmp(rule->name, "part")) {
					FilterPart *part = FILTER_PART(gtk_type_new(part_map->type));
					if (filter_part_xml_create(part, rule) == 0) {
						part_map->append(f, part);
					} else {
						gtk_object_unref((GtkObject *)part);
						g_warning("Cannot load filter part");
					}
				}
				rule = rule->next;
			}
		}
		set = set->next;
	}

	/* now load actual rules */
	if (f->user) {
		set = f->user->root->childs;
		while (set) {
			d(printf("set name = %s\n", set->name));
			rule_map = g_hash_table_lookup(f->rule_set_map, set->name);
			if (rule_map) {
				d(printf("loading rules ...\n"));
				rule = set->childs;
				while (rule) {
					printf("checking node: %s\n", rule->name);
					if (!strcmp(rule->name, "rule")) {
						FilterRule *part = FILTER_RULE(gtk_type_new(rule_map->type));
						if (filter_rule_xml_decode(part, rule, f) == 0) {
							rule_map->append(f, part);
						} else {
							gtk_object_unref((GtkObject *)part);
							g_warning("Cannot load filter part");
						}
					}
					rule = rule->next;
				}
			}
			set = set->next;
		}
	}
	return 0;
}

/**
 * rule_context_save:
 * @f: 
 * @user: 
 * 
 * Save a rule context to disk.
 * 
 * Return value: 
 **/
int		rule_context_save(RuleContext *f, const char *user)
{
	return ((RuleContextClass *)((GtkObject *)f)->klass)->save(f, user);
}

static int	save(RuleContext *f, const char *user)
{
	xmlDocPtr doc;
	xmlNodePtr root, rules, work;
	GList *l;
	FilterRule *rule;
	struct _rule_set_map *map;

	doc = xmlNewDoc("1.0");
	root = xmlNewDocNode(doc, NULL, "filteroptions", NULL);
	xmlDocSetRootElement(doc, root);
	l = f->rule_set_list;
	while (l) {
		map = l->data;
		rules = xmlNewDocNode(doc, NULL, map->name, NULL);
		xmlAddChild(root, rules);
		rule = NULL;
		while ( (rule = map->next(f, rule)) ) {
			d(printf("processing rule %s\n", rule->name));
			work = filter_rule_xml_encode(rule);
			xmlAddChild(rules, work);
		}
		l = g_list_next(l);
	}
	xmlSaveFile(user, doc);
	xmlFreeDoc(doc);
	return 0;
}

FilterPart *rule_context_find_part(RuleContext *f, const char *name)
{
	d(printf("find part : "));
	return filter_part_find_list(f->parts, name);
}

FilterPart 	*rule_context_create_part(RuleContext *f, const char *name)
{
	FilterPart *part;

	part = rule_context_find_part(f, name);
	if (part)
		part = filter_part_clone(part);
	return part;
}

FilterPart 	*rule_context_next_part(RuleContext *f, FilterPart *last)
{
	return filter_part_next_list(f->parts, last);
}

FilterRule 	*rule_context_next_rule(RuleContext *f, FilterRule *last)
{
	return filter_rule_next_list(f->rules, last);
}

FilterRule 	*rule_context_find_rule(RuleContext *f, const char *name)
{
	return filter_rule_find_list(f->rules, name);
}

void		rule_context_add_part(RuleContext *f, FilterPart *part)
{
	f->parts = g_list_append(f->parts, part);
}

void		rule_context_add_rule(RuleContext *f, FilterRule *new)
{
	f->rules = g_list_append(f->rules, new);
}

void		rule_context_remove_rule(RuleContext *f, FilterRule *rule)
{
	f->rules = g_list_remove(f->rules, rule);
}

void		rule_context_rank_rule(RuleContext *f, FilterRule *rule, int rank)
{
	f->rules = g_list_remove(f->rules, rule);
	f->rules = g_list_insert(f->rules, rule, rank);
}

int		rule_context_get_rank_rule(RuleContext *f, FilterRule *rule)
{
	return g_list_index(f->rules, rule);
}
