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

	GHashTable *renderers;
};

#define BUF_SIZE 1024

/* Signal IDs */
enum {
	ATTENDEE_ADDED,
	LAST_SIGNAL
};

static guint e_meeting_list_view_signals[LAST_SIGNAL] = { 0 };

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

	if (priv->name_selector) {
		g_object_unref (priv->name_selector);
		priv->name_selector = NULL;
	}

	if (priv->renderers) {
		g_hash_table_destroy (priv->renderers);
		priv->renderers = NULL;
	}

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

	e_meeting_list_view_signals [ATTENDEE_ADDED] = 
		g_signal_new ("attendee_added", 
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMeetingListViewClass, attendee_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, 
			      G_TYPE_POINTER);
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
	
	priv->renderers = g_hash_table_new (g_direct_hash, g_int_equal);

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

static guint 
get_index_from_role (icalparameter_role role)
{
	switch (role)	{
		case ICAL_ROLE_CHAIR:
			return 0;
		case ICAL_ROLE_REQPARTICIPANT:
			return 1;
		case ICAL_ROLE_OPTPARTICIPANT:
			return 2;
		case ICAL_ROLE_NONPARTICIPANT:
			return 3;
		default:
			return 4;
	}
}

void 
e_meeting_list_view_add_attendee_to_name_selector (EMeetingListView *view, EMeetingAttendee *ma)
{
	EDestinationStore *destination_store;
	ENameSelectorModel *name_selector_model;
	EDestination *des;
	EMeetingListViewPrivate *priv;
	guint i = 1;

	priv = view->priv;

	name_selector_model = e_name_selector_peek_model (priv->name_selector);
	i = get_index_from_role (e_meeting_attendee_get_role (ma));
	e_name_selector_model_peek_section (name_selector_model, sections [i],
					    NULL, &destination_store);
	des = e_destination_new ();
	e_destination_set_email (des, itip_strip_mailto (e_meeting_attendee_get_address (ma)));
	e_destination_set_name (des, e_meeting_attendee_get_cn (ma));
	e_destination_store_append_destination (destination_store, des);
	g_object_unref (des);
}

void 
e_meeting_list_view_remove_attendee_from_name_selector (EMeetingListView *view, EMeetingAttendee *ma)
{
	GList             *destinations, *l;
	EDestinationStore *destination_store;
	ENameSelectorModel *name_selector_model;
	const char *madd = NULL;
	EMeetingListViewPrivate *priv;
	guint i = 1;
	
	priv = view->priv;
		
	name_selector_model = e_name_selector_peek_model (priv->name_selector);
	i = get_index_from_role (e_meeting_attendee_get_role (ma));
	e_name_selector_model_peek_section (name_selector_model, sections [i],
					    NULL, &destination_store);
	destinations = e_destination_store_list_destinations (destination_store);
	madd = itip_strip_mailto (e_meeting_attendee_get_address (ma));
	
	for (l = destinations; l; l = g_list_next (l)) {
		const char *attendee = NULL;		
		EDestination *des = l->data;


		if (e_destination_is_evolution_list (des)) {
			GList *l, *dl;
	
			dl = e_destination_list_get_dests (des);

			for (l = dl; l; l = l->next) {
				attendee = e_destination_get_email (l->data);
				if (madd && attendee && g_str_equal (madd, attendee)) {
					g_object_unref (l->data);
					l = g_list_remove (l, l->data);
					break;
				}
			}
		} else {
			attendee = e_destination_get_email (des);
		
			if (madd && attendee && g_str_equal (madd, attendee)) {
			attendee = e_destination_get_email (des);
			e_destination_store_remove_destination (destination_store, des);
			}
		}
	}
	
	g_list_free (destinations);
}

