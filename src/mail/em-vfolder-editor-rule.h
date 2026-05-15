/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: NotZed <notzed@ximian.com>
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#ifndef EM_VFOLDER_EDITOR_RULE_H
#define EM_VFOLDER_EDITOR_RULE_H

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define EM_TYPE_VFOLDER_EDITOR_RULE \
	(em_vfolder_editor_rule_get_type ())
#define EM_VFOLDER_EDITOR_RULE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_VFOLDER_EDITOR_RULE, EMVFolderEditorRule))
#define EM_VFOLDER_EDITOR_RULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_VFOLDER_EDITOR_RULE, EMVFolderEditorRuleClass))
#define EM_IS_VFOLDER_EDITOR_RULE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_VFOLDER_EDITOR_RULE))
#define EM_IS_VFOLDER_EDITOR_RULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_VFOLDER_EDITOR_RULE))
#define EM_VFOLDER_EDITOR_RULE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_VFOLDER_EDITOR_RULE, EMVFolderEditorRuleClass))

G_BEGIN_DECLS

/* perhaps should be bits? */
enum _em_vfolder_editor_rule_with_t {
	EM_VFOLDER_EDITOR_RULE_WITH_SPECIFIC,
	EM_VFOLDER_EDITOR_RULE_WITH_LOCAL_REMOTE_ACTIVE,
	EM_VFOLDER_EDITOR_RULE_WITH_REMOTE_ACTIVE,
	EM_VFOLDER_EDITOR_RULE_WITH_LOCAL
};

typedef struct _EMVFolderEditorRule EMVFolderEditorRule;
typedef struct _EMVFolderEditorRuleClass EMVFolderEditorRuleClass;
typedef struct _EMVFolderEditorRulePrivate EMVFolderEditorRulePrivate;

typedef enum _em_vfolder_editor_rule_with_t em_vfolder_editor_rule_with_t;

struct _EMVFolderEditorRule {
	EMVFolderRule rule;
	EMVFolderEditorRulePrivate *priv;
};

struct _EMVFolderEditorRuleClass {
	EMVFolderRuleClass parent_class;
};

GType		em_vfolder_editor_rule_get_type	(void);
EFilterRule *	em_vfolder_editor_rule_new		(EMailSession *session);
EMailSession *	em_vfolder_editor_rule_get_session	(EMVFolderEditorRule *rule);

G_END_DECLS

#endif /* EM_VFOLDER_EDITOR_RULE_H */
