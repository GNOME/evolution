/*
 * e-source-notebook.h
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
 */

#ifndef E_SOURCE_NOTEBOOK_H
#define E_SOURCE_NOTEBOOK_H

#include <gtk/gtk.h>
#include <libedataserver/e-source.h>

/* Standard GObject macros */
#define E_TYPE_SOURCE_NOTEBOOK \
	(e_source_notebook_get_type ())
#define E_SOURCE_NOTEBOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_NOTEBOOK, ESourceNotebook))
#define E_SOURCE_NOTEBOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_NOTEBOOK, ESourceNotebookClass))
#define E_IS_SOURCE_NOTEBOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SOURCE_NOTEBOOK))
#define E_IS_SOURCE_NOTEBOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SOURCE_NOTEBOOK))
#define E_SOURCE_NOTEBOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_NOTEBOOK))

G_BEGIN_DECLS

typedef struct _ESourceNotebook ESourceNotebook;
typedef struct _ESourceNotebookClass ESourceNotebookClass;
typedef struct _ESourceNotebookPrivate ESourceNotebookPrivate;

struct _ESourceNotebook {
	GtkNotebook parent;
	ESourceNotebookPrivate *priv;
};

struct _ESourceNotebookClass {
	GtkNotebookClass parent_class;
};

GType		e_source_notebook_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_source_notebook_new		(void);
gint		e_source_notebook_add_page	(ESourceNotebook *notebook,
						 ESource *source,
						 GtkWidget *child);
ESource *	e_source_notebook_get_active_source
						(ESourceNotebook *notebook);
void		e_source_notebook_set_active_source
						(ESourceNotebook *notebook,
						 ESource *source);

G_END_DECLS

#endif /* E_SOURCE_NOTEBOOK_H */

