/*
 * e-charset-combo-box.h
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
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CHARSET_COMBO_BOX_H
#define E_CHARSET_COMBO_BOX_H

#include <e-util/e-action-combo-box.h>

/* Standard GObject macros */
#define E_TYPE_CHARSET_COMBO_BOX \
	(e_charset_combo_box_get_type ())
#define E_CHARSET_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CHARSET_COMBO_BOX, ECharsetComboBox))
#define E_CHARSET_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CHARSET_COMBO_BOX, ECharsetComboBoxClass))
#define E_IS_CHARSET_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CHARSET_COMBO_BOX))
#define E_IS_CHARSET_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CHARSET_COMBO_BOX))
#define E_CHARSET_COMBO_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CHARSET_COMBO_BOX, ECharsetComboBoxClass))

G_BEGIN_DECLS

typedef struct _ECharsetComboBox ECharsetComboBox;
typedef struct _ECharsetComboBoxClass ECharsetComboBoxClass;
typedef struct _ECharsetComboBoxPrivate ECharsetComboBoxPrivate;

struct _ECharsetComboBox {
	EActionComboBox parent;
	ECharsetComboBoxPrivate *priv;
};

struct _ECharsetComboBoxClass {
	EActionComboBoxClass parent_class;
};

GType		e_charset_combo_box_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_charset_combo_box_new		(void);
const gchar *	e_charset_combo_box_get_charset	(ECharsetComboBox *combo_box);
void		e_charset_combo_box_set_charset	(ECharsetComboBox *combo_box,
						 const gchar *charset);

G_END_DECLS

#endif /* E_CHARSET_COMBO_BOX_H */
