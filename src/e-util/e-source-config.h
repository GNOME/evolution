/*
 * e-source-config.h
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

#ifndef E_SOURCE_CONFIG_H
#define E_SOURCE_CONFIG_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_SOURCE_CONFIG \
	(e_source_config_get_type ())
#define E_SOURCE_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_CONFIG, ESourceConfig))
#define E_SOURCE_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_CONFIG, ESourceConfigClass))
#define E_IS_SOURCE_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SOURCE_CONFIG))
#define E_IS_SOURCE_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SOURCE_CONFIG))
#define E_SOURCE_CONFIG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_CONFIG, ESourceConfigClass))

G_BEGIN_DECLS

typedef struct _ESourceConfig ESourceConfig;
typedef struct _ESourceConfigClass ESourceConfigClass;
typedef struct _ESourceConfigPrivate ESourceConfigPrivate;

struct _ESourceConfig {
	GtkBox parent;
	ESourceConfigPrivate *priv;
};

struct _ESourceConfigClass {
	GtkBoxClass parent_class;

	/* Methods */
	const gchar *	(*get_backend_extension_name)
						(ESourceConfig *config);
	GList *		(*list_eligible_collections)
						(ESourceConfig *config);

	/* Signals */
	void		(*init_candidate)	(ESourceConfig *config,
						 ESource *scratch_source);
	gboolean	(*check_complete)	(ESourceConfig *config,
						 ESource *scratch_source);
	void		(*commit_changes)	(ESourceConfig *config,
						 ESource *scratch_source);
	void		(*resize_window)	(ESourceConfig *config);
};

GType		e_source_config_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_source_config_new		(ESourceRegistry *registry,
						 ESource *original_source);
void		e_source_config_set_preselect_type
						(ESourceConfig *config,
						 const gchar *source_uid);
const gchar *	e_source_config_get_preselect_type
						(ESourceConfig *config);
void		e_source_config_insert_widget	(ESourceConfig *config,
						 ESource *scratch_source,
						 const gchar *caption,
						 GtkWidget *widget);
GtkWidget *	e_source_config_get_page	(ESourceConfig *config,
						 ESource *scratch_source);
void		e_source_config_select_page	(ESourceConfig *config,
						 ESource *scratch_source);
const gchar *	e_source_config_get_backend_extension_name
						(ESourceConfig *config);
GList *		e_source_config_list_eligible_collections
						(ESourceConfig *config);
gboolean	e_source_config_check_complete	(ESourceConfig *config);
ESource *	e_source_config_get_original_source
						(ESourceConfig *config);
ESource *	e_source_config_get_collection_source
						(ESourceConfig *config);
GSList *	e_source_config_list_candidates	(ESourceConfig *config); /* ESource * */
ESourceRegistry *
		e_source_config_get_registry	(ESourceConfig *config);
void		e_source_config_resize_window	(ESourceConfig *config);
void		e_source_config_commit		(ESourceConfig *config,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_source_config_commit_finish	(ESourceConfig *config,
						 GAsyncResult *result,
						 GError **error);

/* Convenience functions for common settings. */
void		e_source_config_add_refresh_interval
						(ESourceConfig *config,
						 ESource *scratch_source);
void		e_source_config_add_refresh_on_metered_network
						(ESourceConfig *config,
						 ESource *scratch_source);
void		e_source_config_add_secure_connection
						(ESourceConfig *config,
						 ESource *scratch_source);
void		e_source_config_add_secure_connection_for_webdav
						(ESourceConfig *config,
						 ESource *scratch_source);
GtkWidget *	e_source_config_add_user_entry	(ESourceConfig *config,
						 ESource *scratch_source);
void		e_source_config_add_timeout_interval_for_webdav
						(ESourceConfig *config,
						 ESource *scratch_source);

#endif /* E_SOURCE_CONFIG_H */
