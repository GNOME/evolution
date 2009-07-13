/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _FILTER_RULE_H
#define _FILTER_RULE_H

#include <glib.h>
#include <glib-object.h>

#include "filter-part.h"

#define FILTER_TYPE_RULE            (filter_rule_get_type ())
#define FILTER_RULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILTER_TYPE_RULE, FilterRule))
#define FILTER_RULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILTER_TYPE_RULE, FilterRuleClass))
#define IS_FILTER_RULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILTER_TYPE_RULE))
#define IS_FILTER_RULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILTER_TYPE_RULE))
#define FILTER_RULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILTER_TYPE_RULE, FilterRuleClass))

struct _RuleContext;

typedef struct _FilterRule	FilterRule;
typedef struct _FilterRuleClass	FilterRuleClass;

enum _filter_grouping_t {
	FILTER_GROUP_ALL,	/* all rules must match */
	FILTER_GROUP_ANY	/* any rule must match */
};

/* threading, if the context supports it */
enum _filter_threading_t {
	FILTER_THREAD_NONE,	/* don't add any thread matching */
	FILTER_THREAD_ALL,	/* add all possible threads */
	FILTER_THREAD_REPLIES,	/* add only replies */
	FILTER_THREAD_REPLIES_PARENTS,	/* replies plus parents */
	FILTER_THREAD_SINGLE	/* messages with no replies or parents */
};

#define FILTER_SOURCE_INCOMING "incoming" /* performed on incoming email */
#define FILTER_SOURCE_DEMAND   "demand"   /* performed on the selected folder
					   * when the user asks for it */
#define	FILTER_SOURCE_OUTGOING  "outgoing"/* performed on outgoing mail */
#define	FILTER_SOURCE_JUNKTEST  "junktest"/* perform only junktest on incoming mail */

struct _FilterRule {
	GObject parent_object;
	struct _FilterRulePrivate *priv;

	gchar *name;
	gchar *source;

	enum _filter_grouping_t grouping;
	enum _filter_threading_t threading;

	guint system:1;	/* this is a system rule, cannot be edited/deleted */
	GList *parts;

	gboolean enabled;
};

struct _FilterRuleClass {
	GObjectClass parent_class;

	/* virtual methods */
	gint (*validate) (FilterRule *);
	gint (*eq) (FilterRule *fr, FilterRule *cm);

	xmlNodePtr (*xml_encode) (FilterRule *);
	gint (*xml_decode) (FilterRule *, xmlNodePtr, struct _RuleContext *);

	void (*build_code) (FilterRule *, GString *out);

	void (*copy) (FilterRule *dest, FilterRule *src);

	GtkWidget *(*get_widget) (FilterRule *fr, struct _RuleContext *f);

	/* signals */
	void (*changed) (FilterRule *fr);
};

GType       filter_rule_get_type     (void);
FilterRule *filter_rule_new          (void);

FilterRule *filter_rule_clone        (FilterRule *base);

/* methods */
void        filter_rule_set_name     (FilterRule *fr, const gchar *name);
void        filter_rule_set_source   (FilterRule *fr, const gchar *source);

gint         filter_rule_validate     (FilterRule *fr);
gint         filter_rule_eq           (FilterRule *fr, FilterRule *cm);

xmlNodePtr  filter_rule_xml_encode   (FilterRule *fr);
gint         filter_rule_xml_decode   (FilterRule *fr, xmlNodePtr node, struct _RuleContext *f);

void        filter_rule_copy         (FilterRule *dest, FilterRule *src);

void        filter_rule_add_part     (FilterRule *fr, FilterPart *fp);
void        filter_rule_remove_part  (FilterRule *fr, FilterPart *fp);
void        filter_rule_replace_part (FilterRule *fr, FilterPart *fp, FilterPart *new);

GtkWidget  *filter_rule_get_widget   (FilterRule *fr, struct _RuleContext *f);

void        filter_rule_build_code   (FilterRule *fr, GString *out);
/*
void  filter_rule_build_action(FilterRule *fr, GString *out);
*/

void        filter_rule_emit_changed (FilterRule *fr);

/* static functions */
FilterRule *filter_rule_next_list    (GList *l, FilterRule *last, const gchar *source);
FilterRule *filter_rule_find_list    (GList *l, const gchar *name, const gchar *source);

#endif /* ! _FILTER_RULE_H */
