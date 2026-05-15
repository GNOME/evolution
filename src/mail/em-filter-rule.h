/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Not Zed <notzed@lostzed.mmc.com.au>
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#ifndef EM_FILTER_RULE_H
#define EM_FILTER_RULE_H

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#define EM_TYPE_FILTER_RULE \
	(em_filter_rule_get_type ())
#define EM_FILTER_RULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EM_TYPE_FILTER_RULE, EMFilterRule))
#define EM_FILTER_RULE_CLASS(cls)    (G_TYPE_CHECK_CLASS_CAST ((cls), EM_TYPE_FILTER_RULE, EMFilterRuleClass))
#define EM_IS_FILTER_RULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EM_TYPE_FILTER_RULE))
#define EM_IS_FILTER_RULE_CLASS(cls) (G_TYPE_CHECK_CLASS_TYPE ((cls), EM_TYPE_FILTER_RULE))
#define EM_FILTER_RULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EM_TYPE_FILTER_RULE, EMFilterRuleClass))

typedef struct _EMFilterRule EMFilterRule;
typedef struct _EMFilterRuleClass EMFilterRuleClass;
typedef struct _EMFilterRulePrivate EMFilterRulePrivate;

struct _EMFilterRule {
	EFilterRule parent_object;

	EMFilterRulePrivate *priv;
};

struct _EMFilterRuleClass {
	EFilterRuleClass parent_class;
};

GType           em_filter_rule_get_type (void);
EFilterRule *	em_filter_rule_new      (void);

/* methods */
void            em_filter_rule_add_action     (EMFilterRule *fr, EFilterPart *fp);
void            em_filter_rule_remove_action  (EMFilterRule *fr, EFilterPart *fp);
void            em_filter_rule_replace_action (EMFilterRule *fr, EFilterPart *fp, EFilterPart *new);

void            em_filter_rule_build_action   (EMFilterRule *fr, GString *out);

GList *		em_filter_rule_get_actions	(EMFilterRule *rule);
const gchar *	em_filter_rule_get_account_uid	(EMFilterRule *rule);
void		em_filter_rule_set_account_uid	(EMFilterRule *rule,
						 const gchar *account_uid);

#endif /* EM_FILTER_RULE_H */
