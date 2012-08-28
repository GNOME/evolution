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

#define E_COMPOSER_ACTION(editor, name) \
	(e_editor_get_action (E_EDITOR (editor), (name)))

#define E_COMPOSER_ACTION_ATTACH(editor) \
	E_COMPOSER_ACTION ((editor), "attach")
#define E_COMPOSER_ACTION_CLOSE(editor) \
	E_COMPOSER_ACTION ((editor), "close")
#define E_COMPOSER_ACTION_PGP_ENCRYPT(editor) \
	E_COMPOSER_ACTION ((editor), "pgp-encrypt")
#define E_COMPOSER_ACTION_PGP_SIGN(editor) \
	E_COMPOSER_ACTION ((editor), "pgp-sign")
#define E_COMPOSER_ACTION_PICTURE_GALLERY(editor) \
	E_COMPOSER_ACTION ((editor), "picture-gallery")
#define E_COMPOSER_ACTION_PRINT(editor) \
	E_COMPOSER_ACTION ((editor), "print")
#define E_COMPOSER_ACTION_PRINT_PREVIEW(editor) \
	E_COMPOSER_ACTION ((editor), "print-preview")
#define E_COMPOSER_ACTION_PRIORITIZE_MESSAGE(editor) \
	E_COMPOSER_ACTION ((editor), "prioritize-message")
#define E_COMPOSER_ACTION_REQUEST_READ_RECEIPT(editor) \
	E_COMPOSER_ACTION ((editor), "request-read-receipt")
#define E_COMPOSER_ACTION_SAVE(editor) \
	E_COMPOSER_ACTION ((editor), "save")
#define E_COMPOSER_ACTION_SAVE_AS(editor) \
	E_COMPOSER_ACTION ((editor), "save-as")
#define E_COMPOSER_ACTION_SAVE_DRAFT(editor) \
	E_COMPOSER_ACTION ((editor), "save-draft")
#define E_COMPOSER_ACTION_SECURITY_MENU(editor) \
	E_COMPOSER_ACTION ((editor), "security-menu")
#define E_COMPOSER_ACTION_SEND(editor) \
	E_COMPOSER_ACTION ((editor), "send")
#define E_COMPOSER_ACTION_NEW_MESSAGE(editor) \
	E_COMPOSER_ACTION ((editor), "new-message")
#define E_COMPOSER_ACTION_SMIME_ENCRYPT(editor) \
	E_COMPOSER_ACTION ((editor), "smime-encrypt")
#define E_COMPOSER_ACTION_SMIME_SIGN(editor) \
	E_COMPOSER_ACTION ((editor), "smime-sign")
#define E_COMPOSER_ACTION_VIEW_BCC(editor) \
	E_COMPOSER_ACTION ((editor), "view-bcc")
#define E_COMPOSER_ACTION_VIEW_CC(editor) \
	E_COMPOSER_ACTION ((editor), "view-cc")
#define E_COMPOSER_ACTION_VIEW_REPLY_TO(editor) \
	E_COMPOSER_ACTION ((editor), "view-reply-to")

#endif /* E_COMPOSER_ACTIONS_H */
