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
#include "Evolution-Addressbook-SelectNames.h"
#include "calendar-config.h"
#include "e-meeting-list-view.h"
#include <misc/e-cell-renderer-combo.h>
#include <addressbook/util/eab-destination.h>
#include "e-select-names-renderer.h"

#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames:" BASE_VERSION

struct _EMeetingListViewPrivate 
{
	EMeetingStore *store;

	EBook *ebook;

        GNOME_Evolution_Addressbook_SelectNames corba_select_names;
};

#define BUF_SIZE 1024

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

static GtkTreeViewClass *parent_class = NULL;

static void
start_addressbook_server (EMeetingListView *view)
{
	GError *error = NULL;

	view->priv->ebook = e_book_new ();
	if (!e_book_load_local_addressbook (view->priv->ebook, &error)) {
		g_warning ("start_addressbook_server(): %s", error->message);
		g_error_free (error);

		return;
	}
}
#if 0
static void
popup_delete_cb (GtkWidget *widget, gpointer data) 
{
	EMeetingListView *emlv = data;
	EMeetingListViewPrivate *priv;	
	EMeetingAttendee *ia;
	int pos = 0;
	
	priv = emlv->priv;
	
	ia = e_meeting_store_find_attendee_at_row (priv->store, priv->row);
 
	/* If the user deletes the attendee explicitly, assume they no
	   longer want the organizer showing up */
	if (ia == priv->ia) {
		g_object_unref (priv->ia);
		priv->ia = NULL;
	}	
		
	/* If this was a delegatee, no longer delegate */
	if (e_meeting_attendee_is_set_delfrom (ia)) {
		EMeetingAttendee *ib;
		
		ib = e_meeting_store_find_attendee (priv->model, e_meeting_attendee_get_delfrom (ia), &pos);
		if (ib != NULL) {
			e_meeting_attendee_set_delto (ib, NULL);
			e_meeting_attendee_set_edit_level (ib,  E_MEETING_ATTENDEE_EDIT_FULL);
		}		
	}
	
	/* Handle deleting all attendees in the delegation chain */	
	while (ia != NULL) {
		EMeetingAttendee *ib = NULL;

		g_object_ref (ia);
		g_ptr_array_add (priv->deleted_attendees, ia);
		e_meeting_store_remove_attendee (priv->model, ia);

		if (e_meeting_attendee_get_delto (ia) != NULL)
			ib = e_meeting_store_find_attendee (priv->model, e_meeting_attendee_get_delto (ia), NULL);
		ia = ib;
	}
}

enum {
	CAN_DELEGATE = 2,
	CAN_DELETE = 4
};

static gboolean
button_press_event (GtkWidget *widget, GdkEventButton *event, EMeetingListView *emlv)
{
	EMeetingListViewPrivate *priv;
	GtkWidget *menu;
	GtkTreePath *path;
	ESource *source = NULL;
	
	/* only process right-clicks */
	if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
		return FALSE;

	priv = emlv->priv;

	/* create the menu */
	menu = gtk_menu_new ();

	view_row = e_table_model_to_view_row (etable, row);
	priv->row = e_meeting_model_etable_view_to_model_row (etable, priv->model, view_row);

 	ia = e_meeting_model_find_attendee_at_row (priv->model, priv->row);
 	if (e_meeting_attendee_get_edit_level (ia) != E_MEETING_ATTENDEE_EDIT_FULL)
 		disable_mask = CAN_DELETE;
 
	/* FIXME: if you enable Delegate, then change index to '1'.
	 * (This has now been enabled). */
	/* context_menu[1].pixmap_widget = gnome_stock_new_with_icon (GNOME_STOCK_MENU_TRASH); */
	context_menu[1].pixmap_widget =
	  gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU);

	menu = e_popup_menu_create (context_menu, disable_mask, hide_mask, data);
	e_auto_kill_popup_menu_on_selection_done (menu);

	/* popup the menu */
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);

	return TRUE;
}
#endif
static void
emlv_finalize (GObject *obj)
{
	EMeetingListView *view = E_MEETING_LIST_VIEW (obj);
	EMeetingListViewPrivate *priv = view->priv;
	
	if (priv->ebook != NULL)
		g_object_unref (priv->ebook);

	if (priv->corba_select_names != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;
		CORBA_exception_init (&ev);
		bonobo_object_release_unref (priv->corba_select_names, &ev);
		CORBA_exception_free (&ev);
	}

	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
 		(* G_OBJECT_CLASS (parent_class)->finalize) (obj);
}

static void
emlv_class_init (GObjectClass *klass)
{
	parent_class = g_type_class_peek_parent (klass);

	klass->finalize = emlv_finalize;
}


static void
emlv_init (EMeetingListView *view)
{
	EMeetingListViewPrivate *priv;

	priv = g_new0 (EMeetingListViewPrivate, 1);

	view->priv = priv;

	priv->corba_select_names = CORBA_OBJECT_NIL;
	
	start_addressbook_server (view);

//	g_signal_connect (G_OBJECT (view), "button_press_event", G_CALLBACK (button_press_event), selector);
}

