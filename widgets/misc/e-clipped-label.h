/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * This is similar to GtkLabel but clips itself and displays '...' if it
 * can't fit inside its allocated area. The intended use is for inside buttons
 * that are a fixed size. The GtkLabel would normally display only the middle
 * part of the text, which doesn't look very good. This only supports one line
 * of text (so no wrapping/justification), without underlined characters.
 */
#ifndef _E_CLIPPED_LABEL_H_
#define _E_CLIPPED_LABEL_H_

#include <gtk/gtkmisc.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_CLIPPED_LABEL(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, e_clipped_label_get_type (), EClippedLabel)
#define E_CLIPPED_LABEL_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, e_clipped_label_get_type (), EClippedLabelClass)
#define E_IS_CLIPPED_LABEL(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, e_clipped_label_get_type ())


typedef struct _EClippedLabel       EClippedLabel;
typedef struct _EClippedLabelClass  EClippedLabelClass;

struct _EClippedLabel
{
	GtkMisc misc;

	gchar *label;

	/* Font size multiplication factor; 1.0 means "default GTK font
	   size"  */
	gfloat font_size;
	PangoWeight font_weight;

	/* Our PangoLayout */
	PangoLayout *layout;

	/* This is the width of the entire label string, in pixels. */
	gint label_width;

	/* This is the label's y coord.  we store it here so it won't
	   change as the label's baseline changes (for example if we
	   ellide the 'y' from 'Summary' the baseline drops) */
	gint label_y;

	/* This is the number of characters we can fit in, or
	   E_CLIPPED_LABEL_NEED_RECALC if it needs to be recalculated, or
	   E_CLIPPED_LABEL_SHOW_ENTIRE_LABEL to show the entire label. */
	gint chars_displayed;

	/* This is the x position to display the ellipsis string, e.g. '...',
	   relative to the start of the label. */
	gint ellipsis_x;

	/* This is the width of the ellipsis, in pixels */
	gint ellipsis_width;
};

struct _EClippedLabelClass
{
	GtkMiscClass parent_class;
};


GtkType    e_clipped_label_get_type  (void);
GtkWidget *e_clipped_label_new       (const gchar   *text,
				      PangoWeight    font_weight,
				      gfloat         font_size);

gchar     *e_clipped_label_get_text  (EClippedLabel *label);
void       e_clipped_label_set_text  (EClippedLabel *label,
				      const gchar   *text);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_CLIPPED_LABEL_H_ */
