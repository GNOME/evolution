/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-book-shell-view-actions.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_BOOK_SHELL_VIEW_ACTIONS_H
#define E_BOOK_SHELL_VIEW_ACTIONS_H

#include <shell/e-shell-window-actions.h>

/* Address Book Actions */
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-copy")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-delete")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_MOVE(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-move")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_PROPERTIES(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-properties")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_RENAME(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-rename")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_SAVE_AS(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-save-as")
#define E_SHELL_WINDOW_ACTION_ADDRESS_BOOK_STOP(window) \
	E_SHELL_WINDOW_ACTION ((window), "address-book-stop")

/* Contact Actions */
#define E_SHELL_WINDOW_ACTION_CONTACT_CLIPBOARD_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-clipboard-copy")
#define E_SHELL_WINDOW_ACTION_CONTACT_CLIPBOARD_CUT(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-clipbard-cut")
#define E_SHELL_WINDOW_ACTION_CONTACT_CLIPBOARD_PASTE(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-clipboard-paste")
#define E_SHELL_WINDOW_ACTION_CONTACT_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-copy")
#define E_SHELL_WINDOW_ACTION_CONTACT_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-delete")
#define E_SHELL_WINDOW_ACTION_CONTACT_FORWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-forward")
#define E_SHELL_WINDOW_ACTION_CONTACT_MOVE(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-move")
#define E_SHELL_WINDOW_ACTION_CONTACT_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-new")
#define E_SHELL_WINDOW_ACTION_CONTACT_NEW_LIST(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-new-list")
#define E_SHELL_WINDOW_ACTION_CONTACT_OPEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-open")
#define E_SHELL_WINDOW_ACTION_CONTACT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-preview")
#define E_SHELL_WINDOW_ACTION_CONTACT_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-print")
#define E_SHELL_WINDOW_ACTION_CONTACT_PRINT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-print-preview")
#define E_SHELL_WINDOW_ACTION_CONTACT_SAVE_AS(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-save-as")
#define E_SHELL_WINDOW_ACTION_CONTACT_SELECT_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-select-all")
#define E_SHELL_WINDOW_ACTION_CONTACT_SEND_MESSAGE(window) \
	E_SHELL_WINDOW_ACTION ((window), "contact-send-message")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_CONTACTS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "contacts")

#endif /* E_BOOK_SHELL_VIEW_ACTIONS_H */
