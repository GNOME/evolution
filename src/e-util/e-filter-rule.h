/*
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

#ifndef E_FILTER_RULE_H
#define E_FILTER_RULE_H

#include <e-util/e-filter-part.h>

/* Standard GObject macros */
#define E_TYPE_FILTER_RULE \
	(e_filter_rule_get_type ())
#define E_FILTER_RULE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_RULE, EFilterRule))
#define E_FILTER_RULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_RULE, EFilterRuleClass))
#define E_IS_FILTER_RULE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_RULE))
#define E_IS_FILTER_RULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_RULE))
#define E_FILTER_RULE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_RULE, EFilterRuleClass))

G_BEGIN_DECLS

struct _RuleContext;

typedef struct _EFilterRule EFilterRule;
typedef struct _EFilterRuleClass EFilterRuleClass;
typedef struct _EFilterRulePrivate EFilterRulePrivate;

enum _filter_grouping_t {
	E_FILTER_GROUP_ALL,	/* all rules must match */
	E_FILTER_GROUP_ANY	/* any rule must match */
};

/* threading, if the context supports it */
enum _filter_threading_t {
	E_FILTER_THREAD_NONE,	/* don't add any thread matching */
	E_FILTER_THREAD_ALL,	/* add all possible threads */
	E_FILTER_THREAD_REPLIES,	/* add only replies */
	E_FILTER_THREAD_REPLIES_PARENTS,	/* replies plus parents */
	E_FILTER_THREAD_SINGLE	/* messages with no replies or parents */
};

#define E_FILTER_SOURCE_INCOMING "incoming" /* performed on incoming email */
#define E_FILTER_SOURCE_DEMAND   "demand"   /* performed on the selected folder
					     * when the user asks for it */
#define E_FILTER_SOURCE_OUTGOING  "outgoing"/* performed on outgoing mail */
#define E_FILTER_SOURCE_JUNKTEST  "junktest"/* check incoming mail for junk */

struct _EFilterRule {
	GObject parent_object;
	EFilterRulePrivate *priv;

	gchar *name;
	gchar *source;

	enum _filter_grouping_t grouping;
	enum _filter_threading_t threading;

	guint system:1;	/* this is a system rule, cannot be edited/deleted */
	GList *parts;

	gboolean enabled;
};

struct _EFilterRuleClass {
	GObjectClass parent_class;

	/* virtual methods */
	gint		(*validate)		(EFilterRule *rule,
						 EAlert **alert);
	gint		(*eq)			(EFilterRule *rule_a,
						 EFilterRule *rule_b);

	xmlNodePtr	(*xml_encode)		(EFilterRule *rule);
	gint		(*xml_decode)		(EFilterRule *rule,
						 xmlNodePtr node,
						 struct _ERuleContext *context);

	void		(*build_code)		(EFilterRule *rule,
						 GString *out);

	void		(*copy)			(EFilterRule *dst_rule,
						 EFilterRule *src_rule);

	GtkWidget *	(*get_widget)		(EFilterRule *rule,
						 struct _ERuleContext *context);

	/* signals */
	void		(*changed)		(EFilterRule *rule);
};

GType		e_filter_rule_get_type		(void) G_GNUC_CONST;
EFilterRule *	e_filter_rule_new		(void);
EFilterRule *	e_filter_rule_clone		(EFilterRule *rule);
void		e_filter_rule_set_name		(EFilterRule *rule,
						 const gchar *name);
void		e_filter_rule_set_source	(EFilterRule *rule,
						 const gchar *source);
gint		e_filter_rule_validate		(EFilterRule *rule,
						 EAlert **alert);
gint		e_filter_rule_eq		(EFilterRule *rule_a,
						 EFilterRule *rule_b);
xmlNodePtr	e_filter_rule_xml_encode	(EFilterRule *rule);
gint		e_filter_rule_xml_decode	(EFilterRule *rule,
						 xmlNodePtr node,
						 struct _ERuleContext *context);
void		e_filter_rule_copy		(EFilterRule *dst_rule,
						 EFilterRule *src_rule);
void		e_filter_rule_add_part		(EFilterRule *rule,
						 EFilterPart *part);
void		e_filter_rule_remove_part	(EFilterRule *rule,
						 EFilterPart *part);
void		e_filter_rule_replace_part	(EFilterRule *rule,
						 EFilterPart *old_part,
						 EFilterPart *new_part);
GtkWidget *	e_filter_rule_get_widget	(EFilterRule *rule,
						 struct _ERuleContext *context);
void		e_filter_rule_build_code	(EFilterRule *rule,
						 GString *out);
void		e_filter_rule_emit_changed	(EFilterRule *rule);
void		e_filter_rule_persist_customizations
						(EFilterRule *rule);
/* static functions */
EFilterRule *	e_filter_rule_next_list		(GList *list,
						 EFilterRule *last,
						 const gchar *source);
EFilterRule *	e_filter_rule_find_list		(GList *list,
						 const gchar *name,
						 const gchar *source);

G_END_DECLS

#endif /* E_FILTER_RULE_H */
