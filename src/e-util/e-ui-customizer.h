/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UI_CUSTOMIZER_H
#define E_UI_CUSTOMIZER_H

#include <gtk/gtk.h>

#include <e-util/e-ui-parser.h>
#include <e-util/e-util-enums.h>

G_BEGIN_DECLS

struct _EUIManager;

#define E_TYPE_UI_CUSTOMIZER e_ui_customizer_get_type ()

G_DECLARE_FINAL_TYPE (EUICustomizer, e_ui_customizer, E, UI_CUSTOMIZER, GObject)

const gchar *	e_ui_customizer_get_filename	(EUICustomizer *self);
struct _EUIManager *
		e_ui_customizer_get_manager	(EUICustomizer *self);
EUIParser *	e_ui_customizer_get_parser	(EUICustomizer *self);
gboolean	e_ui_customizer_load		(EUICustomizer *self,
						 GError **error);
gboolean	e_ui_customizer_save		(EUICustomizer *self,
						 GError **error);
void		e_ui_customizer_register	(EUICustomizer *self,
						 const gchar *id,
						 const gchar *display_name);
GPtrArray *	e_ui_customizer_list_registered	(EUICustomizer *self);  /* gchar *id */
const gchar *	e_ui_customizer_get_registered_display_name
						(EUICustomizer *self,
						 const gchar *id);
EUIElement *	e_ui_customizer_get_element	(EUICustomizer *self,
						 const gchar *id);
GPtrArray *	e_ui_customizer_get_accels	(EUICustomizer *self, /* gchar * */
						 const gchar *action_name);
void		e_ui_customizer_take_accels	(EUICustomizer *self,
						 const gchar *action_name,
						 GPtrArray *accels); /* gchar * */

gchar *		e_ui_customizer_util_dup_filename_for_component
						(const gchar *component);

typedef void	(* EUICustomizeFunc)		(GtkWidget *widget,
						 const gchar *id,
						 gpointer user_data);
void		e_ui_customizer_util_attach_toolbar_context_menu
						(GtkWidget *widget,
						 const gchar *toolbar_id,
						 EUICustomizeFunc func,
						 gpointer user_data);

G_END_DECLS

#endif /* E_UI_CUSTOMIZER_H */
