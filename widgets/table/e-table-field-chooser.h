/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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

#ifndef __E_TABLE_FIELD_CHOOSER_H__
#define __E_TABLE_FIELD_CHOOSER_H__

#include <glade/glade.h>
#include <gtk/gtkvbox.h>
#include <table/e-table-header.h>

G_BEGIN_DECLS

/* ETableFieldChooser - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_TABLE_FIELD_CHOOSER_TYPE			(e_table_field_chooser_get_type ())
#define E_TABLE_FIELD_CHOOSER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TABLE_FIELD_CHOOSER_TYPE, ETableFieldChooser))
#define E_TABLE_FIELD_CHOOSER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TABLE_FIELD_CHOOSER_TYPE, ETableFieldChooserClass))
#define E_IS_TABLE_FIELD_CHOOSER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TABLE_FIELD_CHOOSER_TYPE))
#define E_IS_TABLE_FIELD_CHOOSER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TABLE_FIELD_CHOOSER_TYPE))


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
	ETableHeader *header;
};

struct _ETableFieldChooserClass
{
	GtkVBoxClass parent_class;
};


GtkWidget *e_table_field_chooser_new(void);
GType      e_table_field_chooser_get_type (void);

G_END_DECLS

#endif /* __E_TABLE_FIELD_CHOOSER_H__ */
