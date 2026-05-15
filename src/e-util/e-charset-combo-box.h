/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CHARSET_COMBO_BOX_H
#define E_CHARSET_COMBO_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_CHARSET_COMBO_BOX (e_charset_combo_box_get_type ())
G_DECLARE_FINAL_TYPE (ECharsetComboBox, e_charset_combo_box, E, CHARSET_COMBO_BOX, GtkComboBox)

GtkWidget *	e_charset_combo_box_new		(void);
const gchar *	e_charset_combo_box_get_charset	(ECharsetComboBox *combo_box);
void		e_charset_combo_box_set_charset	(ECharsetComboBox *combo_box,
						 const gchar *charset);

G_END_DECLS

#endif /* E_CHARSET_COMBO_BOX_H */
