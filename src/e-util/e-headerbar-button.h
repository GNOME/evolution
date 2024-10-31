/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2022 Cédric Bellegarde <cedric.bellegarde@adishatz.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HEADER_BAR_BUTTON_H
#define E_HEADER_BAR_BUTTON_H

#include <gtk/gtk.h>

#include <e-util/e-ui-action.h>
#include <e-util/e-ui-manager.h>

/* Standard GObject macros */
#define E_TYPE_HEADER_BAR_BUTTON \
	(e_header_bar_button_get_type ())
#define E_HEADER_BAR_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HEADER_BAR_BUTTON, EHeaderBarButton))
#define E_HEADER_BAR_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HEADER_BAR_BUTTON, EHeaderBarButtonClass))
#define E_IS_HEADER_BAR_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HEADER_BAR_BUTTON))
#define E_IS_HEADER_BAR_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HEADER_BAR_BUTTON))
#define E_HEADER_BAR_BUTTON_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HEADER_BAR_BUTTON, EHeaderBarButtonClass))

G_BEGIN_DECLS

typedef struct _EHeaderBarButton EHeaderBarButton;
typedef struct _EHeaderBarButtonClass EHeaderBarButtonClass;
typedef struct _EHeaderBarButtonPrivate EHeaderBarButtonPrivate;

struct _EHeaderBarButton {
	GtkBox parent;
	EHeaderBarButtonPrivate *priv;
};

struct _EHeaderBarButtonClass {
	GtkBoxClass parent_class;
};

GType		e_header_bar_button_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_header_bar_button_new			(const gchar *label,
							 EUIAction *action,
							 EUIManager *ui_manager);
void		e_header_bar_button_add_action		(EHeaderBarButton *header_bar_button,
							 const gchar *label,
							 EUIAction *action);
void		e_header_bar_button_take_menu		(EHeaderBarButton *header_bar_button,
							 GtkWidget *menu);
void		e_header_bar_button_css_add_class	(EHeaderBarButton *header_bar_button,
							 const gchar *class);
void		e_header_bar_button_add_accelerator	(EHeaderBarButton* header_bar_button,
							 GtkAccelGroup* accel_group,
							 guint accel_key,
							 GdkModifierType accel_mods,
							 GtkAccelFlags accel_flags);
void		e_header_bar_button_get_widths		(EHeaderBarButton *self,
							 gint *out_labeled_width,
							 gint *out_icon_only_width);
gboolean	e_header_bar_button_get_show_icon_only	(EHeaderBarButton *self);
void		e_header_bar_button_set_show_icon_only	(EHeaderBarButton *self,
							 gboolean show_icon_only);

G_END_DECLS

#endif /* E_HEADER_BAR_BUTTON_H */
