/*
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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 *
 */

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef E_MAIL_UTILS_H
#define E_MAIL_UTILS_H

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

typedef void	(*EMailUtilsSortSourcesFunc)	(GList **psources,
						 gpointer user_data);

gboolean	em_utils_folder_is_drafts	(ESourceRegistry *registry,
						 CamelFolder *folder);
gboolean	em_utils_folder_name_is_drafts	(ESourceRegistry *registry,
						 CamelStore *store,
						 const gchar *folder_name);
gboolean	em_utils_folder_is_templates	(ESourceRegistry *registry,
						 CamelFolder *folder);
gboolean	em_utils_folder_is_sent		(ESourceRegistry *registry,
						 CamelFolder *folder);
gboolean	em_utils_folder_is_outbox	(ESourceRegistry *registry,
						 CamelFolder *folder);
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
						 const gchar *message_uid,
						 gchar **identity_name,
						 gchar **identity_address);
ESource *	em_utils_guess_mail_account_with_recipients_and_sort
						(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 EMailUtilsSortSourcesFunc sort_func,
						 gpointer sort_func_data);
ESource *	em_utils_guess_mail_identity_with_recipients_and_sort
						(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 gchar **identity_name,
						 gchar **identity_address,
						 EMailUtilsSortSourcesFunc sort_func,
						 gpointer sort_func_data);
ESource *	em_utils_ref_mail_identity_for_store
						(ESourceRegistry *registry,
						 CamelStore *store);
gboolean	em_utils_is_local_delivery_mbox_file
						(CamelService *service);

void		em_utils_expand_groups		(CamelInternetAddress *addresses);
void		em_utils_get_real_folder_and_message_uid
						(CamelFolder *folder,
						 const gchar *uid,
						 CamelFolder **out_real_folder,
						 gchar **folder_uri,
						 gchar **message_uid);
gboolean	em_utils_address_is_user	(ESourceRegistry *registry,
						 const gchar *address,
						 gboolean only_enabled_accounts);
gboolean	em_utils_sender_is_user		(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 gboolean only_enabled_accounts);
CamelHeaderParam *
		em_utils_decode_autocrypt_header_value
						(const gchar *value);
gboolean	em_utils_decode_autocrypt_header(CamelMimeMessage *message,
						 guint index,
						 gboolean *out_prefer_encrypt,
						 guint8 **out_keydata,
						 gsize *out_keydata_size);

#endif /* E_MAIL_UTILS_H */
