/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_COMPOSER_ACTIONS_H
#define E_COMPOSER_ACTIONS_H

#define E_COMPOSER_ACTION(composer, name) \
	(gtkhtml_editor_get_action (GTKHTML_EDITOR (composer), (name)))

#define E_COMPOSER_ACTION_ATTACH(composer) \
	E_COMPOSER_ACTION ((composer), "attach")
#define E_COMPOSER_ACTION_CLOSE(composer) \
	E_COMPOSER_ACTION ((composer), "close")
#define E_COMPOSER_ACTION_PGP_ENCRYPT(composer) \
	E_COMPOSER_ACTION ((composer), "pgp-encrypt")
#define E_COMPOSER_ACTION_PGP_SIGN(composer) \
	E_COMPOSER_ACTION ((composer), "pgp-sign")
#define E_COMPOSER_ACTION_PRINT(composer) \
	E_COMPOSER_ACTION ((composer), "print")
#define E_COMPOSER_ACTION_PRINT_PREVIEW(composer) \
	E_COMPOSER_ACTION ((composer), "print-preview")
#define E_COMPOSER_ACTION_PRIORITIZE_MESSAGE(composer) \
	E_COMPOSER_ACTION ((composer), "prioritize-message")
#define E_COMPOSER_ACTION_REQUEST_READ_RECEIPT(composer) \
	E_COMPOSER_ACTION ((composer), "request-read-receipt")
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
#define E_COMPOSER_ACTION_SEND_OPTIONS(composer) \
	E_COMPOSER_ACTION ((composer), "send-options")
#define E_COMPOSER_ACTION_SMIME_ENCRYPT(composer) \
	E_COMPOSER_ACTION ((composer), "smime-encrypt")
#define E_COMPOSER_ACTION_SMIME_SIGN(composer) \
	E_COMPOSER_ACTION ((composer), "smime-sign")
#define E_COMPOSER_ACTION_VIEW_BCC(composer) \
	E_COMPOSER_ACTION ((composer), "view-bcc")
#define E_COMPOSER_ACTION_VIEW_CC(composer) \
	E_COMPOSER_ACTION ((composer), "view-cc")
#define E_COMPOSER_ACTION_VIEW_FROM(composer) \
	E_COMPOSER_ACTION ((composer), "view-from")
#define E_COMPOSER_ACTION_VIEW_POST_TO(composer) \
	E_COMPOSER_ACTION ((composer), "view-post-to")
#define E_COMPOSER_ACTION_VIEW_REPLY_TO(composer) \
	E_COMPOSER_ACTION ((composer), "view-reply-to")
#define E_COMPOSER_ACTION_VIEW_SUBJECT(composer) \
	E_COMPOSER_ACTION ((composer), "view-subject")
#define E_COMPOSER_ACTION_VIEW_TO(composer) \
	E_COMPOSER_ACTION ((composer), "view-to")

#endif /* E_COMPOSER_ACTIONS_H */
