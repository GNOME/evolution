/*
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
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_COMPOSER_UTILS_H
#define EM_COMPOSER_UTILS_H

#include <em-format/e-mail-part.h>
#include <em-format/e-mail-part-list.h>

#include <composer/e-msg-composer.h>

#include <mail/e-mail-backend.h>
#include <mail/e-mail-enums.h>

G_BEGIN_DECLS

void		em_utils_compose_new_message	(EMsgComposer *composer,
						 CamelFolder *folder);
void		em_utils_compose_new_message_with_selection
						(EMsgComposer *composer,
						 CamelFolder *folder,
						 const gchar *message_uid);
void		em_utils_compose_new_message_with_mailto
						(EShell *shell,
						 const gchar *mailto,
						 CamelFolder *folder);
void		em_utils_compose_new_message_with_mailto_and_selection
						(EShell *shell,
						 const gchar *mailto,
						 CamelFolder *folder,
						 const gchar *message_uid);
void		em_utils_edit_message		(EMsgComposer *composer,
						 CamelFolder *folder,
						 CamelMimeMessage *message,
						 const gchar *message_uid,
						 gboolean keep_signature,
						 gboolean replace_original_message);
void		em_utils_forward_message	(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 EMailForwardStyle style,
						 CamelFolder *folder,
						 const gchar *uid,
						 gboolean skip_insecure_parts);
void		em_utils_forward_attachment	(EMsgComposer *composer,
						 CamelMimePart *part,
						 const gchar *orig_subject,
						 CamelFolder *folder,
						 GPtrArray *uids);
void		em_utils_redirect_message	(EMsgComposer *composer,
						 CamelMimeMessage *message);
gboolean	em_utils_is_munged_list_message	(CamelMimeMessage *message);
void		em_utils_get_reply_sender	(CamelMimeMessage *message,
						 CamelInternetAddress *to,
						 CamelNNTPAddress *postto);
void		em_utils_get_reply_all		(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelInternetAddress *to,
						 CamelInternetAddress *cc,
						 CamelNNTPAddress *postto);
void		em_utils_reply_to_message	(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 EMailReplyType type,
						 EMailReplyStyle style,
						 EMailPartList *source,
						 CamelInternetAddress *address,
						 EMailReplyFlags reply_flags);
void		em_utils_reply_alternative	(GtkWindow *parent,
						 EShell *shell,
						 EAlertSink *alert_sink,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 EMailReplyStyle default_style,
						 EMailPartList *source,
						 EMailPartValidityFlags validity_pgp_sum,
						 EMailPartValidityFlags validity_smime_sum,
						 gboolean skip_insecure_parts);
void		em_utils_get_reply_recipients	(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 EMailReplyType reply_type,
						 CamelInternetAddress *address,
						 CamelInternetAddress *inout_to,
						 CamelInternetAddress *inout_cc,
						 CamelNNTPAddress *inout_postto);
EDestination **	em_utils_camel_address_to_destination
						(CamelInternetAddress *iaddr);
void		em_configure_new_composer	(EMsgComposer *composer,
						 EMailSession *session);
void		em_utils_get_real_folder_uri_and_message_uid
						(CamelFolder *folder,
						 const gchar *uid,
						 gchar **folder_uri,
						 gchar **message_uid);
ESource *	em_utils_check_send_account_override
						(EShell *shell,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 gchar **out_alias_name,
						 gchar **out_alias_address);
void		em_utils_apply_send_account_override_to_composer
						(EMsgComposer *composer,
						 CamelFolder *folder);
gchar *		em_composer_utils_get_forward_marker
						(EMsgComposer *composer);
gchar *		em_composer_utils_get_original_marker
						(EMsgComposer *composer);
gchar *		em_composer_utils_get_reply_credits
						(ESource *identity_source,
						 CamelMimeMessage *message);
void		em_utils_add_installed_languages(GtkComboBoxText *combo);
ESource *	em_composer_utils_guess_identity_source
						(EShell *shell,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 gchar **out_identity_name,
						 gchar **out_identity_address);
void		em_composer_utils_update_security
						(EMsgComposer *composer,
						 EMailPartValidityFlags validity_pgp_sum,
						 EMailPartValidityFlags validity_smime_sum);

G_END_DECLS

#endif /* EM_COMPOSER_UTILS_H */
