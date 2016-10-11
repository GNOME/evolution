/*
 * e-mail-store-utils.h
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

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef E_MAIL_STORE_UTILS_H
#define E_MAIL_STORE_UTILS_H

/* CamelStore wrappers with Evolution-specific policies. */

#include <camel/camel.h>

G_BEGIN_DECLS

gboolean	e_mail_store_create_folder_sync	(CamelStore *store,
						 const gchar *full_name,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_store_create_folder	(CamelStore *store,
						 const gchar *full_name,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_store_create_folder_finish
						(CamelStore *store,
						 GAsyncResult *result,
						 GError **error);

gboolean	e_mail_store_go_offline_sync	(CamelStore *store,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_store_go_offline		(CamelStore *store,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_store_go_offline_finish	(CamelStore *store,
						 GAsyncResult *result,
						 GError **error);

gboolean	e_mail_store_go_online_sync	(CamelStore *store,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_store_go_online		(CamelStore *store,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_store_go_online_finish	(CamelStore *store,
						 GAsyncResult *result,
						 GError **error);

void		e_mail_store_prepare_for_offline
						(CamelStore *store,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_store_prepare_for_offline_finish
						(CamelStore *store,
						 GAsyncResult *result,
						 GError **error);

gboolean	e_mail_store_save_initial_setup_sync
						(CamelStore *store,
						 GHashTable *save_setup,
						 ESource *collection_source,
						 ESource *account_source,
						 ESource *submission_source,
						 ESource *transport_source,
						 gboolean write_sources,
						 GCancellable *cancellable,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_STORE_UTILS_H */