static void
attendee_edited_cb (GtkCellRenderer *renderer, const gchar *path, GList *addresses, GList *names, GtkTreeView *view)
{
	EMeetingStore *model = E_MEETING_STORE (gtk_tree_view_get_model (view));
	GtkTreePath *treepath = gtk_tree_path_new_from_string (path);
	int row = gtk_tree_path_get_indices (treepath)[0];
	EMeetingAttendee *existing_attendee;
	gboolean removed = FALSE;

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
			e_meeting_list_view_add_attendee_to_name_selector (E_MEETING_LIST_VIEW (view), attendee);
		}

		if (existing_attendee) {
			removed = TRUE;
			e_meeting_store_remove_attendee (model, existing_attendee);
		}
		
	} else if (g_list_length (addresses) == 1) {
		char *name = names->data, *email = addresses->data;
		int existing_row;

		if (!((name && *name) || (email && *email)) || ((e_meeting_store_find_attendee (model, email, &existing_row) != NULL) && existing_row != row)){
			if (existing_attendee) {
				removed = TRUE;
				e_meeting_store_remove_attendee (model, existing_attendee);
			}
		} else {
			EMeetingAttendee *attendee = e_meeting_attendee_new ();

			if (existing_attendee)
				e_meeting_list_view_remove_attendee_from_name_selector (E_MEETING_LIST_VIEW (view),
						existing_attendee);

			value_edited (view, E_MEETING_STORE_ADDRESS_COL, path, email);
			value_edited (view, E_MEETING_STORE_CN_COL, path, name);

			e_meeting_attendee_set_address (attendee, g_strdup (email));
			e_meeting_attendee_set_cn (attendee, g_strdup (name));
			e_meeting_attendee_set_role (attendee, ICAL_ROLE_REQPARTICIPANT);
			e_meeting_list_view_add_attendee_to_name_selector (E_MEETING_LIST_VIEW (view), attendee);
			g_object_unref (attendee);
		}
	} else {
		if (existing_attendee) {
			const char *address = e_meeting_attendee_get_address (existing_attendee);
			
			if (address && *address)
				return;

			 removed = TRUE;
			e_meeting_store_remove_attendee (model, existing_attendee);
		}
	}

	gtk_tree_path_free (treepath);

	if (!removed) 
		g_signal_emit_by_name (G_OBJECT (view), "attendee_added", (gpointer) existing_attendee); 
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
build_table (EMeetingListView *lview)
{
	GtkCellRenderer *renderer;
	GtkTreeView *view = GTK_TREE_VIEW (lview);
	EMeetingListViewPrivate *priv;
	GHashTable *edit_table;
	GtkTreeViewColumn *col;
	int pos;
	
	priv = lview->priv;
	edit_table = priv->renderers;
	gtk_tree_view_set_headers_visible (view, TRUE);
	gtk_tree_view_set_rules_hint (view, TRUE);

	renderer = e_select_names_renderer_new ();
	g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
	/* The extra space is just a hack to occupy more space for Attendee */
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("Attendee                          "), renderer,
						     "text", E_MEETING_STORE_ATTENDEE_COL,
						     "name", E_MEETING_STORE_CN_COL,
						     "email", E_MEETING_STORE_ADDRESS_COL,
						     "underline", E_MEETING_STORE_ATTENDEE_UNDERLINE_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_object_set (col, "width", 50, NULL);
	g_signal_connect (renderer, "cell_edited", G_CALLBACK (attendee_edited_cb), view);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (attendee_editing_canceled_cb), view);
	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_ATTENDEE_COL), renderer);	
	
	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_type_strings (), "editable", TRUE, NULL);
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("Type"), renderer,
						     "text", E_MEETING_STORE_TYPE_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_signal_connect (renderer, "edited", G_CALLBACK (type_edited_cb), view);
	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_TYPE_COL), renderer); 
	
	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_role_strings (), "editable", TRUE, NULL);
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("Role"), renderer,
						     "text", E_MEETING_STORE_ROLE_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_signal_connect (renderer, "edited", G_CALLBACK (role_edited_cb), view);
	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_ROLE_COL), renderer);

	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_rsvp_strings (), "editable", TRUE, NULL);
	/* To translators: RSVP means "please reply" */
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("RSVP"), renderer,
						     "text", E_MEETING_STORE_RSVP_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_signal_connect (renderer, "edited", G_CALLBACK (rsvp_edited_cb), view);
	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_RSVP_COL), renderer);

	renderer = e_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "list", get_status_strings (), "editable", TRUE, NULL);
	pos = gtk_tree_view_insert_column_with_attributes (view, -1, _("Status"), renderer,
						     "text", E_MEETING_STORE_STATUS_COL,
						     NULL);
	col = gtk_tree_view_get_column (view, pos -1);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	g_signal_connect (renderer, "edited", G_CALLBACK (status_edited_cb), view);
	g_hash_table_insert (edit_table, GINT_TO_POINTER (E_MEETING_STORE_STATUS_COL), renderer);
	
	priv->renderers = edit_table;
}

