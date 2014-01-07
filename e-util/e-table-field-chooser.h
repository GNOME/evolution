/*
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
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __E_TABLE_FIELD_CHOOSER_H__
#define __E_TABLE_FIELD_CHOOSER_H__

#include <gtk/gtk.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-table-header.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_FIELD_CHOOSER \
	(e_table_field_chooser_get_type ())
#define E_TABLE_FIELD_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_FIELD_CHOOSER, ETableFieldChooser))
#define E_TABLE_FIELD_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_FIELD_CHOOSER, ETableFieldChooserClass))
#define E_IS_TABLE_FIELD_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_FIELD_CHOOSER))
#define E_IS_TABLE_FIELD_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_FIELD_CHOOSER))
#define E_TABLE_FIELD_CHOOSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_FIELD_CHOOSER, ETableFieldChooserClass))

G_BEGIN_DECLS

typedef struct _ETableFieldChooser ETableFieldChooser;
typedef struct _ETableFieldChooserClass ETableFieldChooserClass;

struct _ETableFieldChooser {
	GtkBox parent;

	/* item specific fields */
	GnomeCanvas *canvas;
	GnomeCanvasItem *item;

	GnomeCanvasItem *rect;
	GtkAllocation last_alloc;

	gchar *dnd_code;
	ETableHeader *full_header;
	ETableHeader *header;
};

struct _ETableFieldChooserClass {
	GtkBoxClass parent_class;
};

GType		e_table_field_chooser_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_table_field_chooser_new	(void);

G_END_DECLS

#endif /* __E_TABLE_FIELD_CHOOSER_H__ */
