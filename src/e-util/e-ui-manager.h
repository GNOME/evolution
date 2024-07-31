/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UI_MANAGER_H
#define E_UI_MANAGER_H

#include <gtk/gtk.h>

#include <e-util/e-ui-action.h>
#include <e-util/e-ui-action-group.h>
#include <e-util/e-ui-parser.h>
#include <e-util/e-util-enums.h>

G_BEGIN_DECLS

struct _EUIMenu;

#define E_TYPE_UI_MANAGER e_ui_manager_get_type ()

G_DECLARE_FINAL_TYPE (EUIManager, e_ui_manager, E, UI_MANAGER, GObject)

EUIManager *	e_ui_manager_new		(void);
EUIParser *	e_ui_manager_get_parser		(EUIManager *self);
GtkAccelGroup *	e_ui_manager_get_accel_group	(EUIManager *self);
void		e_ui_manager_freeze		(EUIManager *self);
void		e_ui_manager_thaw		(EUIManager *self);
gboolean	e_ui_manager_is_frozen		(EUIManager *self);
void		e_ui_manager_add_actions	(EUIManager *self,
						 const gchar *group_name,
						 const gchar *translation_domain,
						 const EUIActionEntry *entries,
						 gint n_entries,
						 gpointer user_data);
void		e_ui_manager_add_actions_enum	(EUIManager *self,
						 const gchar *group_name,
						 const gchar *translation_domain,
						 const EUIActionEnumEntry *entries,
						 gint n_entries,
						 gpointer user_data);
void		e_ui_manager_add_action		(EUIManager *self,
						 const gchar *group_name,
						 EUIAction *action,
						 EUIActionFunc activate,
						 EUIActionFunc change_state,
						 gpointer user_data);
void		e_ui_manager_add_actions_with_eui_data
						(EUIManager *self,
						 const gchar *group_name,
						 const gchar *translation_domain,
						 const EUIActionEntry *entries,
						 gint n_entries,
						 gpointer user_data,
						 const gchar *eui);
void		e_ui_manager_add_action_groups_to_widget
						(EUIManager *self,
						 GtkWidget *widget);
void		e_ui_manager_set_action_groups_widget
						(EUIManager *self,
						 GtkWidget *widget);
GtkWidget *	e_ui_manager_ref_action_groups_widget
						(EUIManager *self);
gboolean	e_ui_manager_has_action_group	(EUIManager *self,
						 const gchar *name);
EUIActionGroup *e_ui_manager_get_action_group	(EUIManager *self,
						 const gchar *name);
void		e_ui_manager_add_action_group	(EUIManager *self,
						 EUIActionGroup *action_group);
EUIAction *	e_ui_manager_get_action		(EUIManager *self,
						 const gchar *name);
GIcon *		e_ui_manager_get_gicon		(EUIManager *self,
						 const gchar *name);
GObject *	e_ui_manager_create_item	(EUIManager *self,
						 const gchar *id);
void		e_ui_manager_fill_menu		(EUIManager *self,
						 const gchar *id,
						 struct _EUIMenu *ui_menu);
void		e_ui_manager_update_item_from_action
						(EUIManager *self,
						 gpointer item,
						 EUIAction *action);
GObject *	e_ui_manager_create_item_from_menu_model
						(EUIManager *self,
						 EUIElement *elem,
						 EUIAction *action,
						 EUIElementKind for_kind,
						 GMenuModel *menu_model);

G_END_DECLS

#endif /* E_UI_MANAGER_H */
