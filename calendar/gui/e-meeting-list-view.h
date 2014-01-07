/*
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
 * Authors:
 *		Mike Kestner <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_MEETING_LIST_VIEW_H_
#define _E_MEETING_LIST_VIEW_H_

#include <gtk/gtk.h>

#include "e-meeting-store.h"

G_BEGIN_DECLS

#define E_TYPE_MEETING_LIST_VIEW		(e_meeting_list_view_get_type ())
#define E_MEETING_LIST_VIEW(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MEETING_LIST_VIEW, EMeetingListView))
#define E_MEETING_LIST_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MEETING_LIST_VIEW, EMeetingListViewClass))
#define E_IS_MEETING_LIST_VIEW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MEETING_LIST_VIEW))
#define E_IS_MEETING_LIST_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MEETING_LIST_VIEW))

typedef struct _EMeetingListView        EMeetingListView;
typedef struct _EMeetingListViewPrivate EMeetingListViewPrivate;
typedef struct _EMeetingListViewClass   EMeetingListViewClass;

struct _EMeetingListView {
	GtkTreeView parent;

	EMeetingListViewPrivate *priv;
};

struct _EMeetingListViewClass {
	GtkTreeViewClass parent_class;

	/* Notification Signals */
	void (*attendee_added) (EMeetingListView *emlv, EMeetingAttendee *attendee);
};

GType      e_meeting_list_view_get_type (void);

EMeetingListView *e_meeting_list_view_new (EMeetingStore *store);

void       e_meeting_list_view_column_set_visible (EMeetingListView *emlv, EMeetingStoreColumns column, gboolean visible);

void       e_meeting_list_view_edit (EMeetingListView *emlv, EMeetingAttendee *attendee);

void       e_meeting_list_view_invite_others_dialog (EMeetingListView *emlv);
void	   e_meeting_list_view_remove_attendee_from_name_selector (EMeetingListView *view, EMeetingAttendee *ma);
void	   e_meeting_list_view_remove_all_attendees_from_name_selector (EMeetingListView *view);

void       e_meeting_list_view_add_attendee_to_name_selector (EMeetingListView *view, EMeetingAttendee *ma);
void       e_meeting_list_view_set_editable (EMeetingListView *lview, gboolean set);
ENameSelector * e_meeting_list_view_get_name_selector (EMeetingListView *lview);
void e_meeting_list_view_set_name_selector (EMeetingListView *lview, ENameSelector *name_selector);

G_END_DECLS

#endif
