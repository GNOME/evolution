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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef E_COMPOSER_ACTIONS_H
#define E_COMPOSER_ACTIONS_H

#define E_COMPOSER_ACTION(composer, name) \
	(e_html_editor_get_action ( \
		e_msg_composer_get_editor ( \
		E_MSG_COMPOSER (composer)), (name)))

#define E_COMPOSER_ACTION_ATTACH(composer) \
	E_COMPOSER_ACTION ((composer), "attach")
#define E_COMPOSER_ACTION_CLOSE(composer) \
	E_COMPOSER_ACTION ((composer), "close")
#define E_COMPOSER_ACTION_PGP_ENCRYPT(composer) \
	E_COMPOSER_ACTION ((composer), "pgp-encrypt")
#define E_COMPOSER_ACTION_PGP_SIGN(composer) \
	E_COMPOSER_ACTION ((composer), "pgp-sign")
#define E_COMPOSER_ACTION_PICTURE_GALLERY(composer) \
	E_COMPOSER_ACTION ((composer), "picture-gallery")
#define E_COMPOSER_ACTION_PRINT(composer) \
	E_COMPOSER_ACTION ((composer), "print")
#define E_COMPOSER_ACTION_PRINT_PREVIEW(composer) \
	E_COMPOSER_ACTION ((composer), "print-preview")
#define E_COMPOSER_ACTION_PRIORITIZE_MESSAGE(composer) \
	E_COMPOSER_ACTION ((composer), "prioritize-message")
#define E_COMPOSER_ACTION_REQUEST_READ_RECEIPT(composer) \
	E_COMPOSER_ACTION ((composer), "request-read-receipt")
#define E_COMPOSER_ACTION_DELIVERY_STATUS_NOTIFICATION(composer) \
	E_COMPOSER_ACTION ((composer), "delivery-status-notification")
#define E_COMPOSER_ACTION_SAVE(composer) \
	E_COMPOSER_ACTION ((composer), "save")
#define E_COMPOSER_ACTION_SAVE_AS(composer) \
	E_COMPOSER_ACTION ((composer), "save-as")
#define E_COMPOSER_ACTION_SAVE_DRAFT(composer) \
	E_COMPOSER_ACTION ((composer), "save-draft")
#define E_COMPOSER_ACTION_SECURITY_MENU(composer) \
	E_COMPOSER_ACTION ((composer), "security-menu")
#define E_COMPOSER_ACTION_SEND(composer) \
	E_COMPOSER_ACTION ((composer), "send")
#define E_COMPOSER_ACTION_NEW_MESSAGE(composer) \
	E_COMPOSER_ACTION ((composer), "new-message")
#define E_COMPOSER_ACTION_SMIME_ENCRYPT(composer) \
	E_COMPOSER_ACTION ((composer), "smime-encrypt")
#define E_COMPOSER_ACTION_SMIME_SIGN(composer) \
	E_COMPOSER_ACTION ((composer), "smime-sign")
#define E_COMPOSER_ACTION_TOOLBAR_SHOW_MAIN(composer) \
	E_COMPOSER_ACTION ((composer), "toolbar-show-main")
#define E_COMPOSER_ACTION_TOOLBAR_SHOW_EDIT(composer) \
	E_COMPOSER_ACTION ((composer), "toolbar-show-edit")
#define E_COMPOSER_ACTION_TOOLBAR_PGP_ENCRYPT(composer) \
	E_COMPOSER_ACTION ((composer), "toolbar-pgp-encrypt")
#define E_COMPOSER_ACTION_TOOLBAR_PGP_SIGN(composer) \
	E_COMPOSER_ACTION ((composer), "toolbar-pgp-sign")
#define E_COMPOSER_ACTION_TOOLBAR_PRIORITIZE_MESSAGE(composer) \
	E_COMPOSER_ACTION ((composer), "toolbar-prioritize-message")
#define E_COMPOSER_ACTION_TOOLBAR_REQUEST_READ_RECEIPT(composer) \
	E_COMPOSER_ACTION ((composer), "toolbar-request-read-receipt")
#define E_COMPOSER_ACTION_TOOLBAR_SMIME_ENCRYPT(composer) \
	E_COMPOSER_ACTION ((composer), "toolbar-smime-encrypt")
#define E_COMPOSER_ACTION_TOOLBAR_SMIME_SIGN(composer) \
	E_COMPOSER_ACTION ((composer), "toolbar-smime-sign")
#define E_COMPOSER_ACTION_VIEW_BCC(composer) \
	E_COMPOSER_ACTION ((composer), "view-bcc")
#define E_COMPOSER_ACTION_VIEW_CC(composer) \
	E_COMPOSER_ACTION ((composer), "view-cc")
#define E_COMPOSER_ACTION_VIEW_FROM_OVERRIDE(composer) \
	E_COMPOSER_ACTION ((composer), "view-from-override")
#define E_COMPOSER_ACTION_VIEW_MAIL_FOLLOWUP_TO(composer) \
	E_COMPOSER_ACTION ((composer), "view-mail-followup-to")
#define E_COMPOSER_ACTION_VIEW_MAIL_REPLY_TO(composer) \
	E_COMPOSER_ACTION ((composer), "view-mail-reply-to")
#define E_COMPOSER_ACTION_VIEW_REPLY_TO(composer) \
	E_COMPOSER_ACTION ((composer), "view-reply-to")
#define E_COMPOSER_ACTION_VISUALLY_WRAP_LONG_LINES(composer) \
	E_COMPOSER_ACTION ((composer), "visually-wrap-long-lines")

#endif /* E_COMPOSER_ACTIONS_H */
