/*
 * e-mail-shell-view-private.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_SHELL_VIEW_PRIVATE_H
#define E_MAIL_SHELL_VIEW_PRIVATE_H

#include "e-mail-shell-view.h"

#include <glib/gi18n.h>
#include <gtkhtml/gtkhtml.h>
#include <camel/camel-search-private.h>  /* for camel_search_word */

#include "e-util/e-util.h"
#include "e-util/e-binding.h"
#include "e-util/gconf-bridge.h"
#include "e-util/e-account-utils.h"
#include "e-util/e-ui-manager.h"
#include "filter/e-filter-part.h"
#include "widgets/misc/e-web-view.h"
#include "widgets/misc/e-popup-action.h"
#include "widgets/menus/gal-view-instance.h"

#include "e-mail-label-action.h"
#include "e-mail-label-dialog.h"
#include "e-mail-label-list-store.h"
#include "e-mail-local.h"
#include "e-mail-reader.h"
#include "e-mail-sidebar.h"
#include "e-mail-store.h"
#include "em-composer-utils.h"
#include "em-folder-properties.h"
#include "em-folder-selector.h"
#include "em-folder-utils.h"
#include "em-search-context.h"
#include "em-subscribe-editor.h"
#include "em-utils.h"
#include "mail-autofilter.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-send-recv.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-vfolder.h"
#include "message-list.h"

#include "e-mail-shell-backend.h"
#include "e-mail-shell-content.h"
#include "e-mail-shell-sidebar.h"
#include "e-mail-shell-view-actions.h"

#define E_MAIL_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_VIEW, EMailShellViewPrivate))

/* Shorthand, requires a variable named "shell_window". */
#define ACTION(name) \
	(E_SHELL_WINDOW_ACTION_##name (shell_window))
#define ACTION_GROUP(name) \
	(E_SHELL_WINDOW_ACTION_GROUP_##name (shell_window))

/* For use in dispose() methods. */
#define DISPOSE(obj) \
	G_STMT_START { \
	if ((obj) != NULL) { g_object_unref (obj); (obj) = NULL; } \
	} G_STMT_END

/* ETable Specifications */
#define ETSPEC_FILENAME		"message-list.etspec"

/* State File Keys */
#define STATE_KEY_SEARCH_FILTER		"SearchFilter"
#define STATE_KEY_SEARCH_SCOPE		"SearchScope"
#define STATE_KEY_SEARCH_TEXT		"SearchText"

G_BEGIN_DECLS

/* Filter items are displayed in ascending order.
 * Labels are numbered from zero, so subsequent items must have
 * sufficiently large values.  Unfortunately this introduces an
 * arbitrary upper bound on labels. */
enum {
	MAIL_FILTER_ALL_MESSAGES		= -3,
	MAIL_FILTER_UNREAD_MESSAGES		= -2,
	MAIL_FILTER_NO_LABEL			= -1,
	/* Labels go here */
	MAIL_FILTER_READ_MESSAGES		= 5000,
	MAIL_FILTER_RECENT_MESSAGES		= 5001,
	MAIL_FILTER_LAST_5_DAYS_MESSAGES	= 5002,
	MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS	= 5003,
	MAIL_FILTER_IMPORTANT_MESSAGES		= 5004,
	MAIL_FILTER_MESSAGES_NOT_JUNK		= 5005
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
	MAIL_NUM_SEARCH_RULES
};

/* Scope items are displayed in ascending order. */
enum {
	MAIL_SCOPE_CURRENT_FOLDER,
	MAIL_SCOPE_CURRENT_ACCOUNT,
	MAIL_SCOPE_ALL_ACCOUNTS
};

struct _EMailShellViewPrivate {

	/*** Other Stuff ***/

	/* These are just for convenience. */
	EMailShellBackend *mail_shell_backend;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;

	/* For UI merging and unmerging. */
	guint merge_id;
	guint label_merge_id;

	/* Filter rules correspond to the search entry menu. */
	EFilterRule *search_rules[MAIL_NUM_SEARCH_RULES];

	/* EShell::prepare-for-quit */
	gulong prepare_for_quit_handler_id;

	/* Search folders for interactive search. */
	CamelVeeFolder *search_account_all;
	CamelVeeFolder *search_account_current;
	CamelOperation *search_account_cancel;

	guint show_deleted : 1;
};

void		e_mail_shell_view_private_init
					(EMailShellView *mail_shell_view,
					 EShellViewClass *shell_view_class);
void		e_mail_shell_view_private_constructed
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_private_dispose
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_private_finalize
					(EMailShellView *mail_shell_view);

/* Private Utilities */

void		e_mail_shell_view_actions_init
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_restore_state
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_create_filter_from_selected
					(EMailShellView *mail_shell_view,
					 gint filter_type);
void		e_mail_shell_view_create_vfolder_from_selected
					(EMailShellView *mail_shell_view,
					 gint vfolder_type);
void		e_mail_shell_view_update_popup_labels
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_update_search_filter
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_update_sidebar
					(EMailShellView *mail_shell_view);

G_END_DECLS

#endif /* E_MAIL_SHELL_VIEW_PRIVATE_H */
