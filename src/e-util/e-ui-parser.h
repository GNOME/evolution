/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UI_PARSER_H
#define E_UI_PARSER_H

#include <gtk/gtk.h>

#include <e-util/e-ui-action.h>
#include <e-util/e-util-enums.h>

G_BEGIN_DECLS

#define E_TYPE_UI_PARSER e_ui_parser_get_type ()

G_DECLARE_FINAL_TYPE (EUIParser, e_ui_parser, E, UI_PARSER, GObject)

typedef struct _EUIElement EUIElement;

#define E_TYPE_UI_ELEMENT e_ui_element_get_type ()
GType		e_ui_element_get_type		(void) G_GNUC_CONST;
EUIElement *	e_ui_element_copy		(const EUIElement *src);
void		e_ui_element_free		(EUIElement *self);
EUIElement *	e_ui_element_new_separator	(void);
EUIElement *	e_ui_element_new_for_action	(EUIAction *action);
void		e_ui_element_add_child		(EUIElement *self,
						 EUIElement *child);
gboolean	e_ui_element_remove_child	(EUIElement *self,
						 EUIElement *child);
gboolean	e_ui_element_remove_child_by_id	(EUIElement *self,
						 const gchar *id);

/* generic EUIElement functions, usable for any element */
EUIElementKind	e_ui_element_get_kind		(const EUIElement *self);
const gchar *	e_ui_element_get_id		(const EUIElement *self);
guint		e_ui_element_get_n_children	(const EUIElement *self);
EUIElement *	e_ui_element_get_child		(EUIElement *self,
						 guint index);
EUIElement *	e_ui_element_get_child_by_id	(EUIElement *self,
						 const gchar *id);
/* menu element functions */
gboolean	e_ui_element_menu_get_is_popup	(const EUIElement *self);
/* submenu element functions */
const gchar *	e_ui_element_submenu_get_action	(const EUIElement *self);
/* headerbar element functions */
gboolean	e_ui_element_headerbar_get_use_gtk_type
						(const EUIElement *self);
/* toolbar element functions */
gboolean	e_ui_element_toolbar_get_primary(const EUIElement *self);
/* item element functions */
guint		e_ui_element_item_get_label_priority
						(const EUIElement *self);
gint		e_ui_element_item_get_order	(const EUIElement *self);
void		e_ui_element_item_set_order	(EUIElement *self,
						 gint order);
gboolean	e_ui_element_item_get_icon_only_is_set
						(const EUIElement *self);
gboolean	e_ui_element_item_get_icon_only (const EUIElement *self);
gboolean	e_ui_element_item_get_text_only_is_set
						(const EUIElement *self);
gboolean	e_ui_element_item_get_text_only	(const EUIElement *self);
gboolean	e_ui_element_item_get_important	(const EUIElement *self);
const gchar *	e_ui_element_item_get_css_classes
						(const EUIElement *self);
const gchar *	e_ui_element_item_get_action	(const EUIElement *self);
const gchar *	e_ui_element_item_get_group	(const EUIElement *self);

/* parser functions */
EUIParser *	e_ui_parser_new			(void);
gboolean	e_ui_parser_merge_file		(EUIParser *self,
						 const gchar *filename,
						 GError **error);
gboolean	e_ui_parser_merge_data		(EUIParser *self,
						 const gchar *data,
						 gssize data_len,
						 GError **error);
void		e_ui_parser_clear		(EUIParser *self);
EUIElement *	e_ui_parser_get_root		(EUIParser *self);
EUIElement *	e_ui_parser_create_root		(EUIParser *self);
GPtrArray *	e_ui_parser_get_accels		(EUIParser *self, /* gchar * */
						 const gchar *action_name);
void		e_ui_parser_take_accels		(EUIParser *self,
						 const gchar *action_name,
						 GPtrArray *accels); /* gchar * */
gchar *		e_ui_parser_export		(EUIParser *self,
						 EUIParserExportFlags flags);

G_END_DECLS

#endif /* E_UI_PARSER_H */
