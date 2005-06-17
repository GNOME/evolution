/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-combo-text.h - A combo box for selecting from a list.
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _GAL_COMBO_TEXT_H
#define _GAL_COMBO_TEXT_H

#include <widgets/misc/gal-combo-box.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GAL_COMBO_TEXT_TYPE         (gal_combo_text_get_type ())
#define GAL_COMBO_TEXT(obj)	    G_TYPE_CHECK_INSTANCE_CAST (obj, gal_combo_text_get_type (), GalComboText)
#define GAL_COMBO_TEXT_CLASS(klass) G_TYPE_CHECK_CLASS_CAST (klass, gal_combo_text_get_type (), GalComboTextClass)
#define GAL_IS_COMBO_TEXT(obj)      G_TYPE_CHECK_INSTANCE_TYPE (obj, gal_combo_text_get_type ())

typedef struct _GalComboText	   GalComboText;
/* typedef struct _GalComboTextPrivate GalComboTextPrivate;*/
typedef struct _GalComboTextClass  GalComboTextClass;

struct _GalComboText {
	GalComboBox parent;

	GtkWidget *entry;
	GtkWidget *list;
	GtkWidget *scrolled_window;
	GtkStateType cache_mouse_state;
	GtkWidget *cached_entry;
	gboolean case_sensitive;
	GHashTable*elements;
};

struct _GalComboTextClass {
	GalComboBoxClass parent_class;
};


GtkType    gal_combo_text_get_type  (void);
GtkWidget *gal_combo_text_new       (gboolean const is_scrolled);
void       gal_combo_text_construct (GalComboText *ct, gboolean const is_scrolled);

gint       gal_combo_text_set_case_sensitive (GalComboText *combo_text,
					      gboolean val);
void       gal_combo_text_select_item (GalComboText *combo_text,
				       int elem);
void       gal_combo_text_set_text (GalComboText *combo_text,
				       const gchar *text);
void       gal_combo_text_add_item    (GalComboText *combo_text,
				       const gchar *item,
				       const gchar *value);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif
