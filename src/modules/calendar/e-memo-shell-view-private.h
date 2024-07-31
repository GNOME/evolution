/*
 * e-memo-shell-view-private.h
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

#ifndef E_MEMO_SHELL_VIEW_PRIVATE_H
#define E_MEMO_SHELL_VIEW_PRIVATE_H

#include "e-memo-shell-view.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include "shell/e-shell-utils.h"

#include "calendar/gui/comp-util.h"
#include "calendar/gui/e-cal-component-preview.h"
#include "calendar/gui/print.h"

#include "e-cal-base-shell-sidebar.h"

#include "e-memo-shell-backend.h"
#include "e-memo-shell-content.h"
#include "e-memo-shell-view-actions.h"

/* Shorthand, requires a variable named "shell_view". */
#define ACTION(name) \
	(E_SHELL_VIEW_ACTION_##name (shell_view))

G_BEGIN_DECLS

/* Filter items are displayed in ascending order.
 * Non-negative values are reserved for categories. */
enum {
	MEMO_FILTER_ANY_CATEGORY = -2,
	MEMO_FILTER_UNMATCHED = -1
};

/* Search items are displayed in ascending order. */
enum {
	MEMO_SEARCH_ADVANCED = -1,
	MEMO_SEARCH_SUMMARY_CONTAINS,
	MEMO_SEARCH_DESCRIPTION_CONTAINS,
	MEMO_SEARCH_ANY_FIELD_CONTAINS
};

struct _EMemoShellViewPrivate {

	/* These are just for convenience. */
	EMemoShellBackend *memo_shell_backend;
	EMemoShellContent *memo_shell_content;
	ECalBaseShellSidebar *memo_shell_sidebar;

	EClientCache *client_cache;
	gulong backend_error_handler_id;

	EMemoTable *memo_table;
	gulong open_component_handler_id;
	gulong popup_event_handler_id;
	gulong selection_change_1_handler_id;
	gulong selection_change_2_handler_id;

	ECalModel *model;
	gulong model_changed_handler_id;
	gulong model_rows_deleted_handler_id;
	gulong model_rows_inserted_handler_id;
	gulong row_appended_handler_id;

	ESourceSelector *selector;
	gulong selector_popup_event_handler_id;
	gulong primary_selection_changed_handler_id;
};

void		e_memo_shell_view_private_init
					(EMemoShellView *memo_shell_view);
void		e_memo_shell_view_private_constructed
					(EMemoShellView *memo_shell_view);
void		e_memo_shell_view_private_dispose
					(EMemoShellView *memo_shell_view);
void		e_memo_shell_view_private_finalize
					(EMemoShellView *memo_shell_view);

/* Private Utilities */

void		e_memo_shell_view_actions_init
					(EMemoShellView *memo_shell_view);
void		e_memo_shell_view_open_memo
					(EMemoShellView *memo_shell_view,
					 ECalModelComponent *comp_data);
void		e_memo_shell_view_update_sidebar
					(EMemoShellView *memo_shell_view);
void		e_memo_shell_view_update_search_filter
					(EMemoShellView *memo_shell_view);

G_END_DECLS

#endif /* E_MEMO_SHELL_VIEW_PRIVATE_H */
