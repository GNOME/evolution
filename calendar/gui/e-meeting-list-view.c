/* 
 * e-meeting-list-view.c
 *
 * Authors: Mike Kestner  <mkestner@ximian.com>
 * 	    JP Rosevear  <jpr@ximian.com>
 *
 * Copyright (C) 2003  Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gtk/gtktreemodel.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libebook/e-book.h>
#include <libebook/e-vcard.h>
#include <libecal/e-cal-component.h>
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserverui/e-name-selector.h>
#include "calendar-config.h"
#include "e-meeting-list-view.h"
#include <misc/e-cell-renderer-combo.h>
#include <libebook/e-destination.h>
#include "e-select-names-renderer.h"

struct _EMeetingListViewPrivate {
	EMeetingStore *store;

	ENameSelector *name_selector;
};

#define BUF_SIZE 1024

static void name_selector_dialog_close_cb (ENameSelectorDialog *dialog, gint response, gpointer data);

static char *sections[] = {N_("Chair Persons"), 
			   N_("Required Participants"), 
			   N_("Optional Participants"), 
			   N_("Resources"),
			   NULL};
static icalparameter_role roles[] = {ICAL_ROLE_CHAIR,
				     ICAL_ROLE_REQPARTICIPANT,
				     ICAL_ROLE_OPTPARTICIPANT,
				     ICAL_ROLE_NONPARTICIPANT,
				     ICAL_ROLE_NONE};

G_DEFINE_TYPE (EMeetingListView, e_meeting_list_view, GTK_TYPE_TREE_VIEW);

static void
e_meeting_list_view_finalize (GObject *obj)
{
	EMeetingListView *view = E_MEETING_LIST_VIEW (obj);
	EMeetingListViewPrivate *priv = view->priv;

	if (priv->name_selector)
		g_object_unref (priv->name_selector);

	g_free (priv);

	if (G_OBJECT_CLASS (e_meeting_list_view_parent_class)->finalize)
 		(* G_OBJECT_CLASS (e_meeting_list_view_parent_class)->finalize) (obj);
}

static void
e_meeting_list_view_class_init (EMeetingListViewClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = e_meeting_list_view_finalize;
}


static void
add_section (ENameSelector *name_selector, const char *name)
{
	ENameSelectorModel *name_selector_model;

	name_selector_model = e_name_selector_peek_model (name_selector);
	e_name_selector_model_add_section (name_selector_model, name, gettext (name), NULL);
}

static void
e_meeting_list_view_init (EMeetingListView *view)
{
	EMeetingListViewPrivate *priv;
	ENameSelectorDialog *name_selector_dialog;
	gint i;

	priv = g_new0 (EMeetingListViewPrivate, 1);

	view->priv = priv;

	priv->name_selector = e_name_selector_new ();

	for (i = 0; sections [i]; i++)
		add_section (priv->name_selector, sections [i]);

	name_selector_dialog = e_name_selector_peek_dialog (view->priv->name_selector);
	gtk_window_set_title (GTK_WINDOW (name_selector_dialog), _("Required Participants"));
 	g_signal_connect (name_selector_dialog, "response",
			  G_CALLBACK (name_selector_dialog_close_cb), view);

}

static GList *
get_type_strings ()
{
	GList *strings = NULL;

	strings = g_list_append (strings, (char*) _("Individual"));
        strings = g_list_append (strings, (char*) _("Group"));
        strings = g_list_append (strings, (char*) _("Resource"));
        strings = g_list_append (strings, (char*) _("Room"));
        strings = g_list_append (strings, (char*) _("Unknown"));

	return strings;
}

static GList *
get_role_strings ()
{
	GList *strings = NULL;

	strings = g_list_append (strings, (char*) _("Chair"));
	strings = g_list_append (strings, (char*) _("Required Participant"));
	strings = g_list_append (strings, (char*) _("Optional Participant"));
	strings = g_list_append (strings, (char*) _("Non-Participant"));
	strings = g_list_append (strings, (char*) _("Unknown"));

	return strings;
}

static GList *
get_rsvp_strings ()
{
	GList *strings = NULL;

	strings = g_list_append (strings, (char*) _("Yes"));
	strings = g_list_append (strings, (char*) _("No"));

	return strings;
}

static GList *
get_status_strings ()
{
	GList *strings = NULL;

	strings = g_list_append (strings, (char*) _("Needs Action"));
	strings = g_list_append (strings, (char*) _("Accepted"));
	strings = g_list_append (strings, (char*) _("Declined"));
	strings = g_list_append (strings, (char*) _("Tentative"));
	strings = g_list_append (strings, (char*) _("Delegated"));

	return strings;
}

static void
value_edited (GtkTreeView *view, gint col, const gchar *path, const gchar *text)
{
	EMeetingStore *model = E_MEETING_STORE (gtk_tree_view_get_model (view));
	GtkTreePath *treepath = gtk_tree_path_new_from_string (path);
	int row = gtk_tree_path_get_indices (treepath)[0];
       
	e_meeting_store_set_value (model, row, col, text);
	gtk_tree_path_free (treepath);
}

static void
attendee_edited_cb (GtkCellRenderer *renderer, const gchar *path, GList *addresses, GList *names, GtkTreeView *view)
{
	EMeetingStore *model = E_MEETING_STORE (gtk_tree_view_get_model (view));
	GtkTreePath *treepath = gtk_tree_path_new_from_string (path);
	int row = gtk_tree_path_get_indices (treepath)[0];
	EMeetingAttendee *existing_attendee;

	existing_attendee = e_meeting_store_find_attendee_at_row (model, row);

	if (g_list_length (addresses) > 1) {
		EMeetingAttendee *attendee;
		GList *l, *m;

		for (l = addresses, m = names; l && m; l = l->next, m = m->next) {
			char *name = m->data, *email = l->data;
			
			if (!((name && *name) || (email && *email)))
				continue;
			
			if (e_meeting_store_find_attendee (model, email, NULL) != NULL)
				continue;
			
			attendee = e_meeting_store_add_attendee_with_defaults (model);
			e_meeting_attendee_set_address (attendee, g_strdup (l->data));
			e_meeting_attendee_set_cn (attendee, g_strdup (m->data));
			if (existing_attendee) {
				/* FIXME Should we copy anything else? */
				e_meeting_attendee_set_cutype (attendee, e_meeting_attendee_get_cutype (existing_attendee));
				e_meeting_attendee_set_role (attendee, e_meeting_attendee_get_role (existing_attendee));
				e_meeting_attendee_set_rsvp (attendee, e_meeting_attendee_get_rsvp (existing_attendee));
				e_meeting_attendee_set_status (attendee, e_meeting_attendee_get_status (existing_attendee));
			}
		}

		if (existing_attendee)
			e_meeting_store_remove_attendee (model, existing_attendee);
		
	} else if (g_list_length (addresses) == 1) {
		char *name = names->data, *email = addresses->data;

		if (!((name && *name) || (email && *email)) || e_meeting_store_find_attendee (model, email, NULL) != NULL) {
			if (existing_attendee)
				e_meeting_store_remove_attendee (model, existing_attendee);
		} else {
			value_edited (view, E_MEETING_STORE_ADDRESS_COL, path, email);
			value_edited (view, E_MEETING_STORE_CN_COL, path, name);
		}
	} else {
		if (existing_attendee)
			e_meeting_store_remove_attendee (model, existing_attendee);
	}

	gtk_tree_path_free (treepath);
}

