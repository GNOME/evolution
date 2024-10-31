/* e-source-combo-box.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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

#ifndef E_SOURCE_COMBO_BOX_H
#define E_SOURCE_COMBO_BOX_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#define E_TYPE_SOURCE_COMBO_BOX \
	(e_source_combo_box_get_type ())
#define E_SOURCE_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_COMBO_BOX, ESourceComboBox))
#define E_SOURCE_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_COMBO_BOX, ESourceComboBoxClass))
#define E_IS_SOURCE_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SOURCE_COMBO_BOX))
#define E_IS_SOURCE_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE ((cls), E_TYPE_SOURCE_COMBO_BOX))
#define E_SOURCE_COMBO_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_COMBO_BOX, ESourceComboBox))

G_BEGIN_DECLS

typedef struct _ESourceComboBox ESourceComboBox;
typedef struct _ESourceComboBoxClass ESourceComboBoxClass;
typedef struct _ESourceComboBoxPrivate ESourceComboBoxPrivate;

/**
 * ESourceComboBox:
 *
 * Since: 2.22
 **/
struct _ESourceComboBox {
	GtkComboBox parent;
	ESourceComboBoxPrivate *priv;
};

struct _ESourceComboBoxClass {
	GtkComboBoxClass parent_class;
};

GType		e_source_combo_box_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_source_combo_box_new		(ESourceRegistry *registry,
						 const gchar *extension_name);
ESourceRegistry *
		e_source_combo_box_get_registry	(ESourceComboBox *combo_box);
void		e_source_combo_box_set_registry	(ESourceComboBox *combo_box,
						 ESourceRegistry *registry);
const gchar *	e_source_combo_box_get_extension_name
						(ESourceComboBox *combo_box);
void		e_source_combo_box_set_extension_name
						(ESourceComboBox *combo_box,
						 const gchar *extension_name);
gboolean	e_source_combo_box_get_show_colors
						(ESourceComboBox *combo_box);
void		e_source_combo_box_set_show_colors
						(ESourceComboBox *combo_box,
						 gboolean show_colors);
ESource *	e_source_combo_box_ref_active	(ESourceComboBox *combo_box);
void		e_source_combo_box_set_active	(ESourceComboBox *combo_box,
						 ESource *source);
void		e_source_combo_box_hide_sources	(ESourceComboBox *combo_box,
						 ...) G_GNUC_NULL_TERMINATED;
gint		e_source_combo_box_get_max_natural_width
						(ESourceComboBox *combo_box);
void		e_source_combo_box_set_max_natural_width
						(ESourceComboBox *combo_box,
						 gint value);
gboolean	e_source_combo_box_get_show_full_name
						(ESourceComboBox *combo_box);
void		e_source_combo_box_set_show_full_name
						(ESourceComboBox *combo_box,
						 gboolean show_full_name);

G_END_DECLS

#endif /* E_SOURCE_COMBO_BOX_H */
