/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Mike Kestner  <mkestner@ximian.com>
 */

#ifndef E_SELECT_NAMES_RENDERER_H
#define E_SELECT_NAMES_RENDERER_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_SELECT_NAMES_RENDERER \
	(e_select_names_renderer_get_type ())
#define E_SELECT_NAMES_RENDERER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SELECT_NAMES_RENDERER, ESelectNamesRenderer))
#define E_SELECT_NAMES_RENDERER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SELECT_NAMES_RENDERER, ESelectNamesRendererClass))
#define E_IS_SELECT_NAMES_RENDERER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SELECT_NAMES_RENDERER))
#define E_IS_SELECT_NAMES_RENDERER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SELECT_NAMES_RENDERER))
#define E_SELECT_NAMES_RENDERER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SELECT_NAMES_RENDERER, ESelectNamesRendererClass))

G_BEGIN_DECLS

typedef struct _ESelectNamesRenderer ESelectNamesRenderer;
typedef struct _ESelectNamesRendererClass ESelectNamesRendererClass;
typedef struct _ESelectNamesRendererPrivate ESelectNamesRendererPrivate;

struct _ESelectNamesRenderer {
	GtkCellRendererText parent;
	ESelectNamesRendererPrivate *priv;
};

struct _ESelectNamesRendererClass {
	GtkCellRendererTextClass parent_class;

	void		(*cell_edited)		(ESelectNamesRenderer *renderer,
						 const gchar *path,
						 GList *addresses,
						 GList *names);
};

GType		e_select_names_renderer_get_type
						(void) G_GNUC_CONST;
GtkCellRenderer *
		e_select_names_renderer_new	(EClientCache *client_cache);
EClientCache *	e_select_names_renderer_ref_client_cache
						(ESelectNamesRenderer *renderer);
EDestination *	e_select_names_renderer_get_destination
						(ESelectNamesRenderer *renderer);
const gchar *	e_select_names_renderer_get_name
						(ESelectNamesRenderer *renderer);
void		e_select_names_renderer_set_name
						(ESelectNamesRenderer *renderer,
						 const gchar *name);
const gchar *	e_select_names_renderer_get_email
						(ESelectNamesRenderer *renderer);
void		e_select_names_renderer_set_email
						(ESelectNamesRenderer *renderer,
						 const gchar *email);

G_END_DECLS

#endif /* E_SELECT_NAMES_RENDERER_H */
