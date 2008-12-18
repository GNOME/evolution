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
#include <camel/camel-vtrash-folder.h>

#include "e-util/gconf-bridge.h"
#include "widgets/menus/gal-view-instance.h"

#include "em-composer-utils.h"
#include "em-folder-properties.h"
#include "em-folder-selector.h"
#include "em-folder-utils.h"
#include "em-utils.h"
#include "mail-autofilter.h"
#include "mail-ops.h"
#include "mail-send-recv.h"
#include "mail-vfolder.h"

#include "e-mail-shell-content.h"
#include "e-mail-shell-module.h"
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

G_BEGIN_DECLS

/* Filter items are displayed in ascending order. */
enum {
	MAIL_FILTER_ALL_MESSAGES,
	MAIL_FILTER_UNREAD_MESSAGES,
	MAIL_FILTER_NO_LABEL,
	MAIL_FILTER_LABEL_IMPORTANT,
	MAIL_FILTER_LABEL_WORK,
	MAIL_FILTER_LABEL_PERSONAL,
	MAIL_FILTER_LABEL_TO_DO,
	MAIL_FILTER_LABEL_LATER,
	MAIL_FILTER_READ_MESSAGES,
	MAIL_FILTER_RECENT_MESSAGES,
	MAIL_FILTER_LAST_5_DAYS_MESSAGES,
	MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS,
	MAIL_FILTER_IMPORTANT_MESSAGES,
	MAIL_FILTER_MESSAGES_NOT_JUNK
};

/* Search items are displayed in ascending order. */
enum {
	MAIL_SEARCH_SUBJECT_OR_SENDER_CONTAINS,
	MAIL_SEARCH_SUBJECT_OR_RECIPIENTS_CONTAINS,
	MAIL_SEARCH_RECIPIENTS_CONTAIN,
	MAIL_SEARCH_MESSAGE_CONTAINS,
	MAIL_SEARCH_SUBJECT_CONTAINS,
	MAIL_SEARCH_SENDER_CONTAINS,
	MAIL_SEARCH_BODY_CONTAINS
};

/* Scope items are displayed in ascending order. */
enum {
	MAIL_SCOPE_CURRENT_FOLDER,
	MAIL_SCOPE_CURRENT_ACCOUNT,
	MAIL_SCOPE_ALL_ACCOUNTS,
	MAIL_SCOPE_CURRENT_MESSAGE
};

struct _EMailShellViewPrivate {

	/*** UI Management ***/

	GtkActionGroup *mail_actions;
	GtkActionGroup *filter_actions;

	/*** Other Stuff ***/

	/* These are just for convenience. */
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
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
void		e_mail_shell_view_update_sidebar
					(EMailShellView *mail_shell_view);
void		e_mail_shell_view_create_filter_from_selected
					(EMailShellView *mail_shell_view,
					 gint filter_type);
void		e_mail_shell_view_create_vfolder_from_selected
					(EMailShellView *mail_shell_view,
					 gint vfolder_type);

G_END_DECLS

#endif /* E_MAIL_SHELL_VIEW_PRIVATE_H */
