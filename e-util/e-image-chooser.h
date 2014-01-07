/*
 * e-image-chooser.h
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

#ifndef E_IMAGE_CHOOSER_H
#define E_IMAGE_CHOOSER_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_IMAGE_CHOOSER \
	(e_image_chooser_get_type ())
#define E_IMAGE_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_IMAGE_CHOOSER, EImageChooser))
#define E_IMAGE_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_IMAGE_CHOOSER, EImageChooserClass))
#define E_IS_IMAGE_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_IMAGE_CHOOSER))
#define E_IS_IMAGE_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_IMAGE_CHOOSER))
#define E_IMAGE_CHOOSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_IMAGE_CHOOSER, EImageChooserClass))

G_BEGIN_DECLS

typedef struct _EImageChooser EImageChooser;
typedef struct _EImageChooserClass EImageChooserClass;
typedef struct _EImageChooserPrivate EImageChooserPrivate;

struct _EImageChooser {
	GtkBox parent;
	EImageChooserPrivate *priv;
};

struct _EImageChooserClass {
	GtkBoxClass parent_class;

	/* signals */
	void (*changed) (EImageChooser *chooser);
};

GType		e_image_chooser_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_image_chooser_new		(const gchar *icon_name);
const gchar *	e_image_chooser_get_icon_name	(EImageChooser *chooser);
gboolean	e_image_chooser_set_from_file	(EImageChooser *chooser,
						 const gchar *filename);
gboolean	e_image_chooser_set_image_data	(EImageChooser *chooser,
						 gchar *data,
						 gsize data_length);
gboolean	e_image_chooser_get_image_data	(EImageChooser *chooser,
						 gchar **data,
						 gsize *data_length);

#endif /* E_IMAGE_CHOOSER_H */
