/*
 * e-proxy-editor.h
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_PROXY_EDITOR_H
#define E_PROXY_EDITOR_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_PROXY_EDITOR \
	(e_proxy_editor_get_type ())
#define E_PROXY_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PROXY_EDITOR, EProxyEditor))
#define E_PROXY_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PROXY_EDITOR, EProxyEditorClass))
#define E_IS_PROXY_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PROXY_EDITOR))
#define E_IS_PROXY_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PROXY_EDITOR))
#define E_PROXY_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PROXY_EDITOR, EProxyEditorClass))

G_BEGIN_DECLS

typedef struct _EProxyEditor EProxyEditor;
typedef struct _EProxyEditorClass EProxyEditorClass;
typedef struct _EProxyEditorPrivate EProxyEditorPrivate;

/**
 * EProxyEditor:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EProxyEditor {
	GtkGrid parent;
	EProxyEditorPrivate *priv;
};

struct _EProxyEditorClass {
	GtkGridClass parent_class;
};

GType		e_proxy_editor_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_proxy_editor_new		(ESourceRegistry *registry);
void		e_proxy_editor_save		(EProxyEditor *editor);
ESourceRegistry *
		e_proxy_editor_get_registry	(EProxyEditor *editor);
ESource *	e_proxy_editor_ref_source	(EProxyEditor *editor);
void		e_proxy_editor_set_source	(EProxyEditor *editor,
						 ESource *source);

G_END_DECLS

#endif /* E_PROXY_EDITOR_H */

