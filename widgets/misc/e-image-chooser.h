/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-image-chooser.c
 * Copyright (C) 2004  Novell, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _E_IMAGE_CHOOSER_H_
#define _E_IMAGE_CHOOSER_H_

#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

#define E_TYPE_IMAGE_CHOOSER	        (e_image_chooser_get_type ())
#define E_IMAGE_CHOOSER(obj)	        (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_IMAGE_CHOOSER, EImageChooser))
#define E_IMAGE_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_IMAGE_CHOOSER, EImageChooserClass))
#define E_IS_IMAGE_CHOOSER(obj)	        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_IMAGE_CHOOSER))
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

gboolean   e_image_chooser_set_from_file (EImageChooser *chooser, const char *filename);
void       e_image_chooser_set_editable  (EImageChooser *chooser, gboolean editable);

#endif /* _E_IMAGE_CHOOSER_H_ */