static void
attendee_editing_canceled_cb (GtkCellRenderer *renderer, GtkTreeView *view) 
{
	EMeetingStore *model = E_MEETING_STORE (gtk_tree_view_get_model (view));
	GtkTreePath *path;
	EMeetingAttendee *existing_attendee;
	int row;

	/* This is for newly added attendees when the editing is cancelled */
	gtk_tree_view_get_cursor (view, &path, NULL);
	if (!path)
		return;
	
	row = gtk_tree_path_get_indices (path)[0];
	existing_attendee = e_meeting_store_find_attendee_at_row (model, row);
	if (existing_attendee) {
		if (!e_meeting_attendee_is_set_cn (existing_attendee) && !e_meeting_attendee_is_set_address (existing_attendee))
			e_meeting_store_remove_attendee (model, existing_attendee);
	}
	
	gtk_tree_path_free (path);
}


static void
type_edited_cb (GtkCellRenderer *renderer, const gchar *path, const gchar *text, GtkTreeView *view)
{
	value_edited (view, E_MEETING_STORE_TYPE_COL, path, text);
}

static void
role_edited_cb (GtkCellRenderer *renderer, const gchar *path, const gchar *text, GtkTreeView *view)
{
	value_edited (view, E_MEETING_STORE_ROLE_COL, path, text);
}

