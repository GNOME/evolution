/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UI_MENU_H
#define E_UI_MENU_H

#include <gio/gio.h>

#include <e-util/e-ui-manager.h>
#include <e-util/e-ui-action.h>

G_BEGIN_DECLS

#define E_TYPE_UI_MENU e_ui_menu_get_type ()

G_DECLARE_FINAL_TYPE (EUIMenu, e_ui_menu, E, UI_MENU, GMenuModel)

EUIMenu *	e_ui_menu_new			(EUIManager *manager,
						 const gchar *id);
EUIManager *	e_ui_menu_get_manager		(EUIMenu *self);
const gchar *	e_ui_menu_get_id		(EUIMenu *self);
void		e_ui_menu_append_item		(EUIMenu *self,
						 EUIAction *action,
						 GMenuItem *item);
void		e_ui_menu_append_section	(EUIMenu *self,
						 GMenuModel *section);
void		e_ui_menu_track_action		(EUIMenu *self,
						 EUIAction *action);
void		e_ui_menu_freeze		(EUIMenu *self);
void		e_ui_menu_thaw			(EUIMenu *self);
gboolean	e_ui_menu_is_frozen		(EUIMenu *self);
void		e_ui_menu_rebuild		(EUIMenu *self);
void		e_ui_menu_remove_all		(EUIMenu *self);

G_END_DECLS

#endif /* E_UI_MENU_H */
