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

#include <bonobo/bonobo-win.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo-conf/bonobo-config-database.h>

#include "e-msg-composer-attachment-bar.h"
#include "e-msg-composer-hdrs.h"
#include "Editor.h"

#include <mail/mail-config.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define E_TYPE_MSG_COMPOSER	       (e_msg_composer_get_type ())
#define E_MSG_COMPOSER(obj)	       (GTK_CHECK_CAST ((obj), E_TYPE_MSG_COMPOSER, EMsgComposer))
#define E_MSG_COMPOSER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_MSG_COMPOSER, EMsgComposerClass))
#define E_IS_MSG_COMPOSER(obj)	       (GTK_CHECK_TYPE ((obj), E_TYPE_MSG_COMPOSER))
#define E_IS_MSG_COMPOSER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_MSG_COMPOSER))



struct _EMsgComposer {
	BonoboWindow parent;
	
	BonoboUIComponent *uic;
	
	GtkWidget *hdrs;
	GPtrArray *extra_hdr_names, *extra_hdr_values;
	
	GtkWidget *editor;
	
	GtkWidget *attachment_bar;
	GtkWidget *attachment_scroll_frame;
	
	GtkWidget *address_dialog;
	
	Bonobo_PersistFile       persist_file_interface;
	Bonobo_PersistStream     persist_stream_interface;
	GNOME_GtkHTML_Editor_Engine  editor_engine;
	BonoboObject            *editor_listener;
	GHashTable              *inline_images, *inline_images_by_url;
	GList                   *current_images;
	
	Bonobo_ConfigDatabase    config_db;
	
	char *mime_type, *mime_body, *charset;

	char *autosave_file;
        int   autosave_fd;

	gboolean attachment_bar_visible : 1;
	gboolean send_html     : 1;
	gboolean is_alternative: 1;
	gboolean pgp_sign      : 1;
	gboolean pgp_encrypt   : 1;
	gboolean smime_sign    : 1;
	gboolean smime_encrypt : 1;
	gboolean view_from     : 1;
	gboolean view_replyto  : 1;
	gboolean view_bcc      : 1;
	gboolean view_cc       : 1;
	gboolean view_subject  : 1;
	gboolean has_changed   : 1;

	gboolean in_signature_insert : 1;

	gboolean enable_autosave : 1;
	
	CamelMimeMessage *redirect;

	MailConfigSignature *signature;
	gboolean random_signature;
};

struct _EMsgComposerClass {
	BonoboWindowClass parent_class;
	
	void (* send) (EMsgComposer *composer);
	void (* postpone) (EMsgComposer *composer);
	void (* save_draft) (EMsgComposer *composer, int quit);
};


GtkType                  e_msg_composer_get_type                         (void);
EMsgComposer            *e_msg_composer_new                              (void);
EMsgComposer            *e_msg_composer_new_with_message                 (CamelMimeMessage  *msg);
EMsgComposer            *e_msg_composer_new_from_url                     (const char        *url);
EMsgComposer            *e_msg_composer_new_redirect                     (CamelMimeMessage  *message,
									  const char        *resent_from);
void                     e_msg_composer_show_attachments                 (EMsgComposer      *composer,
									  gboolean           show);
void                     e_msg_composer_set_headers                      (EMsgComposer      *composer,
									  const char        *from,
									  EDestination     **to,
									  EDestination     **cc,
									  EDestination     **bcc,
									  const char        *subject);
void                     e_msg_composer_set_body_text                    (EMsgComposer      *composer,
									  const char        *text);
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
CamelMimeMessage        *e_msg_composer_get_message                      (EMsgComposer      *composer);
CamelMimeMessage        *e_msg_composer_get_message_draft                (EMsgComposer      *composer);
void                     e_msg_composer_show_sig_file                    (EMsgComposer      *composer);
gboolean                 e_msg_composer_get_send_html                    (EMsgComposer      *composer);
void                     e_msg_composer_set_send_html                    (EMsgComposer      *composer,
									  gboolean           send_html);
gboolean                 e_msg_composer_get_view_from                    (EMsgComposer      *composer);
void                     e_msg_composer_set_view_from                    (EMsgComposer      *composer,
									  gboolean           view_from);
gboolean                 e_msg_composer_get_view_replyto                 (EMsgComposer      *composer);
void                     e_msg_composer_set_view_replyto                 (EMsgComposer      *composer,
									  gboolean           view_replyto);
gboolean                 e_msg_composer_get_view_bcc                     (EMsgComposer      *composer);
void                     e_msg_composer_set_view_bcc                     (EMsgComposer      *composer,
									  gboolean           view_bcc);
gboolean                 e_msg_composer_get_view_cc                      (EMsgComposer      *composer);
void                     e_msg_composer_set_view_cc                      (EMsgComposer      *composer,
									  gboolean           view_cc);

EDestination    **e_msg_composer_get_recipients       (EMsgComposer     *composer);

const MailConfigAccount *e_msg_composer_get_preferred_account            (EMsgComposer      *composer);
void                     e_msg_composer_clear_inlined_table              (EMsgComposer      *composer);
gchar                   *e_msg_composer_guess_mime_type                  (const gchar       *file_name);
void                     e_msg_composer_set_changed                      (EMsgComposer      *composer);
void                     e_msg_composer_unset_changed                    (EMsgComposer      *composer);
gboolean                 e_msg_composer_is_dirty                         (EMsgComposer      *composer);
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
gchar                   *e_msg_composer_get_sig_file_content             (const char        *sigfile,
									  gboolean           in_html);
void                     e_msg_composer_add_message_attachments          (EMsgComposer      *composer,
									  CamelMimeMessage  *message,
									  gboolean           just_inlines);
void                     e_msg_composer_ignore                           (EMsgComposer      *composer,
									  const gchar       *str);
void                     e_msg_composer_drop_editor_undo                 (EMsgComposer      *composer);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ___E_MSG_COMPOSER_H__ */