static void
rsvp_edited_cb (GtkCellRenderer *renderer, const gchar *path, const gchar *text, GtkTreeView *view)
{
	value_edited (view, E_MEETING_STORE_RSVP_COL, path, text);
}

static void
status_edited_cb (GtkCellRenderer *renderer, const gchar *path, const gchar *text, GtkTreeView *view)
{
	value_edited (view, E_MEETING_STORE_STATUS_COL, path, text);
}

static void
build_table (GtkTreeView *view)
{
	GtkCellRenderer *renderer;
	
	gtk_tree_view_set_headers_visible (view, TRUE);
	gtk_tree_view_set_rules_hint (view, TRUE);

	renderer = e_select_names_renderer_new ();
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
	gtk_tree_view_insert_column_with_attributes (view, -1, _("Attendee"), renderer,
						     "text", E_MEETING_STORE_ATTENDEE_COL,
						     "name", E_MEETING_STORE_CN_COL,
						     "email", E_MEETING_STORE_ADDRESS_COL,
						     "underline", E_MEETING_STORE_ATTENDEE_UNDERLINE_COL,
						     NULL);
	g_signal_connect (renderer, "cell_edited", G_CALLBACK (attendee_edited_cb), view);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (attendee_editing_canceled_cb), view);
	
	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_type_strings (), "editable", TRUE, NULL);
	gtk_tree_view_insert_column_with_attributes (view, -1, _("Type"), renderer,
						     "text", E_MEETING_STORE_TYPE_COL,
						     NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (type_edited_cb), view);
	
	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_role_strings (), "editable", TRUE, NULL);
	gtk_tree_view_insert_column_with_attributes (view, -1, _("Role"), renderer,
						     "text", E_MEETING_STORE_ROLE_COL,
						     NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (role_edited_cb), view);

	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_rsvp_strings (), "editable", TRUE, NULL);
	gtk_tree_view_insert_column_with_attributes (view, -1, _("RSVP"), renderer,
						     "text", E_MEETING_STORE_RSVP_COL,
						     NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (rsvp_edited_cb), view);

	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_status_strings (), "editable", TRUE, NULL);
	gtk_tree_view_insert_column_with_attributes (view, -1, _("Status"), renderer,
						     "text", E_MEETING_STORE_STATUS_COL,
						     NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (status_edited_cb), view);
}

EMeetingListView *
e_meeting_list_view_new (EMeetingStore *store)
{
	EMeetingListView *view = g_object_new (E_TYPE_MEETING_LIST_VIEW, NULL);

	if (view) {
		view->priv->store = store;
		gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (store));
		build_table (GTK_TREE_VIEW (view));
	}

	return view;
}

void
e_meeting_list_view_column_set_visible (EMeetingListView *view, const gchar *col_name, gboolean visible)
{
	GList *cols, *l;

	cols = gtk_tree_view_get_columns (GTK_TREE_VIEW (view));

	for (l = cols; l; l = l->next) {
		GtkTreeViewColumn *col = (GtkTreeViewColumn *) l->data;

		if (strcmp (gtk_tree_view_column_get_title (col), col_name) == 0) {
			gtk_tree_view_column_set_visible (col, visible);
			break;
		}
	}
}

