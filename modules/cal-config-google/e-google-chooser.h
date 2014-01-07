/*
 * e-google-chooser.h
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

#ifndef E_GOOGLE_CHOOSER_H
#define E_GOOGLE_CHOOSER_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_GOOGLE_CHOOSER \
	(e_google_chooser_get_type ())
#define E_GOOGLE_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_GOOGLE_CHOOSER, EGoogleChooser))
#define E_GOOGLE_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_GOOGLE_CHOOSER, EGoogleChooserClass))
#define E_IS_GOOGLE_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_GOOGLE_CHOOSER))
#define E_IS_GOOGLE_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_GOOGLE_CHOOSER))
#define E_GOOGLE_CHOOSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_GOOGLE_CHOOSER, EGoogleChooserClass))

G_BEGIN_DECLS

typedef struct _EGoogleChooser EGoogleChooser;
typedef struct _EGoogleChooserClass EGoogleChooserClass;
typedef struct _EGoogleChooserPrivate EGoogleChooserPrivate;

struct _EGoogleChooser {
	GtkTreeView parent;
	EGoogleChooserPrivate *priv;
};

struct _EGoogleChooserClass {
	GtkTreeViewClass parent_class;
};

GType		e_google_chooser_get_type	(void);
void		e_google_chooser_type_register
						(GTypeModule *type_module);
GtkWidget *	e_google_chooser_new		(ESource *source);
ESource *	e_google_chooser_get_source	(EGoogleChooser *chooser);
gchar *		e_google_chooser_get_decoded_user
						(EGoogleChooser *chooser);
void		e_google_chooser_populate	(EGoogleChooser *chooser,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_google_chooser_populate_finish
						(EGoogleChooser *chooser,
						 GAsyncResult *result,
						 GError **error);
gboolean	e_google_chooser_apply_selected	(EGoogleChooser *chooser);

void		e_google_chooser_construct_default_uri
						(SoupURI *soup_uri,
						 const gchar *username);

#endif /* E_GOOGLE_CHOOSER_H */
