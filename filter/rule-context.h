/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#include <glib.h>
#include <glib-object.h>
#include <libxml/parser.h>

#include "filter-part.h"
#include "filter-rule.h"

#define RULE_TYPE_CONTEXT            (rule_context_get_type ())
#define RULE_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RULE_TYPE_CONTEXT, RuleContext))
#define RULE_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RULE_TYPE_CONTEXT, RuleContextClass))
#define IS_RULE_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RULE_TYPE_CONTEXT))
#define IS_RULE_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RULE_TYPE_CONTEXT))
#define RULE_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RULE_TYPE_CONTEXT, RuleContextClass))

typedef struct _RuleContext RuleContext;
typedef struct _RuleContextClass RuleContextClass;

/* backend capabilities, this is a hack since we don't support nested rules */
enum {
	RULE_CONTEXT_GROUPING = 1 << 0,
	RULE_CONTEXT_THREADING = 1 << 1,
};

struct _RuleContext {
	GObject parent_object;
	struct _RuleContextPrivate *priv;
	
	char *error;              /* string version of error */

	guint32 flags;		/* capability flags */

	GList *parts;
	GList *rules;
	
	GHashTable *part_set_map; /* map set types to part types */
	GList *part_set_list;
	GHashTable *rule_set_map; /* map set types to rule types */
	GList *rule_set_list;
};

typedef void (*RCRegisterFunc) (RuleContext *rc, FilterRule *rule, gpointer user_data);

struct _RuleContextClass {
	GObjectClass parent_class;

	/* virtual methods */
	int (*load) (RuleContext *rc, const char *system, const char *user);
	int (*save) (RuleContext *rc, const char *user);
	int (*revert) (RuleContext *rc, const char *user);
	
	GList *(*delete_uri) (RuleContext *rc, const char *uri, GCompareFunc cmp);
	GList *(*rename_uri) (RuleContext *rc, const char *olduri, const char *newuri, GCompareFunc cmp);

	FilterElement *(*new_element)(RuleContext *rc, const char *name);
	
	/* signals */
	void (*rule_added) (RuleContext *rc, FilterRule *rule);
	void (*rule_removed) (RuleContext *rc, FilterRule *rule);
	void (*changed) (RuleContext *rc);
};

typedef void (*RCPartFunc) (RuleContext *rc, FilterPart *part);
typedef void (*RCRuleFunc) (RuleContext *rc, FilterRule *part);
typedef FilterPart * (*RCNextPartFunc) (RuleContext *rc, FilterPart *part);
typedef FilterRule * (*RCNextRuleFunc) (RuleContext *rc, FilterRule *rule, const char *source);

struct _part_set_map {
	char *name;
	GType type;
	RCPartFunc append;
	RCNextPartFunc next;
};

struct _rule_set_map {
	char *name;
	GType type;
	RCRuleFunc append;
	RCNextRuleFunc next;
};

GType rule_context_get_type (void);

/* methods */
RuleContext *rule_context_new (void);

/* io */
int rule_context_load (RuleContext *rc, const char *system, const char *user);
int rule_context_save (RuleContext *rc, const char *user);
int rule_context_revert (RuleContext *rc, const char *user);

void rule_context_add_part (RuleContext *rc, FilterPart *new);
FilterPart *rule_context_find_part (RuleContext *rc, const char *name);
FilterPart *rule_context_create_part (RuleContext *rc, const char *name);
FilterPart *rule_context_next_part (RuleContext *rc, FilterPart *last);

FilterRule *rule_context_next_rule (RuleContext *rc, FilterRule *last, const char *source);
FilterRule *rule_context_find_rule (RuleContext *rc, const char *name, const char *source);
FilterRule *rule_context_find_rank_rule (RuleContext *rc, int rank, const char *source);
void rule_context_add_rule (RuleContext *rc, FilterRule *new);
void rule_context_add_rule_gui (RuleContext *rc, FilterRule *rule, const char *title, const char *path);
void rule_context_remove_rule (RuleContext *rc, FilterRule *rule);

/* get/set the rank (position) of a rule */
void rule_context_rank_rule (RuleContext *rc, FilterRule *rule, const char *source, int rank);
int rule_context_get_rank_rule (RuleContext *rc, FilterRule *rule, const char *source);

/* setup type for set parts */
void rule_context_add_part_set (RuleContext *rc, const char *setname, GType part_type,
				RCPartFunc append, RCNextPartFunc next);
void rule_context_add_rule_set (RuleContext *rc, const char *setname, GType rule_type,
				RCRuleFunc append, RCNextRuleFunc next);

/* dynamic element types */
FilterElement *rule_context_new_element(RuleContext *rc, const char *name);

/* uri's disappear/renamed externally */
GList *rule_context_delete_uri (RuleContext *rc, const char *uri, GCompareFunc cmp);
GList *rule_context_rename_uri (RuleContext *rc, const char *olduri, const char *newuri, GCompareFunc cmp);

void rule_context_free_uri_list (RuleContext *rc, GList *uris);

#endif /* ! _RULE_CONTEXT_H */
