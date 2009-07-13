/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_IMAGE_CHOOSER_H_
#define _E_IMAGE_CHOOSER_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_IMAGE_CHOOSER		(e_image_chooser_get_type ())
#define E_IMAGE_CHOOSER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_IMAGE_CHOOSER, EImageChooser))
#define E_IMAGE_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_IMAGE_CHOOSER, EImageChooserClass))
#define E_IS_IMAGE_CHOOSER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_IMAGE_CHOOSER))
#define E_IS_IMAGE_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_IMAGE_CHOOSER))

typedef struct _EImageChooser        EImageChooser;
typedef struct _EImageChooserClass   EImageChooserClass;
typedef struct _EImageChooserPrivate EImageChooserPrivate;

struct _EImageChooser
{
	GtkVBox parent;

	EImageChooserPrivate *priv;
};

struct _EImageChooserClass
{
	GtkVBoxClass parent_class;

	/* signals */
	void (*changed) (EImageChooser *chooser);

};

GtkWidget *e_image_chooser_new      (void);
GType      e_image_chooser_get_type (void);

gboolean   e_image_chooser_set_from_file  (EImageChooser *chooser, const gchar *filename);
gboolean   e_image_chooser_set_image_data (EImageChooser *chooser, gchar *data, gsize data_length);
void       e_image_chooser_set_editable   (EImageChooser *chooser, gboolean editable);

gboolean   e_image_chooser_get_image_data (EImageChooser *chooser, gchar **data, gsize *data_length);

#endif /* _E_IMAGE_CHOOSER_H_ */