static void
change_edit_cols_for_user (gpointer key, gpointer value, gpointer user_data)
{
       GtkCellRenderer *renderer = (GtkCellRenderer *) value;
       int key_val = GPOINTER_TO_INT (key);
       switch (key_val)
       {
               case E_MEETING_STORE_ATTENDEE_COL:
                       g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
               break;
               case E_MEETING_STORE_ROLE_COL:
                       g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
               break;
               case E_MEETING_STORE_TYPE_COL:
                       g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
               break;
               case E_MEETING_STORE_RSVP_COL:
                       g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
               break;
               case E_MEETING_STORE_STATUS_COL:
                       g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
               break;
       }
}

static void   
change_edit_cols_for_organizer (gpointer key, gpointer value, gpointer user_data)
{
       GtkCellRenderer *renderer = (GtkCellRenderer *) value;
       guint edit_level = GPOINTER_TO_INT (user_data); 
       g_object_set (G_OBJECT (renderer), "editable", GINT_TO_POINTER (edit_level), NULL);
}

static void
row_activated_cb (GtkTreeSelection *selection, EMeetingListView *view)
{
       EMeetingAttendee *existing_attendee;
       EMeetingListViewPrivate *priv;
       GtkTreeIter iter;
       int row;
       EMeetingAttendeeEditLevel el;
       gint  edit_level;
       GtkTreeModel *model; 
       GtkTreePath *path = NULL;

       priv = view->priv;
               

       if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
               path = gtk_tree_model_get_path  (model, &iter);
       }
       
       if (!path)
	       return;
       
       row = gtk_tree_path_get_indices (path)[0];
       existing_attendee = e_meeting_store_find_attendee_at_row (priv->store, row);
       el = e_meeting_attendee_get_edit_level (existing_attendee);
       
       switch (el)
       {
               case  E_MEETING_ATTENDEE_EDIT_NONE:
               edit_level = FALSE;
               g_hash_table_foreach (priv->renderers, change_edit_cols_for_organizer, GINT_TO_POINTER (edit_level));
               break;
       
               case E_MEETING_ATTENDEE_EDIT_FULL:
               edit_level = TRUE;
               g_hash_table_foreach (priv->renderers, change_edit_cols_for_organizer, GINT_TO_POINTER (edit_level));
               break;
             
               case E_MEETING_ATTENDEE_EDIT_STATUS:
               edit_level = FALSE;
               g_hash_table_foreach (priv->renderers, change_edit_cols_for_user, GINT_TO_POINTER (edit_level));
               break;
       }
               
}
 

