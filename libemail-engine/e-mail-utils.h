/*
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
 *
 * Authors:
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 *
 */

#ifndef E_MAIL_UTILS_H
#define E_MAIL_UTILS_H

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

typedef void (* EMailUtilsSourtSourcesFunc) (GList **psources, gpointer user_data);

gboolean	em_utils_folder_is_drafts	(ESourceRegistry *registry,
						 CamelFolder *folder);
gboolean	em_utils_folder_is_templates	(ESourceRegistry *registry,
						 CamelFolder *folder);
gboolean	em_utils_folder_is_sent		(ESourceRegistry *registry,
						 CamelFolder *folder);
gboolean	em_utils_folder_is_outbox	(ESourceRegistry *registry,
						 CamelFolder *folder);
gboolean	em_utils_in_addressbook		(ESourceRegistry *registry,
						 CamelInternetAddress *addr,
						 gboolean local_only,
						 GCancellable *cancellable);
CamelMimePart *	em_utils_contact_photo		(ESourceRegistry *registry,
						 CamelInternetAddress *addr,
						 gboolean local,
						 GCancellable *cancellable);
ESource *	em_utils_guess_mail_account	(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid);
ESource *	em_utils_guess_mail_identity	(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid);
ESource *	em_utils_guess_mail_account_with_recipients
						(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid);
ESource *	em_utils_guess_mail_identity_with_recipients
						(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid);
ESource *	em_utils_guess_mail_account_with_recipients_and_sort
						(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 EMailUtilsSourtSourcesFunc sort_func,
						 gpointer sort_func_data);
ESource *	em_utils_guess_mail_identity_with_recipients_and_sort
						(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 EMailUtilsSourtSourcesFunc sort_func,
						 gpointer sort_func_data);
ESource *	em_utils_ref_mail_identity_for_store
						(ESourceRegistry *registry,
						 CamelStore *store);
gboolean	em_utils_is_source_enabled_with_parents
						(ESourceRegistry *registry,
						 ESource *source);
void		emu_remove_from_mail_cache	(const GSList *addresses);
void		emu_remove_from_mail_cache_1	(const gchar *address);
void		emu_free_mail_cache		(GDestroyNotify done_cb,
						 gpointer user_data);
void		em_utils_uids_free		(GPtrArray *uids);
gboolean	em_utils_is_local_delivery_mbox_file
						(CamelURL *url);

#endif /* E_MAIL_UTILS_H */