void
e_meeting_list_view_edit (EMeetingListView *emlv, EMeetingAttendee *attendee)
{
	EMeetingListViewPrivate *priv;
	GtkTreePath *path;
	GtkTreeViewColumn *focus_col;
	
	priv = emlv->priv;

	g_return_if_fail (emlv != NULL);
	g_return_if_fail (E_IS_MEETING_LIST_VIEW (emlv));
	g_return_if_fail (attendee != NULL);

	path = e_meeting_store_find_attendee_path (priv->store, attendee);
	focus_col = gtk_tree_view_get_column (GTK_TREE_VIEW (emlv), 0);	
	
	if (path) {
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (emlv), path, focus_col, TRUE);

		gtk_tree_path_free (path);
	}	
}

static void
process_section (EMeetingListView *view, GList *destinations, icalparameter_role role)
{
	EMeetingListViewPrivate *priv;
	GList *l;

	priv = view->priv;
	for (l = destinations; l; l = g_list_next (l)) {
		EDestination *destination = l->data;
		const GList *list_dests, *l;
		GList card_dest;

		if (e_destination_is_evolution_list (destination)) {
			list_dests = e_destination_list_get_dests (destination);
		} else {
			card_dest.next = NULL;
			card_dest.prev = NULL;
			card_dest.data = destination;
			list_dests = &card_dest;
		}		
		
		for (l = list_dests; l; l = l->next) {
			EDestination *dest = l->data;
			const char *name, *attendee = NULL;
			char *attr = NULL;
			
			name = e_destination_get_name (dest);

			/* Get the field as attendee from the backend */
			if (e_cal_get_ldap_attribute (e_meeting_store_get_e_cal (priv->store),
						      &attr, NULL)) {
				/* FIXME this should be more general */
				if (!g_ascii_strcasecmp (attr, "icscalendar")) {
					EContact *contact;

					/* FIXME: this does not work, have to use first
					   e_destination_use_contact() */
					contact = e_destination_get_contact (dest);
					if (contact) {
						attendee = e_contact_get (contact, E_CONTACT_FREEBUSY_URL);
						if (!attendee)
							attendee = e_contact_get (contact, E_CONTACT_CALENDAR_URI);
					}
				}
			}

			/* If we couldn't get the attendee prior, get the email address as the default */
			if (attendee == NULL || *attendee == '\0') {
				attendee = e_destination_get_email (dest);
			}
		
			if (attendee == NULL || *attendee == '\0')
				continue;
		
			if (e_meeting_store_find_attendee (priv->store, attendee, NULL) == NULL) {
				EMeetingAttendee *ia = e_meeting_store_add_attendee_with_defaults (priv->store);

				e_meeting_attendee_set_address (ia, g_strdup_printf ("MAILTO:%s", attendee));
				e_meeting_attendee_set_role (ia, role);
				if (role == ICAL_ROLE_NONPARTICIPANT)
					e_meeting_attendee_set_cutype (ia, ICAL_CUTYPE_RESOURCE);
				e_meeting_attendee_set_cn (ia, g_strdup (name));
			}
		}
		
	}
}

static void
name_selector_dialog_close_cb (ENameSelectorDialog *dialog, gint response, gpointer data)
{
	EMeetingListView   *view = E_MEETING_LIST_VIEW (data);
	ENameSelectorModel *name_selector_model;
	int i;

	name_selector_model = e_name_selector_peek_model (view->priv->name_selector);
	
	for (i = 0; sections[i] != NULL; i++) {
		EDestinationStore *destination_store;
		GList             *destinations;

		e_name_selector_model_peek_section (name_selector_model, sections [i],
						    NULL, &destination_store);
		g_assert (destination_store);

		destinations = e_destination_store_list_destinations (destination_store);
		process_section (view, destinations, roles [i]);
		g_list_free (destinations);
	}

	gtk_widget_hide (GTK_WIDGET (dialog));
}

void
e_meeting_list_view_invite_others_dialog (EMeetingListView *view)
{
	ENameSelectorDialog *dialog;

	dialog = e_name_selector_peek_dialog (view->priv->name_selector);
	gtk_widget_show (GTK_WIDGET (dialog));
}
