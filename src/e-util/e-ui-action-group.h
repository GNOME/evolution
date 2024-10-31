/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UI_ACTION_GROUP_H
#define E_UI_ACTION_GROUP_H

#include <gio/gio.h>
#include <e-util/e-ui-action.h>

G_BEGIN_DECLS

#define E_TYPE_UI_ACTION_GROUP e_ui_action_group_get_type ()

G_DECLARE_FINAL_TYPE (EUIActionGroup, e_ui_action_group, E, UI_ACTION_GROUP, GSimpleActionGroup)

EUIActionGroup *e_ui_action_group_new		(const gchar *name);
const gchar *	e_ui_action_group_get_name	(EUIActionGroup *self);
void		e_ui_action_group_add		(EUIActionGroup *self,
						 EUIAction *action);
void		e_ui_action_group_remove	(EUIActionGroup *self,
						 EUIAction *action);
void		e_ui_action_group_remove_by_name(EUIActionGroup *self,
						 const gchar *action_name);
void		e_ui_action_group_remove_all	(EUIActionGroup *self);
EUIAction *	e_ui_action_group_get_action	(EUIActionGroup *self,
						 const gchar *action_name);
GPtrArray *	e_ui_action_group_list_actions	(EUIActionGroup *self);
gboolean	e_ui_action_group_get_visible	(EUIActionGroup *self);
void		e_ui_action_group_set_visible	(EUIActionGroup *self,
						 gboolean value);
gboolean	e_ui_action_group_get_sensitive	(EUIActionGroup *self);
void		e_ui_action_group_set_sensitive	(EUIActionGroup *self,
						 gboolean value);

G_END_DECLS

#endif /* E_UI_ACTION_GROUP_H */