E_MAKE_TYPE (e_meeting_list_view, "EMeetingListView", EMeetingListView, emlv_class_init, emlv_init, GTK_TYPE_TREE_VIEW);

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
attendee_edited_cb (GtkCellRenderer *renderer, const gchar *path, const gchar *address, const gchar *name, GtkTreeView *view)
{
	value_edited (view, E_MEETING_STORE_ADDRESS_COL, path, address);
	value_edited (view, E_MEETING_STORE_CN_COL, path, name);
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
						     "address", E_MEETING_STORE_ADDRESS_COL,
						     "underline", E_MEETING_STORE_ATTENDEE_UNDERLINE_COL,
						     NULL);
	g_signal_connect (renderer, "cell_edited", G_CALLBACK (attendee_edited_cb), view);
	
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
process_section (EMeetingListView *view, EABDestination **cards, icalparameter_role role)
{
	EMeetingListViewPrivate *priv;
	int i;

	priv = view->priv;
	for (i = 0; i < G_N_ELEMENTS (cards); i++) {
		const char *name, *attendee = NULL;
		char *attr = NULL;

		name = eab_destination_get_name (cards[i]);

		/* Get the field as attendee from the backend */
		if (e_cal_get_ldap_attribute (e_meeting_store_get_e_cal (priv->store),
						   &attr, NULL)) {
			/* FIXME this should be more general */
			if (!g_ascii_strcasecmp (attr, "icscalendar")) {
				EContact *contact;

				/* FIXME: this does not work, have to use first
				   eab_destination_use_contact() */
				contact = eab_destination_get_contact (cards[i]);
				if (contact) {
					attendee = e_contact_get (contact, E_CONTACT_FREEBUSY_URL);
					if (!attendee)
						attendee = e_contact_get (contact, E_CONTACT_CALENDAR_URI);
				}
			}
		}

		/* If we couldn't get the attendee prior, get the email address as the default */
		if (attendee == NULL || *attendee == '\0') {
			attendee = eab_destination_get_email (cards[i]);
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

static void
select_names_ok_cb (BonoboListener *listener, const char *event_name, const CORBA_any *arg, CORBA_Environment *ev, gpointer data)
{
	EMeetingListView *view = E_MEETING_LIST_VIEW (data);
	int i;
	
	for (i = 0; sections[i] != NULL; i++) {
		EABDestination **destv;
		char *string = NULL;
		Bonobo_Control corba_control = GNOME_Evolution_Addressbook_SelectNames_getEntryBySection 
			(view->priv->corba_select_names, sections[i], ev);
		GtkWidget *control_widget = bonobo_widget_new_control_from_objref (corba_control, CORBA_OBJECT_NIL);

		bonobo_widget_get_property (BONOBO_WIDGET (control_widget), "destinations",
					    TC_CORBA_string, &string, NULL);
		destv = eab_destination_importv (string);
 		if (destv) {
 			process_section (view, destv, roles[i]);
 			g_free (destv);
 		}
	}
}

static void
add_section (GNOME_Evolution_Addressbook_SelectNames corba_select_names, const char *name)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_addSection (
					corba_select_names, name, gettext (name), &ev);

	CORBA_exception_free (&ev);
}

static gboolean
get_select_name_dialog (EMeetingListView *view) 
{
	EMeetingListViewPrivate *priv;
	CORBA_Environment ev;
	int i;
	
	priv = view->priv;

	CORBA_exception_init (&ev);

	if (priv->corba_select_names != CORBA_OBJECT_NIL) {
		int i;
		
		for (i = 0; sections[i] != NULL; i++) {			
			GtkWidget *control_widget;
			Bonobo_Control corba_control = GNOME_Evolution_Addressbook_SelectNames_getEntryBySection 
							(priv->corba_select_names, sections[i], &ev);
			if (BONOBO_EX (&ev)) {
				CORBA_exception_free (&ev);
				return FALSE;				
			}
			
			control_widget = bonobo_widget_new_control_from_objref (corba_control, CORBA_OBJECT_NIL);
			
			bonobo_widget_set_property (BONOBO_WIDGET (control_widget), "text", TC_CORBA_string, "", NULL);		
		}
		CORBA_exception_free (&ev);

		return TRUE;
	}
	
	priv->corba_select_names = bonobo_activation_activate_from_id (SELECT_NAMES_OAFID, 0, NULL, &ev);

	for (i = 0; sections[i] != NULL; i++)
		add_section (priv->corba_select_names, sections[i]);

	bonobo_event_source_client_add_listener (priv->corba_select_names,
						 (BonoboListenerCallbackFn) select_names_ok_cb,
						 "GNOME/Evolution:ok:dialog", NULL, view);
	
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}

void
e_meeting_list_view_invite_others_dialog (EMeetingListView *view)
{
	CORBA_Environment ev;
	
	if (!get_select_name_dialog (view))
		return;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_activateDialog (
				view->priv->corba_select_names, _("Required Participants"), &ev);

	CORBA_exception_free (&ev);
}

