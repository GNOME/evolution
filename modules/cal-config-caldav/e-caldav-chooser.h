/*
 * e-caldav-chooser.h
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

#ifndef E_CALDAV_CHOOSER_H
#define E_CALDAV_CHOOSER_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <libedataserverui/libedataserverui.h>

/* Standard GObject macros */
#define E_TYPE_CALDAV_CHOOSER \
	(e_caldav_chooser_get_type ())
#define E_CALDAV_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CALDAV_CHOOSER, ECaldavChooser))
#define E_CALDAV_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CALDAV_CHOOSER, ECaldavChooserClass))
#define E_IS_CALDAV_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CALDAV_CHOOSER))
#define E_IS_CALDAV_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CALDAV_CHOOSER))
#define E_CALDAV_CHOOSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CALDAV_CHOOSER, ECaldavChooserClass))

G_BEGIN_DECLS

typedef struct _ECaldavChooser ECaldavChooser;
typedef struct _ECaldavChooserClass ECaldavChooserClass;
typedef struct _ECaldavChooserPrivate ECaldavChooserPrivate;

struct _ECaldavChooser {
	GtkTreeView parent;
	ECaldavChooserPrivate *priv;
};

struct _ECaldavChooserClass {
	GtkTreeViewClass parent_class;
};

GType		e_caldav_chooser_get_type	(void);
void		e_caldav_chooser_type_register	(GTypeModule *type_module);
GtkWidget *	e_caldav_chooser_new		(ESourceRegistry *registry,
						 ESource *source,
						 ECalClientSourceType source_type);
ESourceRegistry *
		e_caldav_chooser_get_registry	(ECaldavChooser *chooser);
ECredentialsPrompter *
		e_caldav_chooser_get_prompter	(ECaldavChooser *chooser);
ESource *	e_caldav_chooser_get_source	(ECaldavChooser *chooser);
ECalClientSourceType
		e_caldav_chooser_get_source_type
						(ECaldavChooser *chooser);
void		e_caldav_chooser_populate	(ECaldavChooser *chooser,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_caldav_chooser_populate_finish
						(ECaldavChooser *chooser,
						 GAsyncResult *result,
						 GError **error);
gboolean	e_caldav_chooser_apply_selected	(ECaldavChooser *chooser);

void		e_caldav_chooser_run_trust_prompt
						(ECaldavChooser *chooser,
						 GtkWindow *parent,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
GError *	e_caldav_chooser_new_ssl_trust_error
						(ECaldavChooser *chooser);
void		e_caldav_chooser_run_credentials_prompt
						(ECaldavChooser *chooser,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_caldav_chooser_run_credentials_prompt_finish
						(ECaldavChooser *chooser,
						 GAsyncResult *result,
						 ENamedParameters **out_credentials,
						 GError **error);
void		e_caldav_chooser_authenticate	(ECaldavChooser *chooser,
						 const ENamedParameters *credentials,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_caldav_chooser_authenticate_finish
						(ECaldavChooser *chooser,
						 GAsyncResult *result,
						 GError **error);

#endif /* E_CALDAV_CHOOSER_H */
