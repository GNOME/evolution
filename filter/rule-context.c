/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-stock.h>

#include <gal/util/e-xml-utils.h>

#include "rule-context.h"

#define d(x) 

static int load(RuleContext * f, const char *system, const char *user);
static int save(RuleContext * f, const char *user);
static int revert(RuleContext *f, const char *user);
static GList *rename_uri(RuleContext *f, const char *olduri, const char *newuri, GCompareFunc cmp);
static GList *delete_uri(RuleContext *f, const char *uri, GCompareFunc cmp);

static void rule_context_class_init(RuleContextClass * class);
static void rule_context_init(RuleContext * gspaper);
static void rule_context_finalise(GtkObject * obj);

#define _PRIVATE(x) (((RuleContext *)(x))->priv)

struct _RuleContextPrivate {
	int frozen;
};

static GtkObjectClass *parent_class;

enum {
	RULE_ADDED,
	RULE_REMOVED,
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
rule_context_get_type(void)
{
	static guint type = 0;

	if (!type) {
		GtkTypeInfo type_info = {
			"RuleContext",
			sizeof(RuleContext),
			sizeof(RuleContextClass),
			(GtkClassInitFunc) rule_context_class_init,
			(GtkObjectInitFunc) rule_context_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		type = gtk_type_unique(gtk_object_get_type(), &type_info);
	}

	return type;
}

static void
rule_context_class_init (RuleContextClass * class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class(gtk_object_get_type());
	
	object_class->finalize = rule_context_finalise;
	
	/* override methods */
	class->load = load;
	class->save = save;
	class->revert = revert;
	class->rename_uri = rename_uri;
	class->delete_uri = delete_uri;

	/* signals */
	signals[RULE_ADDED] =
		gtk_signal_new("rule_added",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (RuleContextClass, rule_added),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	signals[RULE_REMOVED] =
		gtk_signal_new("rule_removed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (RuleContextClass, rule_removed),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	signals[CHANGED] =
		gtk_signal_new("changed",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET (RuleContextClass, changed),
			       gtk_marshal_NONE__NONE,
			       GTK_TYPE_NONE, 0);
	
	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
rule_context_init (RuleContext * o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
	
	o->part_set_map = g_hash_table_new (g_str_hash, g_str_equal);
	o->rule_set_map = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
free_part_set (struct _part_set_map *map, void *data)
{
	g_free (map->name);
	g_free (map);
}

static void
free_rule_set (struct _rule_set_map *map, void *data)
{
	g_free (map->name);
	g_free (map);
}

static void
rule_context_finalise (GtkObject * obj)
{
	RuleContext *o = (RuleContext *) obj;
	
	g_list_foreach (o->rule_set_list, (GFunc)free_rule_set, NULL);
	g_list_free (o->rule_set_list);
	g_hash_table_destroy (o->rule_set_map);
	
	g_list_foreach (o->part_set_list, (GFunc)free_part_set, NULL);
	g_list_free (o->part_set_list);
	g_hash_table_destroy (o->part_set_map);
	
	g_free (o->error);
	
	g_list_foreach (o->parts, (GFunc)gtk_object_unref, NULL);
	g_list_free (o->parts);
	g_list_foreach (o->rules, (GFunc)gtk_object_unref, NULL);
	g_list_free (o->rules);
	
	if (o->system)
		xmlFreeDoc (o->system);
	if (o->user)
		xmlFreeDoc (o->user);
	
	g_free (o->priv);
	
	((GtkObjectClass *) (parent_class))->finalize (obj);
}

/**
 * rule_context_new:
 *
 * Create a new RuleContext object.
 * 
 * Return value: A new #RuleContext object.
 **/
RuleContext *
rule_context_new (void)
{
	RuleContext *o = (RuleContext *) gtk_type_new(rule_context_get_type());

	return o;
}

void
rule_context_add_part_set (RuleContext * f, const char *setname, int part_type, RCPartFunc append, RCNextPartFunc next)
{
	struct _part_set_map *map;
	
	g_assert(g_hash_table_lookup(f->part_set_map, setname) == NULL);
	
	map = g_malloc0 (sizeof(*map));
	map->type = part_type;
	map->append = append;
	map->next = next;
	map->name = g_strdup (setname);
	g_hash_table_insert (f->part_set_map, map->name, map);
	f->part_set_list = g_list_append (f->part_set_list, map);
	d(printf("adding part set '%s'\n", setname));
}

void
rule_context_add_rule_set (RuleContext * f, const char *setname, int rule_type, RCRuleFunc append, RCNextRuleFunc next)
{
	struct _rule_set_map *map;

	g_assert(g_hash_table_lookup (f->rule_set_map, setname) == NULL);
	
	map = g_malloc0 (sizeof (*map));
	map->type = rule_type;
	map->append = append;
	map->next = next;
	map->name = g_strdup (setname);
	g_hash_table_insert (f->rule_set_map, map->name, map);
	f->rule_set_list = g_list_append (f->rule_set_list, map);
	d(printf("adding rule set '%s'\n", setname));
}

/**
 * rule_context_set_error:
 * @f: 
 * @error: 
 * 
 * Set the text error for the context, or NULL to clear it.
 **/
static void
rule_context_set_error (RuleContext * f, char *error)
{
	g_assert(f);

	g_free (f->error);
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
int
rule_context_load (RuleContext *f, const char *system, const char *user)
{
	int res;

	g_assert(f);

	d(printf("rule_context: loading %s %s\n", system, user));

	f->priv->frozen++;
	res= ((RuleContextClass *) ((GtkObject *) f)->klass)->load (f, system, user);
	f->priv->frozen--;

	return res;
}

static int
load (RuleContext *f, const char *system, const char *user)
{
	xmlNodePtr set, rule;
	struct _part_set_map *part_map;
	struct _rule_set_map *rule_map;
	
	rule_context_set_error (f, NULL);
	
	d(printf("loading rules %s %s\n", system, user));
	
	f->system = xmlParseFile (system);
	if (f->system == NULL) {
		rule_context_set_error(f, g_strdup_printf ("Unable to load system rules '%s': %s",
							   system, strerror(errno)));
		return -1;
	}
	if (strcmp (f->system->root->name, "filterdescription")) {
		rule_context_set_error (f, g_strdup_printf ("Unable to load system rules '%s': Invalid format", system));
		xmlFreeDoc (f->system);
		f->system = NULL;
		return -1;
	}
	/* doesn't matter if this doens't exist */
	f->user = xmlParseFile (user);
	
	/* now parse structure */
	/* get rule parts */
	set = f->system->root->childs;
	while (set) {
		d(printf("set name = %s\n", set->name));
		part_map = g_hash_table_lookup (f->part_set_map, set->name);
		if (part_map) {
			d(printf("loading parts ...\n"));
			rule = set->childs;
			while (rule) {
				if (!strcmp (rule->name, "part")) {
					FilterPart *part = FILTER_PART (gtk_type_new (part_map->type));
					
					if (filter_part_xml_create (part, rule) == 0) {
						part_map->append (f, part);
					} else {
						gtk_object_unref (GTK_OBJECT (part));
						g_warning ("Cannot load filter part");
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
			rule_map = g_hash_table_lookup (f->rule_set_map, set->name);
			if (rule_map) {
				d(printf("loading rules ...\n"));
				rule = set->childs;
				while (rule) {
					d(printf("checking node: %s\n", rule->name));
					if (!strcmp (rule->name, "rule")) {
						FilterRule *part = FILTER_RULE(gtk_type_new (rule_map->type));
						
						if (filter_rule_xml_decode (part, rule, f) == 0) {
							rule_map->append (f, part);
						} else {
							gtk_object_unref (GTK_OBJECT (part));
							g_warning ("Cannot load filter part");
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
int
rule_context_save (RuleContext *f, const char *user)
{
	g_assert(f);
	g_assert(user);

	return ((RuleContextClass *) ((GtkObject *) f)->klass)->save(f, user);
}

static int
save (RuleContext *f, const char *user)
{
	xmlDocPtr doc;
	xmlNodePtr root, rules, work;
	GList *l;
	FilterRule *rule;
	struct _rule_set_map *map;
	char *usersav, *userbak, *slash;
	int ret;
	
	doc = xmlNewDoc ("1.0");
	root = xmlNewDocNode (doc, NULL, "filteroptions", NULL);
	xmlDocSetRootElement (doc, root);
	l = f->rule_set_list;
	while (l) {
		map = l->data;
		rules = xmlNewDocNode (doc, NULL, map->name, NULL);
		xmlAddChild (root, rules);
		rule = NULL;
		while ((rule = map->next (f, rule, NULL))) {
			d(printf("processing rule %s\n", rule->name));
			work = filter_rule_xml_encode (rule);
			xmlAddChild (rules, work);
		}
		l = g_list_next (l);
	}
	
	usersav = alloca(strlen(user)+5);
	userbak = alloca(strlen(user)+5);
	slash = strrchr(user, '/');
	if (slash)
		sprintf(usersav, "%.*s.#%s", slash-user+1, user, slash+1);
	else
		sprintf(usersav, ".#%s", user);
	sprintf(userbak, "%s~", user);
	d(printf("saving rules to '%s' then backup '%s'\n", usersav, userbak));
	ret = e_xml_save_file (usersav, doc);
	if (ret != -1) {
		rename(user, userbak);
		ret = rename(usersav, user);
	}
	xmlFreeDoc (doc);
	return ret;
}

/**
 * rule_context_revert:
 * @f: 
 * @user: 
 *
 * Reverts a rule context from a user description file.  Assumes the
 * system description file is unchanged from when it was loaded.
 * 
 * Return value: 
 **/
int
rule_context_revert(RuleContext *f, const char *user)
{
	g_assert(f);

	d(printf("rule_context: restoring %s %s\n", user));

	return ((RuleContextClass *) ((GtkObject *) f)->klass)->revert(f, user);
}

struct _revert_data {
	GHashTable *rules;
	int rank;
};

static void
revert_rule_remove(void *key, FilterRule *frule, RuleContext *f)
{
	rule_context_remove_rule(f, frule);
	gtk_object_unref((GtkObject *)frule);
}

static void
revert_source_remove(void *key, struct _revert_data *rest_data, RuleContext *f)
{
	g_hash_table_foreach(rest_data->rules, (GHFunc)revert_rule_remove, f);
	g_hash_table_destroy(rest_data->rules);
	g_free(rest_data);
}

static guint source_hashf(const char *a)
{
	if (a)
		return g_str_hash(a);
	return 0;
}

static int source_eqf(const char *a, const char *b)
{
	return (a && b && strcmp(a, b) == 0)
		|| (a == NULL && b == NULL);
}

static int
revert(RuleContext *f, const char *user)
{
	xmlNodePtr set, rule;
	/*struct _part_set_map *part_map;*/
	struct _rule_set_map *rule_map;
	struct _revert_data *rest_data;
	GHashTable *source_hash;
	xmlDocPtr userdoc;
	FilterRule *frule;

	rule_context_set_error (f, NULL);
	
	d(printf("restoring rules %s %s\n", user));

	userdoc = xmlParseFile (user);
	if (userdoc == NULL)
		/* clear out anythign we have? */
		return 0;

	source_hash = g_hash_table_new((GHashFunc)source_hashf, (GCompareFunc)source_eqf);

	/* setup stuff we have now */
	/* Note that we assume there is only 1 set of rules in a given rule context,
	   although other parts of the code dont assume this */
	frule = NULL;
	while ((frule = rule_context_next_rule(f, frule, NULL))) {
		rest_data = g_hash_table_lookup(source_hash, frule->source);
		if (rest_data == NULL) {
			rest_data = g_malloc0(sizeof(*rest_data));
			rest_data->rules = g_hash_table_new(g_str_hash, g_str_equal);
			g_hash_table_insert(source_hash, frule->source, rest_data);
		}
		g_hash_table_insert(rest_data->rules, frule->name, frule);
	}

	/* make what we have, match what we load */
	set = userdoc->root->childs;
	while (set) {
		d(printf("set name = %s\n", set->name));
		rule_map = g_hash_table_lookup (f->rule_set_map, set->name);
		if (rule_map) {
			d(printf("loading rules ...\n"));
			rule = set->childs;
			while (rule) {
				d(printf("checking node: %s\n", rule->name));
				if (!strcmp (rule->name, "rule")) {
					FilterRule *part = FILTER_RULE(gtk_type_new (rule_map->type));
					
					if (filter_rule_xml_decode (part, rule, f) == 0) {
						/* use the revert data to keep track of the right rank of this rule part */
						rest_data = g_hash_table_lookup(source_hash, part->source);
						if (rest_data == NULL) {
							rest_data = g_malloc0(sizeof(*rest_data));
							rest_data->rules = g_hash_table_new(g_str_hash, g_str_equal);
							g_hash_table_insert(source_hash, part->source, rest_data);
						}
						frule = g_hash_table_lookup(rest_data->rules, part->name);
						if (frule) {
							if (f->priv->frozen == 0 && !filter_rule_eq(frule, part))
								filter_rule_copy(frule, part);
							gtk_object_unref (GTK_OBJECT (part));
							rule_context_rank_rule(f, frule, rest_data->rank);
							g_hash_table_remove(rest_data->rules, frule->name);
						} else {
							rule_context_add_rule(f, part);
							rule_context_rank_rule(f, part, rest_data->rank);
						}
						rest_data->rank++;
					} else {
						gtk_object_unref (GTK_OBJECT (part));
						g_warning ("Cannot load filter part");
					}
				}
				rule = rule->next;
			}
		}
		set = set->next;
	}

	xmlFreeDoc(userdoc);

	/* remove any we still have that weren't in the file */
	g_hash_table_foreach(source_hash, (GHFunc)revert_source_remove, f);
	g_hash_table_destroy(source_hash);

	return 0;
}

FilterPart *
rule_context_find_part (RuleContext *f, const char *name)
{
	g_assert(f);
	g_assert(name);

	d(printf("find part : "));
	return filter_part_find_list (f->parts, name);
}

FilterPart *
rule_context_create_part (RuleContext *f, const char *name)
{
	FilterPart *part;

	g_assert(f);
	g_assert(name);

	part = rule_context_find_part (f, name);
	if (part)
		part = filter_part_clone (part);
	return part;
}

FilterPart *
rule_context_next_part (RuleContext *f, FilterPart *last)
{
	g_assert(f);

	return filter_part_next_list (f->parts, last);
}

FilterRule *
rule_context_next_rule (RuleContext *f, FilterRule *last, const char *source)
{
	g_assert(f);

	return filter_rule_next_list (f->rules, last, source);
}

FilterRule *
rule_context_find_rule (RuleContext *f, const char *name, const char *source)
{
	g_assert(name);
	g_assert(f);

	return filter_rule_find_list (f->rules, name, source);
}

void
rule_context_add_part (RuleContext *f, FilterPart *part)
{
	g_assert(f);
	g_assert(part);

	f->parts = g_list_append (f->parts, part);
}

void
rule_context_add_rule (RuleContext *f, FilterRule *new)
{
	g_assert(f);
	g_assert(new);

	d(printf("add rule '%s'\n", new->name));

	f->rules = g_list_append (f->rules, new);

	if (f->priv->frozen == 0) {
		gtk_signal_emit((GtkObject *)f, signals[RULE_ADDED], new);
		gtk_signal_emit((GtkObject *)f, signals[CHANGED]);
	}
}

static void
new_rule_clicked (GtkWidget *dialog, int button, RuleContext *context)
{
	if (button == 0) {
		FilterRule *rule = gtk_object_get_data (GTK_OBJECT (dialog), "rule");
		char *user = gtk_object_get_data (GTK_OBJECT (dialog), "path");
		
		if (!filter_rule_validate (rule)) {
			/* no need to popup a dialog because the validate code does that. */
			return;
		}
		
		gtk_object_ref (GTK_OBJECT (rule));
		rule_context_add_rule (context, rule);
		if (user) {
			rule_context_save ((RuleContext *) context, user);
		}
	}
	
	if (button != -1) {
		gnome_dialog_close (GNOME_DIALOG (dialog));
	}
}

/* add a rule, with a gui, asking for confirmation first ... optionally save to path */
void
rule_context_add_rule_gui (RuleContext *f, FilterRule *rule, const char *title, const char *path)
{
	GtkWidget *dialog, *w;

	d(printf("add rule gui '%s'\n", rule->name));

	g_assert(f);
	g_assert(rule);
	
	w = filter_rule_get_widget (rule, f);
	dialog = gnome_dialog_new (title, GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), w, TRUE, TRUE, 0);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 400);
	gtk_widget_show (w);
	
	gtk_object_set_data_full (GTK_OBJECT (dialog), "rule", rule, (GtkDestroyNotify) gtk_object_unref);
	if (path)
		gtk_object_set_data_full (GTK_OBJECT (dialog), "path", g_strdup (path), (GtkDestroyNotify) g_free);
	gtk_signal_connect (GTK_OBJECT (dialog), "clicked", new_rule_clicked, f);
	gtk_object_ref (GTK_OBJECT (f));
	gtk_object_set_data_full (GTK_OBJECT (dialog), "context", f, (GtkDestroyNotify) gtk_object_unref);
	gtk_widget_show (dialog);
}

void
rule_context_remove_rule (RuleContext *f, FilterRule *rule)
{
	g_assert(f);
	g_assert(rule);

	d(printf("remove rule '%s'\n", rule->name));

	f->rules = g_list_remove (f->rules, rule);

	if (f->priv->frozen == 0) {
		gtk_signal_emit((GtkObject *)f, signals[RULE_REMOVED], rule);
		gtk_signal_emit((GtkObject *)f, signals[CHANGED]);
	}
}

void
rule_context_rank_rule (RuleContext *f, FilterRule *rule, int rank)
{
	GList *node;
	int i = 0, index = 0;

	g_assert(f);
	g_assert(rule);

	if (rule_context_get_rank_rule(f, rule, rule->source) == rank)
		return;

	f->rules = g_list_remove(f->rules, rule);
	node = f->rules;
	while (node) {
		FilterRule *r = node->data;

		if (i == rank) {
			f->rules = g_list_insert(f->rules, rule, index);
			if (f->priv->frozen == 0)
				gtk_signal_emit((GtkObject *)f, signals[CHANGED]);
			return;
		}

		index++;		
		if (rule->source == NULL || (r->source && strcmp (r->source, rule->source) == 0))
			i++;

		node = node->next;
	}

	f->rules = g_list_append(f->rules, rule);
	if (f->priv->frozen == 0)
		gtk_signal_emit((GtkObject *)f, signals[CHANGED]);
}

int
rule_context_get_rank_rule (RuleContext *f, FilterRule *rule, const char *source)
{
	GList *node;
	int i = 0;

	g_assert(f);
	g_assert(rule);

	d(printf("getting rank of rule '%s'\n", rule->name));

	node = f->rules;
	while (node) {
		FilterRule *r = node->data;

		d(printf(" checking against rule '%s' rank '%d'\n", r->name, i));
		
		if (r == rule)
			return i;

		if (source == NULL || (r->source && strcmp (r->source, source) == 0))
			i++;
		
		node = node->next;
	}
	
	return -1;
}

FilterRule *
rule_context_find_rank_rule (RuleContext *f, int rank, const char *source)
{
	GList *node;
	int i = 0;

	g_assert(f);

	d(printf("getting rule at rank %d source '%s'\n", rank, source?source:"<any>"));

	node = f->rules;
	while (node) {
		FilterRule *r = node->data;

		d(printf(" checking against rule '%s' rank '%d'\n", r->name, i));

		if (source == NULL || (r->source && strcmp(r->source, source) == 0)) {
			if (rank == i)
				return r;
			i++;
		}
		
		node = node->next;
	}
	
	return NULL;
}

static GList *
delete_uri(RuleContext *f, const char *uri, GCompareFunc cmp)
{
	return NULL;
}

GList *
rule_context_delete_uri(RuleContext *f, const char *uri, GCompareFunc cmp)
{
	return ((RuleContextClass *) ((GtkObject *) f)->klass)->delete_uri(f, uri, cmp);
}

static GList *
rename_uri(RuleContext *f, const char *olduri, const char *newuri, GCompareFunc cmp)
{
	return NULL;
}

GList *
rule_context_rename_uri(RuleContext *f, const char *olduri, const char *newuri, GCompareFunc cmp)
{
	return ((RuleContextClass *) ((GtkObject *) f)->klass)->rename_uri (f, olduri, newuri, cmp);
}

void
rule_context_free_uri_list(RuleContext *f, GList *uris)
{
	GList *l = uris, *n;

	/* TODO: should be virtual */

	while (l) {
		n = l->next;
		g_free(l->data);
		g_list_free_1(l);
		l = n;
	}
}


