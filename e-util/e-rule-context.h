/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_RULE_CONTEXT_H
#define E_RULE_CONTEXT_H

#include <libxml/parser.h>

#include <e-util/e-filter-part.h>
#include <e-util/e-filter-rule.h>

/* Standard GObject macros */
#define E_TYPE_RULE_CONTEXT \
	(e_rule_context_get_type ())
#define E_RULE_CONTEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_RULE_CONTEXT, ERuleContext))
#define E_RULE_CONTEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_RULE_CONTEXT, ERuleContextClass))
#define E_IS_RULE_CONTEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_RULE_CONTEXT))
#define E_IS_RULE_CONTEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_RULE_CONTEXT))
#define E_RULE_CONTEXT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_RULE_CONTEXT, ERuleContextClass))

G_BEGIN_DECLS

typedef struct _ERuleContext ERuleContext;
typedef struct _ERuleContextClass ERuleContextClass;
typedef struct _ERuleContextPrivate ERuleContextPrivate;

/* backend capabilities, this is a hack since we don't support nested rules */
enum {
	E_RULE_CONTEXT_GROUPING = 1 << 0,
	E_RULE_CONTEXT_THREADING = 1 << 1
};

typedef void	(*ERuleContextRegisterFunc)	(ERuleContext *context,
						 EFilterRule *rule,
						 gpointer user_data);
typedef void	(*ERuleContextPartFunc)		(ERuleContext *context,
						 EFilterPart *part);
typedef void	(*ERuleContextRuleFunc)		(ERuleContext *context,
						 EFilterRule *part);
typedef EFilterPart *
		(*ERuleContextNextPartFunc)	(ERuleContext *context,
						 EFilterPart *part);
typedef EFilterRule *
		(*ERuleContextNextRuleFunc)	(ERuleContext *context,
						 EFilterRule *rule,
						 const gchar *source);

struct _ERuleContext {
	GObject parent;
	ERuleContextPrivate *priv;

	gchar *error;		/* string version of error */

	guint32 flags;		/* capability flags */

	GList *parts;
	GList *rules;

	GHashTable *part_set_map; /* map set types to part types */
	GList *part_set_list;
	GHashTable *rule_set_map; /* map set types to rule types */
	GList *rule_set_list;
};

struct _ERuleContextClass {
	GObjectClass parent_class;

	/* methods */
	gint		(*load)			(ERuleContext *context,
						 const gchar *system,
						 const gchar *user);
	gint		(*save)			(ERuleContext *context,
						 const gchar *user);
	gint		(*revert)		(ERuleContext *context,
						 const gchar *user);

	GList *		(*delete_uri)		(ERuleContext *context,
						 const gchar *uri,
						 GCompareFunc compare_func);
	GList *		(*rename_uri)		(ERuleContext *context,
						 const gchar *old_uri,
						 const gchar *new_uri,
						 GCompareFunc compare_func);

	EFilterElement *(*new_element)		(ERuleContext *context,
						 const gchar *name);

	/* signals */
	void		(*rule_added)		(ERuleContext *context,
						 EFilterRule *rule);
	void		(*rule_removed)		(ERuleContext *context,
						 EFilterRule *rule);
	void		(*changed)		(ERuleContext *context);
};

struct _part_set_map {
	gchar *name;
	GType type;
	ERuleContextPartFunc append;
	ERuleContextNextPartFunc next;
};

struct _rule_set_map {
	gchar *name;
	GType type;
	ERuleContextRuleFunc append;
	ERuleContextNextRuleFunc next;
};

GType		e_rule_context_get_type		(void) G_GNUC_CONST;
ERuleContext *	e_rule_context_new		(void);

gint		e_rule_context_load		(ERuleContext *context,
						 const gchar *system,
						 const gchar *user);
gint		e_rule_context_save		(ERuleContext *context,
						 const gchar *user);
gint		e_rule_context_revert		(ERuleContext *context,
						 const gchar *user);

void		e_rule_context_add_part		(ERuleContext *context,
						 EFilterPart *part);
EFilterPart *	e_rule_context_find_part	(ERuleContext *context,
						 const gchar *name);
EFilterPart *	e_rule_context_create_part	(ERuleContext *context,
						 const gchar *name);
EFilterPart *	e_rule_context_next_part	(ERuleContext *context,
						 EFilterPart *last);

EFilterRule *	e_rule_context_next_rule	(ERuleContext *context,
						 EFilterRule *last,
						 const gchar *source);
EFilterRule *	e_rule_context_find_rule	(ERuleContext *context,
						 const gchar *name,
						 const gchar *source);
EFilterRule *	e_rule_context_find_rank_rule	(ERuleContext *context,
						 gint rank,
						 const gchar *source);
void		e_rule_context_add_rule		(ERuleContext *context,
						 EFilterRule *rule);
void		e_rule_context_add_rule_gui	(ERuleContext *context,
						 EFilterRule *rule,
						 const gchar *title,
						 const gchar *path);
void		e_rule_context_remove_rule	(ERuleContext *context,
						 EFilterRule *rule);

void		e_rule_context_rank_rule	(ERuleContext *context,
						 EFilterRule *rule,
						 const gchar *source,
						 gint rank);
gint		e_rule_context_get_rank_rule	(ERuleContext *context,
						 EFilterRule *rule,
						 const gchar *source);

void		e_rule_context_add_part_set	(ERuleContext *context,
						 const gchar *setname,
						 GType part_type,
						 ERuleContextPartFunc append,
						 ERuleContextNextPartFunc next);
void		e_rule_context_add_rule_set	(ERuleContext *context,
						 const gchar *setname,
						 GType rule_type,
						 ERuleContextRuleFunc append,
						 ERuleContextNextRuleFunc next);

EFilterElement *e_rule_context_new_element	(ERuleContext *context,
						 const gchar *name);

GList *		e_rule_context_delete_uri	(ERuleContext *context,
						 const gchar *uri,
						 GCompareFunc compare);
GList *		e_rule_context_rename_uri	(ERuleContext *context,
						 const gchar *old_uri,
						 const gchar *new_uri,
						 GCompareFunc compare);

void		e_rule_context_free_uri_list	(ERuleContext *context,
						 GList *uris);

G_END_DECLS

#endif /* E_RULE_CONTEXT_H */
