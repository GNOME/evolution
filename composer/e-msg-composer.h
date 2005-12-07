/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer.h
 *
 * Copyright (C) 1999, 2000  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */


#ifndef ___E_MSG_COMPOSER_H__
#define ___E_MSG_COMPOSER_H__

typedef struct _EMsgComposer       EMsgComposer;
typedef struct _EMsgComposerClass  EMsgComposerClass;

#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-ui-component.h>

#include "e-msg-composer-hdrs.h"
#include "Editor.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define E_TYPE_MSG_COMPOSER	       (e_msg_composer_get_type ())
#define E_MSG_COMPOSER(obj)	       (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MSG_COMPOSER, EMsgComposer))
#define E_MSG_COMPOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MSG_COMPOSER, EMsgComposerClass))
#define E_IS_MSG_COMPOSER(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MSG_COMPOSER))
#define E_IS_MSG_COMPOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MSG_COMPOSER))





GtkType                  e_msg_composer_get_type                         (void);

EMsgComposer            *e_msg_composer_new                              (void);

#define E_MSG_COMPOSER_MAIL 1
#define E_MSG_COMPOSER_POST 2
#define E_MSG_COMPOSER_MAIL_POST E_MSG_COMPOSER_MAIL|E_MSG_COMPOSER_POST

EMsgComposer            *e_msg_composer_new_with_type                    (int type);

EMsgComposer            *e_msg_composer_new_with_message                 (CamelMimeMessage  *msg);
EMsgComposer            *e_msg_composer_new_from_url                     (const char        *url);
EMsgComposer            *e_msg_composer_new_redirect                     (CamelMimeMessage  *message,
									  const char        *resent_from);
void			e_msg_composer_show_attachments_ui		 (EMsgComposer *composer);

/*
void                     e_msg_composer_show_attachments                 (EMsgComposer      *composer,
									  gboolean           show);*/

void                     e_msg_composer_set_alternative                  (EMsgComposer      *composer,
									  gboolean           alt);
									  
void                     e_msg_composer_set_headers                      (EMsgComposer      *composer,
									  const char        *from,
									  EDestination     **to,
									  EDestination     **cc,
									  EDestination     **bcc,
									  const char        *subject);
void                     e_msg_composer_set_body_text                    (EMsgComposer      *composer,
									  const char        *text,
									  ssize_t            len);
void                     e_msg_composer_set_body                         (EMsgComposer      *composer,
									  const char        *body,
									  const char        *mime_type);
void                     e_msg_composer_add_header                       (EMsgComposer      *composer,
									  const char        *name,
									  const char        *value);
void                     e_msg_composer_attach                           (EMsgComposer      *composer,
									  CamelMimePart     *attachment);
CamelMimePart           *e_msg_composer_add_inline_image_from_file       (EMsgComposer      *composer,
									  const char        *filename);
void                     e_msg_composer_add_inline_image_from_mime_part  (EMsgComposer      *composer,
									  CamelMimePart     *part);
CamelMimeMessage        *e_msg_composer_get_message                      (EMsgComposer      *composer,
									  gboolean           save_html_object_data);
CamelMimeMessage        *e_msg_composer_get_message_draft                (EMsgComposer      *composer);
void                     e_msg_composer_show_sig_file                    (EMsgComposer      *composer);
gboolean                 e_msg_composer_get_send_html                    (EMsgComposer      *composer);
void                     e_msg_composer_set_send_html                    (EMsgComposer      *composer,
									  gboolean           send_html);

gboolean                 e_msg_composer_get_view_from                    (EMsgComposer      *composer);
void                     e_msg_composer_set_view_from                    (EMsgComposer      *composer,
									  gboolean           view_from);
gboolean                 e_msg_composer_get_view_to                      (EMsgComposer      *composer);
void                     e_msg_composer_set_view_to                      (EMsgComposer      *composer,
									  gboolean           view_replyto);
gboolean                 e_msg_composer_get_view_replyto                 (EMsgComposer      *composer);
void                     e_msg_composer_set_view_replyto                 (EMsgComposer      *composer,
									  gboolean           view_replyto);
gboolean                 e_msg_composer_get_view_postto                  (EMsgComposer      *composer);
void                     e_msg_composer_set_view_postto                  (EMsgComposer      *composer,
									  gboolean           view_replyto);
