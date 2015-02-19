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
#include <libecal/libecal.h>
#include <libedataserverui/libedataserverui.h>

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
void		e_google_chooser_type_register	(GTypeModule *type_module);
GtkWidget *	e_google_chooser_new		(ESourceRegistry *registry,
						 ESource *source,
						 ECalClientSourceType source_type);
ESourceRegistry *
		e_google_chooser_get_registry	(EGoogleChooser *chooser);
ECredentialsPrompter *
		e_google_chooser_get_prompter	(EGoogleChooser *chooser);
ESource *	e_google_chooser_get_source	(EGoogleChooser *chooser);
ECalClientSourceType
		e_google_chooser_get_source_type
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

void		e_google_chooser_run_trust_prompt
						(EGoogleChooser *chooser,
						 GtkWindow *parent,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
GError *	e_google_chooser_new_ssl_trust_error
						(EGoogleChooser *chooser);
void		e_google_chooser_run_credentials_prompt
						(EGoogleChooser *chooser,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_google_chooser_run_credentials_prompt_finish
						(EGoogleChooser *chooser,
						 GAsyncResult *result,
						 ENamedParameters **out_credentials,
						 GError **error);
void		e_google_chooser_authenticate	(EGoogleChooser *chooser,
						 const ENamedParameters *credentials,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_google_chooser_authenticate_finish
						(EGoogleChooser *chooser,
						 GAsyncResult *result,
						 GError **error);

#endif /* E_GOOGLE_CHOOSER_H */
