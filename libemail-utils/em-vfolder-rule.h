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
 *		NotZed <notzed@ximian.com>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_VFOLDER_RULE_H
#define EM_VFOLDER_RULE_H

#include <filter/e-filter-rule.h>

/* Standard GObject macros */
#define EM_TYPE_VFOLDER_RULE \
	(em_vfolder_rule_get_type ())
#define EM_VFOLDER_RULE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_VFOLDER_RULE, EMVFolderRule))
#define EM_VFOLDER_RULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_VFOLDER_RULE, EMVFolderRuleClass))
#define EM_IS_VFOLDER_RULE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_VFOLDER_RULE))
#define EM_IS_VFOLDER_RULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_VFOLDER_RULE))
#define EM_VFOLDER_RULE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_VFOLDER_RULE, EMVFolderRuleClass))

G_BEGIN_DECLS

/* perhaps should be bits? */
enum _em_vfolder_rule_with_t {
	EM_VFOLDER_RULE_WITH_SPECIFIC,
	EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE,
	EM_VFOLDER_RULE_WITH_REMOTE_ACTIVE,
	EM_VFOLDER_RULE_WITH_LOCAL
};

typedef struct _EMVFolderRule EMVFolderRule;
typedef struct _EMVFolderRuleClass EMVFolderRuleClass;
typedef struct _EMVFolderRulePrivate EMVFolderRulePrivate;

typedef enum _em_vfolder_rule_with_t em_vfolder_rule_with_t;

struct _EMVFolderRule {
	EFilterRule rule;
	EMVFolderRulePrivate *priv;
};

struct _EMVFolderRuleClass {
	EFilterRuleClass parent_class;
};

GType		em_vfolder_rule_get_type	(void);
EFilterRule *	em_vfolder_rule_new		();
void		em_vfolder_rule_add_source	(EMVFolderRule *rule,
						 const gchar *uri);
void		em_vfolder_rule_remove_source	(EMVFolderRule *rule,
						 const gchar *uri);
const gchar *	em_vfolder_rule_find_source	(EMVFolderRule *rule,
						 const gchar *uri);
const gchar *	em_vfolder_rule_next_source	(EMVFolderRule *rule,
						 const gchar *last);
void		em_vfolder_rule_set_with	(EMVFolderRule *rule,
						 em_vfolder_rule_with_t with);
em_vfolder_rule_with_t
		em_vfolder_rule_get_with	(EMVFolderRule *rule);
GQueue *	em_vfolder_rule_get_sources	(EMVFolderRule *rule);
void		em_vfolder_rule_set_autoupdate	(EMVFolderRule *rule,
						 gboolean autoupdate);
gboolean	em_vfolder_rule_get_autoupdate	(EMVFolderRule *rule);

G_END_DECLS

#endif /* EM_VFOLDER_RULE_H */