gboolean                 e_msg_composer_get_view_cc                      (EMsgComposer      *composer);
void                     e_msg_composer_set_view_cc                      (EMsgComposer      *composer,
									  gboolean           view_cc);
gboolean                 e_msg_composer_get_view_bcc                     (EMsgComposer      *composer);
void                     e_msg_composer_set_view_bcc                     (EMsgComposer      *composer,
									  gboolean           view_bcc);

gboolean                 e_msg_composer_get_request_receipt              (EMsgComposer *composer);
void                     e_msg_composer_set_request_receipt              (EMsgComposer *composer,
									  gboolean      request_receipt);

gboolean                 e_msg_composer_get_priority              (EMsgComposer *composer);
void                     e_msg_composer_set_priority              (EMsgComposer *composer,
								  gboolean      set_priority);

EDestination           **e_msg_composer_get_recipients                   (EMsgComposer *composer);
EDestination           **e_msg_composer_get_to                           (EMsgComposer *composer);
EDestination           **e_msg_composer_get_cc                           (EMsgComposer *composer);
EDestination           **e_msg_composer_get_bcc                          (EMsgComposer *composer);
const char              *e_msg_composer_get_subject                      (EMsgComposer *composer);

EAccount                *e_msg_composer_get_preferred_account            (EMsgComposer      *composer);
void                     e_msg_composer_clear_inlined_table              (EMsgComposer      *composer);
char                    *e_msg_composer_guess_mime_type                  (const char        *file_name);
void                     e_msg_composer_set_changed                      (EMsgComposer      *composer);
void                     e_msg_composer_unset_changed                    (EMsgComposer      *composer);
gboolean                 e_msg_composer_is_dirty                         (EMsgComposer      *composer);
void                     e_msg_composer_set_autosaved                    (EMsgComposer      *composer);
void                     e_msg_composer_unset_autosaved                  (EMsgComposer      *composer);
gboolean                 e_msg_composer_is_autosaved                     (EMsgComposer      *composer);
void                     e_msg_composer_set_enable_autosave              (EMsgComposer      *composer,
									  gboolean           enabled);

/* PGP */
void                     e_msg_composer_set_pgp_sign                     (EMsgComposer      *composer,
									  gboolean           pgp_sign);
gboolean                 e_msg_composer_get_pgp_sign                     (EMsgComposer      *composer);
void                     e_msg_composer_set_pgp_encrypt                  (EMsgComposer      *composer,
									  gboolean           pgp_encrypt);
gboolean                 e_msg_composer_get_pgp_encrypt                  (EMsgComposer      *composer);

/* S/MIME */
void                     e_msg_composer_set_smime_sign                   (EMsgComposer      *composer,
									  gboolean           smime_sign);
gboolean                 e_msg_composer_get_smime_sign                   (EMsgComposer      *composer);
void                     e_msg_composer_set_smime_encrypt                (EMsgComposer      *composer,
									  gboolean           smime_encrypt);
gboolean                 e_msg_composer_get_smime_encrypt                (EMsgComposer      *composer);
char                    *e_msg_composer_get_sig_file_content             (const char        *sigfile,
									  gboolean           in_html);
void                     e_msg_composer_add_message_attachments          (EMsgComposer      *composer,
									  CamelMimeMessage  *message,
									  gboolean           just_inlines);
void                     e_msg_composer_ignore                           (EMsgComposer      *composer,
									  const char        *str);
void                     e_msg_composer_drop_editor_undo                 (EMsgComposer      *composer);

gboolean                 e_msg_composer_request_close_all                (void);
void			 e_msg_composer_check_autosave			 (GtkWindow *parent);
int			 e_msg_composer_get_remote_download_count   	 (EMsgComposer *composer);


void			 e_msg_composer_reply_indent			 (EMsgComposer *composer);
void			 e_msg_composer_insert_paragraph_before 	 (EMsgComposer *composer);
void			 e_msg_composer_insert_paragraph_after		 (EMsgComposer *composer);
void			 e_msg_composer_delete				 (EMsgComposer *composer);
gchar*			 e_msg_composer_resolve_image_url 		 (EMsgComposer *composer, gchar *url);
CamelMimePart*		 e_msg_composer_url_requested 			 (EMsgComposer *composer, gchar *url);

EMsgComposerHdrs*	 e_msg_composer_get_hdrs			 (EMsgComposer *composer);
void			 e_msg_composer_set_saved			 (EMsgComposer *composer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ___E_MSG_COMPOSER_H__ */
