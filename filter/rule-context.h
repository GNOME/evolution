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

#ifndef _RULE_CONTEXT_H
#define _RULE_CONTEXT_H

#include <gtk/gtkobject.h>
#include <gnome-xml/parser.h>

#include "filter-part.h"
#include "filter-rule.h"

#define RULE_CONTEXT(obj)	GTK_CHECK_CAST (obj, rule_context_get_type (), RuleContext)
#define RULE_CONTEXT_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, rule_context_get_type (), RuleContextClass)
#define IS_RULE_CONTEXT(obj)      GTK_CHECK_TYPE (obj, rule_context_get_type ())

typedef struct _RuleContext	RuleContext;
typedef struct _RuleContextClass	RuleContextClass;

struct _RuleContext {
	GtkObject parent;
	struct _RuleContextPrivate *priv;

	char *error;		/* string version of error */

	xmlDocPtr system;	/* system rules source */
	xmlDocPtr user;		/* user defined rules source */

	GList *parts;
	GList *rules;

	GHashTable *part_set_map;/* map set types to part types */
	GList *part_set_list;
	GHashTable *rule_set_map;/* map set types to rule types */
	GList *rule_set_list;
};

typedef void (*RCRegisterFunc)(RuleContext *f, FilterRule *rule, gpointer data);

struct _RuleContextClass {
	GtkObjectClass parent_class;

	/* virtual methods */
	int (*load)(RuleContext *f, const char *system, const char *user);
	int (*save)(RuleContext *f, const char *user);
	int (*revert)(RuleContext *f, const char *user);

	GList *(*delete_uri)(RuleContext *f, const char *uri, GCompareFunc cmp);
	GList *(*rename_uri)(RuleContext *f, const char *olduri, const char *newuri, GCompareFunc cmp);

	/* signals */
	void (*rule_added)(RuleContext *f, FilterRule *rule);
	void (*rule_removed)(RuleContext *f, FilterRule *rule);
	void (*changed)(RuleContext *f);
};

typedef void (*RCPartFunc)(RuleContext *f, FilterPart *part);
typedef void (*RCRuleFunc)(RuleContext *f, FilterRule *part);
typedef FilterPart * (*RCNextPartFunc)(RuleContext *f, FilterPart *part);
typedef FilterRule * (*RCNextRuleFunc)(RuleContext *f, FilterRule *rule, const char *source);

struct _part_set_map {
	char *name;
	int type;
	RCPartFunc append;
	RCNextPartFunc next;
};

struct _rule_set_map {
	char *name;
	int type;
	RCRuleFunc append;
	RCNextRuleFunc next;
};

guint		rule_context_get_type	(void);
RuleContext	*rule_context_new	(void);

/* methods */
int		rule_context_load(RuleContext *f, const char *system, const char *user);
int		rule_context_save(RuleContext *f, const char *user);
int		rule_context_revert(RuleContext *f, const char *user);

void		rule_context_add_part(RuleContext *f, FilterPart *new);
FilterPart 	*rule_context_find_part(RuleContext *f, const char *name);
FilterPart 	*rule_context_create_part(RuleContext *f, const char *name);
FilterPart 	*rule_context_next_part(RuleContext *f, FilterPart *last);

FilterRule 	*rule_context_next_rule(RuleContext *f, FilterRule *last, const char *source);
FilterRule 	*rule_context_find_rule(RuleContext *f, const char *name, const char *source);
FilterRule 	*rule_context_find_rank_rule(RuleContext *f, int rank, const char *source);
void		rule_context_add_rule(RuleContext *f, FilterRule *new);
void		rule_context_add_rule_gui(RuleContext *f, FilterRule *rule, const char *title, const char *path);
void		rule_context_remove_rule(RuleContext *f, FilterRule *rule);

/* get/set the rank (position) of a rule */
void		rule_context_rank_rule(RuleContext *f, FilterRule *rule, int rank);
int		rule_context_get_rank_rule(RuleContext *f, FilterRule *rule, const char *source);

/* setup type for set parts */
void		rule_context_add_part_set(RuleContext *f, const char *setname, int part_type, RCPartFunc append, RCNextPartFunc next);
void		rule_context_add_rule_set(RuleContext *f, const char *setname, int rule_type, RCRuleFunc append, RCNextRuleFunc next);

/* uri's disappear/renamed externally */
GList		*rule_context_delete_uri(RuleContext *f, const char *uri, GCompareFunc cmp);
GList		*rule_context_rename_uri(RuleContext *f, const char *olduri, const char *newuri, GCompareFunc cmp);

void		rule_context_free_uri_list(RuleContext *f, GList *uris);

#endif /* ! _RULE_CONTEXT_H */