EMeetingListView *
e_meeting_list_view_new (EMeetingStore *store)
{
	EMeetingListView *view = g_object_new (E_TYPE_MEETING_LIST_VIEW, NULL);
	GtkTreeSelection *selection;

	if (view) {
		view->priv->store = store;
		gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (store));
		build_table (view);
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(view));
	g_signal_connect (selection, "changed", G_CALLBACK (row_activated_cb), view);
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
process_section (EMeetingListView *view, GList *destinations, icalparameter_role role, GSList **la)
{
	EMeetingListViewPrivate *priv;
	GList *l;

	priv = view->priv;
	for (l = destinations; l; l = g_list_next (l)) {
		EDestination *destination = l->data, *des = NULL;
		const GList *list_dests, *l;
		GList card_dest;

		if (e_destination_is_evolution_list (destination)) {
			list_dests = e_destination_list_get_dests (destination);
		} else {
			EContact *contact = e_destination_get_contact (destination);
			/* check if the contact is contact list which is not expanded yet */
			/* we expand it by getting the list again from the server forming the query */
			if (contact && e_contact_get (contact , E_CONTACT_IS_LIST)) {
				EBook *book = NULL;
				ENameSelectorDialog *dialog;
				EContactStore *c_store;
				GList *books, *l;
				char *uri = e_contact_get (contact, E_CONTACT_BOOK_URI);

				dialog = e_name_selector_peek_dialog (view->priv->name_selector);
				c_store = dialog->name_selector_model->contact_store;
				books = e_contact_store_get_books (c_store);

				for (l = books; l; l = l->next) {
					EBook *b = l->data;
					if (g_str_equal (uri, e_book_get_uri (b))) {
						book = b;
						break;
					}
				}
				
				if (book) {
					GList *contacts;
					EContact *n_con = NULL;
					char *qu;
					EBookQuery *query;

					qu = g_strdup_printf ("(is \"full_name\" \"%s\")", 
							(char *) e_contact_get (contact, E_CONTACT_FULL_NAME));
					query = e_book_query_from_string (qu); 

					if (!e_book_get_contacts (book, query, &contacts, NULL)) {
						g_warning ("Could not get contact from the book \n");
						return;
					} else {
						des = e_destination_new ();
						n_con = contacts->data;

						e_destination_set_contact (des, n_con, 0);
						list_dests = e_destination_list_get_dests (des);

						g_list_foreach (contacts, (GFunc) g_object_unref, NULL); 	 
						g_list_free (contacts);
					}

					e_book_query_unref (query);
					g_free (qu);
				}
			} else {
				card_dest.next = NULL;
				card_dest.prev = NULL;
				card_dest.data = destination;
				list_dests = &card_dest;
			}
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
			} else {
				if (g_slist_length (*la) == 1) {
					g_slist_free (*la);
					*la = NULL;
				} else
					*la = g_slist_remove_link (*la, g_slist_find_custom (*la, attendee, g_strcasecmp));
			}
		}

		if (des) {
			g_object_unref (des);
			des = NULL;
		}
				
	}
}

static void
add_to_list (gpointer data, gpointer u_data)
{
	GSList **user_data = u_data;

	*user_data = g_slist_append (*user_data, itip_strip_mailto (e_meeting_attendee_get_address (data)));
}

static void
name_selector_dialog_close_cb (ENameSelectorDialog *dialog, gint response, gpointer data)
{
	EMeetingListView   *view = E_MEETING_LIST_VIEW (data);
	ENameSelectorModel *name_selector_model;
	EMeetingStore *store;
	const GPtrArray *attendees;
	int i;
	GSList 		  *la = NULL, *l;

	name_selector_model = e_name_selector_peek_model (view->priv->name_selector);
	store = E_MEETING_STORE (gtk_tree_view_get_model (view));
	attendees = e_meeting_store_get_attendees (store);
	
	/* get all the email ids of the attendees */
	g_ptr_array_foreach ((GPtrArray *)attendees, (GFunc) add_to_list, &la);
	
	for (i = 0; sections[i] != NULL; i++) {
		EDestinationStore *destination_store;
		GList             *destinations;

		e_name_selector_model_peek_section (name_selector_model, sections [i],
						    NULL, &destination_store);
		g_assert (destination_store);

		destinations = e_destination_store_list_destinations (destination_store);
		process_section (view, destinations, roles [i], &la);
		g_list_free (destinations);
	}

	/* remove the deleted attendees from name selector */
	for (l = la; l != NULL; l = l->next) {
		EMeetingAttendee *ma = NULL;
		const char *email = l->data;
		int i;

		ma = e_meeting_store_find_attendee (store, email, &i);

		if (ma)
			e_meeting_store_remove_attendee (store, ma);
	}

	g_slist_free (la);
	gtk_widget_hide (GTK_WIDGET (dialog));
}

void
e_meeting_list_view_invite_others_dialog (EMeetingListView *view)
{
	ENameSelectorDialog *dialog;

	dialog = e_name_selector_peek_dialog (view->priv->name_selector);
	gtk_widget_show (GTK_WIDGET (dialog));
}
