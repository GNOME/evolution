/*
 * SPDX-FileCopyrightText: (C) 2012 Dan Vrátil <dvratil@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_COLOR_COMBO_H
#define E_COLOR_COMBO_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_COLOR_COMBO \
	(e_color_combo_get_type ())
#define E_COLOR_COMBO(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COLOR_COMBO, EColorCombo))
#define E_COLOR_COMBO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COLOR_COMBO, EColorComboClass))
#define E_IS_COLOR_COMBO(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COLOR_COMBO))
#define E_IS_COLOR_COMBO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COLOR_COMBO))
#define E_COLOR_COMBO_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COLOR_COMBO, EColorComboClass))

G_BEGIN_DECLS

typedef struct _EColorCombo EColorCombo;
typedef struct _EColorComboClass EColorComboClass;
typedef struct _EColorComboPrivate EColorComboPrivate;

struct _EColorCombo {
	GtkButton parent;
	EColorComboPrivate *priv;
};

struct _EColorComboClass {
	GtkButtonClass parent_class;

	void		(*popup)		(EColorCombo *combo);
	void		(*popdown)		(EColorCombo *combo);
	void		(*activated)		(EColorCombo *combo,
						 GdkRGBA *color);
};

GType		e_color_combo_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_color_combo_new		(void);
GtkWidget *	e_color_combo_new_defaults	(GdkRGBA *default_color,
						 const gchar *default_label);
void		e_color_combo_popup		(EColorCombo *combo);
void		e_color_combo_popdown		(EColorCombo *combo);
void		e_color_combo_get_current_color	(EColorCombo *combo,
						 GdkRGBA *rgba);
void		e_color_combo_set_current_color	(EColorCombo *combo,
						 const GdkRGBA *color);
void		e_color_combo_get_default_color	(EColorCombo *combo,
						 GdkRGBA *color);
void		e_color_combo_set_default_color	(EColorCombo *combo,
						 const GdkRGBA *default_color);
const gchar *	e_color_combo_get_default_label	(EColorCombo *combo);
void		e_color_combo_set_default_label	(EColorCombo *combo,
						 const gchar *text);
gboolean	e_color_combo_get_default_transparent
						(EColorCombo *combo);
void		e_color_combo_set_default_transparent
						(EColorCombo *combo,
						 gboolean transparent);
GList	*	e_color_combo_get_palette	(EColorCombo *combo);
void		e_color_combo_set_palette	(EColorCombo *combo,
						 GList *palette);

G_END_DECLS

#endif /* E_COLOR_COMBO_H */
