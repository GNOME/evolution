/*
 * Copyright (C) 2020 Red Hat (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ELLIPSIZED_COMBO_BOX_TEXT_H
#define E_ELLIPSIZED_COMBO_BOX_TEXT_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_ELLIPSIZED_COMBO_BOX_TEXT \
	(e_ellipsized_combo_box_text_get_type ())
#define E_ELLIPSIZED_COMBO_BOX_TEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ELLIPSIZED_COMBO_BOX_TEXT, EEllipsizedComboBoxText))
#define E_ELLIPSIZED_COMBO_BOX_TEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ELLIPSIZED_COMBO_BOX_TEXT, EEllipsizedComboBoxTextClass))
#define E_IS_ELLIPSIZED_COMBO_BOX_TEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ELLIPSIZED_COMBO_BOX_TEXT))
#define E_IS_ELLIPSIZED_COMBO_BOX_TEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ELLIPSIZED_COMBO_BOX_TEXT))
#define E_ELLIPSIZED_COMBO_BOX_TEXT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ELLIPSIZED_COMBO_BOX_TEXT, EEllipsizedComboBoxTextClass))

G_BEGIN_DECLS

typedef struct _EEllipsizedComboBoxText EEllipsizedComboBoxText;
typedef struct _EEllipsizedComboBoxTextClass EEllipsizedComboBoxTextClass;
typedef struct _EEllipsizedComboBoxTextPrivate EEllipsizedComboBoxTextPrivate;

struct _EEllipsizedComboBoxText {
	GtkComboBoxText parent;
	EEllipsizedComboBoxTextPrivate *priv;
};

struct _EEllipsizedComboBoxTextClass {
	GtkComboBoxTextClass parent_class;
};

GType		e_ellipsized_combo_box_text_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_ellipsized_combo_box_text_new		(gboolean has_entry);
gint		e_ellipsized_combo_box_text_get_max_natural_width
							(EEllipsizedComboBoxText *combo_box);
void		e_ellipsized_combo_box_text_set_max_natural_width
							(EEllipsizedComboBoxText *combo_box,
							 gint max_natural_width);

G_END_DECLS

#endif /* E_ELLIPSIZED_COMBO_BOX_TEXT_H */
