/*
 * e-mail-shell-view-private.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_SHELL_VIEW_PRIVATE_H
#define E_MAIL_SHELL_VIEW_PRIVATE_H

#include "e-mail-shell-view.h"

#include <glib/gi18n.h>
#include <camel/camel-search-private.h>  /* for camel_search_word */

#include <mail/e-mail-folder-create-dialog.h>
#include <mail/e-mail-reader.h>
#include <mail/e-mail-reader-utils.h>
#include <mail/e-mail-sidebar.h>
#include <mail/e-mail-ui-session.h>
#include <mail/em-composer-utils.h>
#include <mail/em-folder-properties.h>
#include <mail/em-folder-selector.h>
#include <mail/em-folder-utils.h>
#include <mail/em-search-context.h>
#include <mail/em-subscription-editor.h>
#include <mail/em-utils.h>
#include <mail/mail-autofilter.h>
#include <mail/mail-send-recv.h>
#include <mail/mail-vfolder-ui.h>
#include <mail/message-list.h>

#include "e-mail-shell-backend.h"
#include "e-mail-shell-content.h"
#include "e-mail-shell-sidebar.h"
#include "e-mail-shell-view-actions.h"

/* Shorthand, requires a variable named "shell_view". */
#define ACTION(name) \
	(E_SHELL_VIEW_ACTION_##name (shell_view))

/* ETable Specifications */
#define ETSPEC_FILENAME		"message-list.etspec"

/* State File Keys */
#define STATE_KEY_SEARCH_FILTER		"SearchFilter"
#define STATE_KEY_SEARCH_SCOPE		"SearchScope"
#define STATE_KEY_SEARCH_TEXT		"SearchText"

G_BEGIN_DECLS

/* Filter items are displayed in ascending order.
 * Labels are numbered from zero, so subsequent items must have
 * sufficiently large/small values. */
enum {
	MAIL_FILTER_ALL_MESSAGES = -20,
	MAIL_FILTER_UNREAD_MESSAGES = -19,
	MAIL_FILTER_READ_MESSAGES = -18,
	MAIL_FILTER_MESSAGE_THREAD = -17,
	MAIL_FILTER_LAST_5_DAYS_MESSAGES = -16,
	MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS = -15,
	MAIL_FILTER_MESSAGES_WITH_NOTES = -14,
	MAIL_FILTER_IMPORTANT_MESSAGES = -13,
	MAIL_FILTER_MESSAGES_NOT_JUNK = -12,
	MAIL_FILTER_NO_LABEL = -11
	/* Labels go here */
};

/* Search items are displayed in ascending order. */
enum {
	MAIL_SEARCH_ADVANCED = -1,
	MAIL_SEARCH_SUBJECT_OR_ADDRESSES_CONTAIN,
	MAIL_SEARCH_RECIPIENTS_CONTAIN,
	MAIL_SEARCH_MESSAGE_CONTAINS,
	MAIL_SEARCH_SUBJECT_CONTAINS,
	MAIL_SEARCH_SENDER_CONTAINS,
	MAIL_SEARCH_BODY_CONTAINS,
	MAIL_SEARCH_FREE_FORM_EXPR,
	MAIL_NUM_SEARCH_RULES
};

/* Scope items are displayed in ascending order. */
enum {
	MAIL_SCOPE_CURRENT_FOLDER,
	MAIL_SCOPE_CURRENT_FOLDER_AND_SUBFOLDERS,
	MAIL_SCOPE_CURRENT_ACCOUNT,
	MAIL_SCOPE_ALL_ACCOUNTS
};

struct _EMailShellViewPrivate {

	/*** Other Stuff ***/

	/* These are just for convenience. */
	EMailShellBackend *mail_shell_backend;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;

	/* Filter rules correspond to the search entry menu. */
	EFilterRule *search_rules[MAIL_NUM_SEARCH_RULES];

	/* EShell::prepare-for-quit */
	gulong prepare_for_quit_handler_id;

	/* For opening the selected folder. */
	GCancellable *opening_folder;

	GMenu *send_receive_menu;

	/* Search folders for interactive search. */
	CamelVeeFolder *search_folder_and_subfolders;
	CamelVeeFolder *search_account_all;
	CamelVeeFolder *search_account_current;
	GCancellable *search_account_cancel;

	gboolean vfolder_allow_expunge;
	gboolean ignore_folder_popup_selection_done;
	gboolean web_view_accel_group_added;

	/* Selected UIDs for MAIL_FILTER_MESSAGE_THREAD filter */
	GSList *selected_uids;
};

void		e_mail_shell_view_private_init
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_private_constructed
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_private_dispose
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_private_finalize
					(EMailShellView *mail_shell_view);

/* Private Utilities */

void		e_mail_shell_view_actions_init
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_fill_send_receive_menu
					(EMailShellView *self);
void		e_mail_shell_view_restore_state
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_update_search_filter
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_update_sidebar
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_rename_folder
					(EMailShellView *mail_shell_view);

G_END_DECLS

#endif /* E_MAIL_SHELL_VIEW_PRIVATE_H */
