/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-editor-fullname.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#ifndef __E_TABLE_FIELD_CHOOSER_H__
#define __E_TABLE_FIELD_CHOOSER_H__

#include <gnome.h>
#include <glade/glade.h>
#include "e-table-header.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* ETableFieldChooser - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_TABLE_FIELD_CHOOSER_TYPE			(e_table_field_chooser_get_type ())
#define E_TABLE_FIELD_CHOOSER(obj)			(GTK_CHECK_CAST ((obj), E_TABLE_FIELD_CHOOSER_TYPE, ETableFieldChooser))
#define E_TABLE_FIELD_CHOOSER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TABLE_FIELD_CHOOSER_TYPE, ETableFieldChooserClass))
#define E_IS_TABLE_FIELD_CHOOSER(obj)		(GTK_CHECK_TYPE ((obj), E_TABLE_FIELD_CHOOSER_TYPE))
#define E_IS_TABLE_FIELD_CHOOSER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TABLE_FIELD_CHOOSER_TYPE))


typedef struct _ETableFieldChooser       ETableFieldChooser;
typedef struct _ETableFieldChooserClass  ETableFieldChooserClass;

struct _ETableFieldChooser
{
	GtkVBox parent;
	
	/* item specific fields */
	GladeXML *gui;
	GnomeCanvas *canvas;
	GnomeCanvasItem *item;

	GnomeCanvasItem *rect;
	GtkAllocation last_alloc;

	gchar *dnd_code;
	ETableHeader *full_header;
};

struct _ETableFieldChooserClass
{
	GtkVBoxClass parent_class;
};


GtkWidget *e_table_field_chooser_new(void);
GtkType    e_table_field_chooser_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_TABLE_FIELD_CHOOSER_H__ */
