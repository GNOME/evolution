/* Evolution calendar - Framework for a calendar component editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkstock.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-messagebox.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>
#include <evolution-shell-component-utils.h>
#include "../print.h"
#include "../comp-util.h"
#include "save-comp.h"
#include "delete-comp.h"
#include "send-comp.h"
#include "changed-comp.h"
#include "cancel-comp.h"
#include "recur-comp.h"
#include "comp-editor.h"



/* Private part of the CompEditor structure */
struct _CompEditorPrivate {
	/* Client to use */
	ECal *client;
	
	/* Source client (where comp lives currently) */
	ECal *source_client;

	/* View to listen for changes */
	ECalView *view;

	/* Calendar object/uid we are editing; this is an internal copy */
	ECalComponent *comp;

	/* The pages we have */
	GList *pages;

	/* Notebook to hold the pages */
	GtkNotebook *notebook;
	
	gboolean changed;
	gboolean needs_send;

	CalObjModType mod;
	
 	gboolean existing_org;
 	gboolean user_org;
	
 	gboolean warned;
};



static gint comp_editor_key_press_event (GtkWidget *d, GdkEventKey *e);
static void comp_editor_finalize (GObject *object);

static void real_set_e_cal (CompEditor *editor, ECal *client);
static void real_edit_comp (CompEditor *editor, ECalComponent *comp);
static gboolean real_send_comp (CompEditor *editor, ECalComponentItipMethod method);
static gboolean prompt_to_save_changes (CompEditor *editor, gboolean send);
static void delete_comp (CompEditor *editor);
static void close_dialog (CompEditor *editor);

static void page_changed_cb (GtkObject *obj, gpointer data);
static void needs_send_cb (GtkObject *obj, gpointer data);
static void page_summary_changed_cb (GtkObject *obj, const char *summary, gpointer data);
static void page_dates_changed_cb (GtkObject *obj, CompEditorPageDates *dates, gpointer data);

static void obj_modified_cb (ECal *client, GList *objs, gpointer data);
static void obj_removed_cb (ECal *client, GList *uids, gpointer data);

G_DEFINE_TYPE (CompEditor, comp_editor, GTK_TYPE_DIALOG);

/* Class initialization function for the calendar component editor */
static void
comp_editor_class_init (CompEditorClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	klass->set_e_cal = real_set_e_cal;
	klass->edit_comp = real_edit_comp;
	klass->send_comp = real_send_comp;

	widget_class->key_press_event = comp_editor_key_press_event;
	object_class->finalize = comp_editor_finalize;
}

static void
listen_for_changes (CompEditor *editor)
{
	CompEditorPrivate *priv;
	const char *uid = NULL;

	priv = editor->priv;

	/* Discard change listener */
	if (priv->view) {
		g_signal_handlers_disconnect_matched (G_OBJECT (priv->view),
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      editor);
				
		g_object_unref (priv->view);
		priv->view = NULL;
	}
	
	/* Listen for changes */
	if (priv->comp)
		e_cal_component_get_uid (priv->comp, &uid);

	if (uid) {
		char *query;
		
		query = g_strdup_printf ("(uid? \"%s\")", uid);
		e_cal_get_query (priv->source_client, query, &priv->view, NULL);
		g_free (query);
	}
	
	if (priv->view) {
		g_signal_connect (priv->view, "objects_modified",
				  G_CALLBACK (obj_modified_cb), editor);

		g_signal_connect((priv->view), "objects_removed",
				 G_CALLBACK (obj_removed_cb), editor);

		e_cal_view_start (priv->view);
	}
}

/* This sets the focus to the toplevel, so any field being edited is committed.
   FIXME: In future we may also want to check some of the fields are valid,
   e.g. the EDateEdit fields. */
static void
commit_all_fields (CompEditor *editor)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

	gtk_window_set_focus (GTK_WINDOW (editor), NULL);
}

static void
send_timezone (gpointer key, gpointer value, gpointer user_data)
{
	icaltimezone *zone = value;
	CompEditor *editor = user_data;

	e_cal_add_timezone (editor->priv->client, zone, NULL);
}

static gboolean
save_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	ECalComponent *clone;
	GList *l;
	gboolean result;
	GError *error = NULL;
	GHashTable *timezones;
	const char *orig_uid;

	priv = editor->priv;

	if (!priv->changed)
		return TRUE;

	/* Stop listening because we are about to change things */
	if (priv->view) {
		g_signal_handlers_disconnect_matched (G_OBJECT (priv->view),
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      editor);

		g_object_unref (priv->view);
		priv->view = NULL;
	}

	/* Update on the server */
	timezones = g_hash_table_new (g_str_hash, g_str_equal);

	clone = e_cal_component_clone (priv->comp);
	for (l = priv->pages; l != NULL; l = l->next) {
		if (!comp_editor_page_fill_component (l->data, clone)) {
			g_object_unref (clone);
			g_hash_table_destroy (timezones);
			comp_editor_show_page (editor, COMP_EDITOR_PAGE (l->data));
			return FALSE;
		}

		/* retrieve all timezones */
		comp_editor_page_fill_timezones (l->data, timezones);
	}
	
	/* If we are not the organizer, we don't update the sequence number */
	if (!e_cal_component_has_organizer (clone) || itip_organizer_is_user (clone, priv->client))
		e_cal_component_commit_sequence (clone);
	else
		e_cal_component_abort_sequence (clone);

	g_object_unref (priv->comp);
	priv->comp = clone;

	e_cal_component_get_uid (priv->comp, &orig_uid);

	/* send timezones */
	g_hash_table_foreach (timezones, (GHFunc) send_timezone, editor);
	g_hash_table_destroy (timezones);

	/* send the component to the server */
	if (!cal_comp_is_on_server (priv->comp, priv->client)) {
		result = e_cal_create_object (priv->client, e_cal_component_get_icalcomponent (priv->comp), NULL, &error);
	} else {
		result = e_cal_modify_object (priv->client, e_cal_component_get_icalcomponent (priv->comp), priv->mod, &error);
	}

	if (!result) {
		GtkWidget *dlg;
		char *msg;

		msg = g_strdup (error ? error->message : _("Could not update object"));

		dlg = gnome_error_dialog (msg);
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));

		g_free (msg);
		if (error)
			g_error_free (error);

		return FALSE;
	} else {
		if (priv->source_client &&
		    !e_source_equal (e_cal_get_source (priv->client),
				     e_cal_get_source (priv->source_client)) &&
		    cal_comp_is_on_server (priv->comp, priv->source_client)) {
			/* Comp found a new home. Remove it from old one. */
			e_cal_remove_object (priv->source_client, orig_uid, NULL);

			/* Let priv->source_client point to new home, so we can move it
			 * again this session. */
			g_object_unref (priv->source_client);
			priv->source_client = g_object_ref (priv->client);
			
			listen_for_changes (editor);
		}

		priv->changed = FALSE;
	}

	return TRUE;
}

static gboolean
save_comp_with_send (CompEditor *editor)
{
	CompEditorPrivate *priv;
	gboolean send;

	priv = editor->priv;

	send = priv->changed && priv->needs_send;

	if (!save_comp (editor))
		return FALSE;

 	if (send && send_component_dialog ((GtkWindow *) editor, priv->client, priv->comp, !priv->existing_org)) {
 		if (itip_organizer_is_user (priv->comp, priv->client))
 			return comp_editor_send_comp (editor, E_CAL_COMPONENT_METHOD_REQUEST);
 		else
 			return comp_editor_send_comp (editor, E_CAL_COMPONENT_METHOD_REPLY);
 	}

	return TRUE;
}

static gboolean
prompt_to_save_changes (CompEditor *editor, gboolean send)
{
	CompEditorPrivate *priv;
	gboolean read_only;

	priv = editor->priv;

	if (!priv->changed)
		return TRUE;

	if (!e_cal_is_read_only (priv->client, &read_only, NULL) || read_only)
		return TRUE;

	switch (save_component_dialog (GTK_WINDOW(editor), priv->comp)) {
	case GTK_RESPONSE_YES: /* Save */
		if (e_cal_component_is_instance (priv->comp))
			if (!recur_component_dialog (priv->client, priv->comp, &priv->mod, GTK_WINDOW (editor)))
				return FALSE;

		if (send && save_comp_with_send (editor))
			return TRUE;
		else if (!send && save_comp (editor))
			return TRUE;
		else
			return FALSE;
	case GTK_RESPONSE_NO: /* Discard */
		return TRUE;
	case GTK_RESPONSE_CANCEL: /* Cancel */
	default:
		return FALSE;
	}
}

static void
response_cb (GtkWidget *widget, int response, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	
	priv = editor->priv;
	
	switch (response) {
	case GTK_RESPONSE_OK:
		commit_all_fields (editor);
		
		if (e_cal_component_is_instance (priv->comp))
			if (!recur_component_dialog (priv->client, priv->comp, &priv->mod, GTK_WINDOW (editor)))
				return;
		
		if (save_comp_with_send (editor))
			close_dialog (editor);
		break;
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_DELETE_EVENT:
		commit_all_fields (editor);
		
		if (prompt_to_save_changes (editor, TRUE))
			close_dialog (editor);
		break;
	}
}

/* Creates the basic in the editor */
static void
setup_widgets (CompEditor *editor)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

	/* Notebook */
	priv->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_widget_show (GTK_WIDGET (priv->notebook));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (editor)->vbox), GTK_WIDGET (priv->notebook),
			    TRUE, TRUE, 6);

	/* Buttons */
	gtk_dialog_add_button  (GTK_DIALOG (editor), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button  (GTK_DIALOG (editor), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (editor), GTK_RESPONSE_OK, FALSE);

	g_signal_connect (editor, "response", G_CALLBACK (response_cb), editor);
}

/* Object initialization function for the calendar component editor */
static void
comp_editor_init (CompEditor *editor)
{
	CompEditorPrivate *priv;

	priv = g_new0 (CompEditorPrivate, 1);
	editor->priv = priv;

	setup_widgets (editor);

	priv->pages = NULL;
	priv->changed = FALSE;
	priv->needs_send = FALSE;
	priv->mod = CALOBJ_MOD_ALL;
 	priv->existing_org = FALSE;
 	priv->user_org = FALSE;
 	priv->warned = FALSE;

	gtk_window_set_type_hint (GTK_WINDOW (editor), GDK_WINDOW_TYPE_HINT_NORMAL);
}


static gint
comp_editor_key_press_event (GtkWidget *d, GdkEventKey *e)
{
#if 0
	if (e->keyval == GDK_Escape) {
		if (prompt_to_save_changes (COMP_EDITOR (d), TRUE))
			close_dialog (COMP_EDITOR (d));
		return TRUE;
	}
#endif

	if (GTK_WIDGET_CLASS (comp_editor_parent_class)->key_press_event)
		return (* GTK_WIDGET_CLASS (comp_editor_parent_class)->key_press_event) (d, e);

	return FALSE;
}

/* Destroy handler for the calendar component editor */
static void
comp_editor_finalize (GObject *object)
{
	CompEditor *editor;
	CompEditorPrivate *priv;
	GList *l;

	editor = COMP_EDITOR (object);
	priv = editor->priv;

	if (priv->client) {
		g_object_unref (priv->client);
		priv->client = NULL;
	}
	
	if (priv->source_client) {
		g_object_unref (priv->source_client);
		priv->source_client = NULL;
	}

	if (priv->view) {
		g_signal_handlers_disconnect_matched (G_OBJECT (priv->view),
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      editor);

		g_object_unref (priv->view);
		priv->view = NULL;
	}

	/* We want to destroy the pages after the widgets get destroyed,
	   since they have lots of signal handlers connected to the widgets
	   with the pages as the data. */
	for (l = priv->pages; l != NULL; l = l->next)
		g_object_unref (l->data);

	if (priv->comp) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	g_free (priv);
	editor->priv = NULL;

	if (G_OBJECT_CLASS (comp_editor_parent_class)->finalize)
		(* G_OBJECT_CLASS (comp_editor_parent_class)->finalize) (object);
}


static void
delete_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	const char *uid;

	priv = editor->priv;

	e_cal_component_get_uid (priv->comp, &uid);
	e_cal_remove_object (priv->client, uid, NULL);
	close_dialog (editor);
}

/* Closes the dialog box and emits the appropriate signals */
static void
close_dialog (CompEditor *editor)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

	/* FIXME Unfortunately we do this here because otherwise corba
	   calls happen during destruction and we might get a change
	   notification back when we are in an inconsistent state */
	if (priv->view)
		g_signal_handlers_disconnect_matched (G_OBJECT (priv->view),
						      G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor); 
	
	gtk_widget_destroy (GTK_WIDGET (editor));
}



void
comp_editor_set_existing_org (CompEditor *editor, gboolean existing_org)
{
	CompEditorPrivate *priv;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	priv->existing_org = existing_org;
}

gboolean
comp_editor_get_existing_org (CompEditor *editor)
{
	CompEditorPrivate *priv;

	g_return_val_if_fail (editor != NULL, FALSE);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	priv = editor->priv;

	return priv->existing_org;
}

void
comp_editor_set_user_org (CompEditor *editor, gboolean user_org)
{
	CompEditorPrivate *priv;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	priv->user_org = user_org;
}

gboolean
comp_editor_get_user_org (CompEditor *editor)
{
	CompEditorPrivate *priv;

	g_return_val_if_fail (editor != NULL, FALSE);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	priv = editor->priv;

	return priv->user_org;
}


/**
 * comp_editor_set_changed:
 * @editor: A component editor
 * @changed: Value to set the changed state to
 *
 * Set the dialog changed state to the given value
 **/
void
comp_editor_set_changed (CompEditor *editor, gboolean changed)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

	priv->changed = changed;

	gtk_dialog_set_response_sensitive (GTK_DIALOG (editor), GTK_RESPONSE_OK, changed);
	gtk_dialog_set_default_response (GTK_DIALOG (editor), GTK_RESPONSE_OK);
}

/**
 * comp_editor_get_changed:
 * @editor: A component editor
 *
 * Gets the changed state of the dialog
 *
 * Return value: A boolean indicating if the dialog is in a changed
 * state
 **/
gboolean
comp_editor_get_changed (CompEditor *editor)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

	return priv->changed;
}

/**
 * comp_editor_set_needs_send:
 * @editor: A component editor
 * @needs_send: Value to set the needs send state to
 *
 * Set the dialog needs send state to the given value
 **/
void
comp_editor_set_needs_send (CompEditor *editor, gboolean needs_send)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

	priv->needs_send = needs_send;
}

/**
 * comp_editor_get_needs_send:
 * @editor: A component editor
 *
 * Gets the needs send state of the dialog
 *
 * Return value: A boolean indicating if the dialog is in a needs send
 * state
 **/
gboolean
comp_editor_get_needs_send (CompEditor *editor)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

	return priv->needs_send;
}

static void page_mapped_cb (GtkWidget *page_widget,
			    CompEditorPage *page)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (page_widget);
	if (!GTK_IS_WINDOW (toplevel))
		return;

	if (page->accel_group) {
		gtk_window_add_accel_group (GTK_WINDOW (toplevel),
					    page->accel_group);
	}
}

static void page_unmapped_cb (GtkWidget *page_widget,
			      CompEditorPage *page)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (page_widget);
	if (!GTK_IS_WINDOW (toplevel))
		return;

	if (page->accel_group) {
		gtk_window_remove_accel_group (GTK_WINDOW (toplevel),
					       page->accel_group);
	}
}

/**
 * comp_editor_append_page:
 * @editor: A component editor
 * @page: A component editor page
 * @label: Label of the page
 *
 * Appends a page to the editor notebook with the given label
 **/
void
comp_editor_append_page (CompEditor *editor,
			 CompEditorPage *page,
			 const char *label)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	GtkWidget *label_widget;
	gboolean is_first_page;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (label != NULL);

	priv = editor->priv;

	g_object_ref (page);

	/* If we are editing something, fill the widgets with current info */
	if (priv->comp != NULL) {
		ECalComponent *comp;

		comp = comp_editor_get_current_comp (editor);
		comp_editor_page_fill_widgets (page, comp);
		g_object_unref (comp);
	}

	page_widget = comp_editor_page_get_widget (page);
	g_assert (page_widget != NULL);

	label_widget = gtk_label_new (label);

	is_first_page = (priv->pages == NULL);

	priv->pages = g_list_append (priv->pages, page);
	gtk_notebook_append_page (priv->notebook, page_widget, label_widget);

	/* Listen for things happening on the page */
	g_signal_connect(page, "changed",
			    G_CALLBACK (page_changed_cb), editor);
	g_signal_connect(page, "needs_send",
			    G_CALLBACK (needs_send_cb), editor);
	g_signal_connect(page, "summary_changed",
			    G_CALLBACK (page_summary_changed_cb), editor);
	g_signal_connect(page, "dates_changed",
			    G_CALLBACK (page_dates_changed_cb), editor);

	/* Listen for when the page is mapped/unmapped so we can
	   install/uninstall the appropriate GtkAccelGroup. */
	g_signal_connect((page_widget), "map",
			    G_CALLBACK (page_mapped_cb), page);
	g_signal_connect((page_widget), "unmap",
			    G_CALLBACK (page_unmapped_cb), page);

	/* The first page is the main page of the editor, so we ask it to focus
	 * its main widget.
	 */
	if (is_first_page)
		comp_editor_page_focus_main_widget (page);
}

/**
 * comp_editor_remove_page:
 * @editor: A component editor
 * @page: A component editor page
 *
 * Removes the page from the component editor
 **/
void
comp_editor_remove_page (CompEditor *editor, CompEditorPage *page)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	gint page_num;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	priv = editor->priv;

	page_widget = comp_editor_page_get_widget (page);
	page_num = gtk_notebook_page_num (priv->notebook, page_widget);
	if (page_num == -1)
		return;
	
	/* Disconnect all the signals added in append_page(). */
	g_signal_handlers_disconnect_matched (page, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_disconnect_matched (page_widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page);

	gtk_notebook_remove_page (priv->notebook, page_num);

	priv->pages = g_list_remove (priv->pages, page);
	g_object_unref (page);
}

/**
 * comp_editor_show_page:
 * @editor:
 * @page:
 *
 *
 **/
void
comp_editor_show_page (CompEditor *editor, CompEditorPage *page)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	gint page_num;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	priv = editor->priv;

	page_widget = comp_editor_page_get_widget (page);
	page_num = gtk_notebook_page_num (priv->notebook, page_widget);
	gtk_notebook_set_page (priv->notebook, page_num);
}

/**
 * comp_editor_set_e_cal:
 * @editor: A component editor
 * @client: The calendar client to use
 *
 * Sets the calendar client used by the editor to update components
 **/
void
comp_editor_set_e_cal (CompEditor *editor, ECal *client)
{
	CompEditorClass *klass;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	klass = COMP_EDITOR_CLASS (G_OBJECT_GET_CLASS (editor));

	if (klass->set_e_cal)
		klass->set_e_cal (editor, client);
}

/**
 * comp_editor_get_e_cal:
 * @editor: A component editor
 *
 * Returns the calendar client of the editor
 *
 * Return value: The calendar client of the editor
 **/
ECal *
comp_editor_get_e_cal (CompEditor *editor)
{
	CompEditorPrivate *priv;

	g_return_val_if_fail (editor != NULL, NULL);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	priv = editor->priv;

	return priv->client;
}

/* Creates an appropriate title for the event editor dialog */
static char *
make_title_from_comp (ECalComponent *comp)
{
	char *title;
	const char *type_string;
	ECalComponentVType type;
	ECalComponentText text;

	if (!comp)
		return g_strdup (_("Edit Appointment"));

	type = e_cal_component_get_vtype (comp);
	switch (type) {
	case E_CAL_COMPONENT_EVENT:
		type_string = _("Appointment - %s");
		break;
	case E_CAL_COMPONENT_TODO:
		type_string = _("Task - %s");
		break;
	case E_CAL_COMPONENT_JOURNAL:
		type_string = _("Journal entry - %s");
		break;
	default:
		g_message ("make_title_from_comp(): Cannot handle object of type %d", type);
		return NULL;
	}

	e_cal_component_get_summary (comp, &text);
	if (text.value) {
		title = g_strdup_printf (type_string, text.value);
	} else {
		title = g_strdup_printf (type_string, _("No summary"));
	}

	return title;
}

/* Creates an appropriate title for the event editor dialog */
static char *
make_title_from_string (ECalComponent *comp, const char *str)
{
	char *title;
	const char *type_string;
	ECalComponentVType type;

	if (!comp)
		return g_strdup (_("Edit Appointment"));

	type = e_cal_component_get_vtype (comp);
	switch (type) {
	case E_CAL_COMPONENT_EVENT:
		type_string = _("Appointment - %s");
		break;
	case E_CAL_COMPONENT_TODO:
		type_string = _("Task - %s");
		break;
	case E_CAL_COMPONENT_JOURNAL:
		type_string = _("Journal entry - %s");
		break;
	default:
		g_message ("make_title_from_string(): Cannot handle object of type %d", type);
		return NULL;
	}

	if (str) {
		title = g_strdup_printf (type_string, str);
	} else {
		title = g_strdup_printf (type_string, _("No summary"));
	}

	return title;
}

static const char *
make_icon_from_comp (ECalComponent *comp)
{
	ECalComponentVType type;

	if (!comp)
		return "stock_calendar";

	type = e_cal_component_get_vtype (comp);
	switch (type) {
	case E_CAL_COMPONENT_EVENT:
		return "stock_new-appointment";
		break;
	case E_CAL_COMPONENT_TODO:
		return "stock_task";
		break;
	default:
		return "stock_calendar";
	}
}

/* Sets the event editor's window title from a calendar component */
static void
set_title_from_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	char *title;

	priv = editor->priv;
	title = make_title_from_comp (priv->comp);
	gtk_window_set_title (GTK_WINDOW (editor), title);
	g_free (title);
}

static void
set_title_from_string (CompEditor *editor, const char *str)
{
	CompEditorPrivate *priv;
	char *title;

	priv = editor->priv;
	title = make_title_from_string (priv->comp, str);
	gtk_window_set_title (GTK_WINDOW (editor), title);
	g_free (title);
}

static void
set_icon_from_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	const char *icon_name;
	GList *icon_list;

	priv = editor->priv;
	icon_name = make_icon_from_comp (priv->comp);

	icon_list = e_icon_factory_get_icon_list (icon_name);
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (editor), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}
}

static void
fill_widgets (CompEditor *editor)
{
	CompEditorPrivate *priv;
	GList *l;

	priv = editor->priv;

	for (l = priv->pages; l != NULL; l = l->next)
		comp_editor_page_fill_widgets (l->data, priv->comp);
}

static void
real_set_e_cal (CompEditor *editor, ECal *client)
{
	CompEditorPrivate *priv;
	GList *elem;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	if (client == priv->client)
		return;

	if (client) {
		g_return_if_fail (E_IS_CAL (client));
		g_return_if_fail (e_cal_get_load_state (client) ==
				  E_CAL_LOAD_LOADED);
		g_object_ref (client);
	}

	if (priv->client)
		g_object_unref (priv->client);

	priv->client = client;
	if (!priv->source_client)
		priv->source_client = g_object_ref (client);

	/* Pass the client to any pages that need it. */
	for (elem = priv->pages; elem; elem = elem->next)
		comp_editor_page_set_e_cal (elem->data, client);
}

static void
real_edit_comp (CompEditor *editor, ECalComponent *comp)
{
	CompEditorPrivate *priv;
	
	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	if (priv->comp) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (comp)
		priv->comp = e_cal_component_clone (comp);
	
 	priv->existing_org = e_cal_component_has_organizer (comp);
 	priv->user_org = itip_organizer_is_user (comp, priv->client);
 	priv->warned = FALSE;
 		
	set_title_from_comp (editor);
	set_icon_from_comp (editor);
	fill_widgets (editor);

	listen_for_changes (editor);
}


static gboolean
real_send_comp (CompEditor *editor, ECalComponentItipMethod method)
{
	CompEditorPrivate *priv;
	ECalComponent *tmp_comp;
	
	g_return_val_if_fail (editor != NULL, FALSE);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	priv = editor->priv;

	if (itip_send_comp (method, priv->comp, priv->client, NULL)) {
		tmp_comp = priv->comp;
		g_object_ref (tmp_comp);
		comp_editor_edit_comp (editor, tmp_comp);
		g_object_unref (tmp_comp);
		
		comp_editor_set_changed (editor, TRUE);
		save_comp (editor);

		return TRUE;
	}

	comp_editor_set_changed (editor, TRUE);

	return FALSE;
}


/**
 * comp_editor_edit_comp:
 * @editor: A component editor
 * @comp: A calendar component
 *
 * Starts the editor editing the given component
 **/
void
comp_editor_edit_comp (CompEditor *editor, ECalComponent *comp)
{
	CompEditorClass *klass;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	klass = COMP_EDITOR_CLASS (G_OBJECT_GET_CLASS (editor));

	if (klass->edit_comp)
		klass->edit_comp (editor, comp);
}

ECalComponent *
comp_editor_get_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;

	g_return_val_if_fail (editor != NULL, NULL);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	priv = editor->priv;

	return priv->comp;
}

ECalComponent *
comp_editor_get_current_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	ECalComponent *comp;
	GList *l;

	g_return_val_if_fail (editor != NULL, NULL);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	priv = editor->priv;

	comp = e_cal_component_clone (priv->comp);
	if (priv->changed) {
		for (l = priv->pages; l != NULL; l = l->next)
			comp_editor_page_fill_component (l->data, comp);
	}

	return comp;
}

/**
 * comp_editor_save_comp:
 * @editor:
 *
 *
 **/
gboolean
comp_editor_save_comp (CompEditor *editor, gboolean send)
{
	return prompt_to_save_changes (editor, send);
}

/**
 * comp_editor_delete_comp:
 * @editor:
 *
 *
 **/
void
comp_editor_delete_comp (CompEditor *editor)
{
	delete_comp (editor);
}

/**
 * comp_editor_send_comp:
 * @editor:
 * @method:
 *
 *
 **/
gboolean
comp_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method)
{
	CompEditorClass *klass;

	g_return_val_if_fail (editor != NULL, FALSE);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	klass = COMP_EDITOR_CLASS (G_OBJECT_GET_CLASS (editor));

	if (klass->send_comp)
		return klass->send_comp (editor, method);

	return FALSE;
}

gboolean
comp_editor_close (CompEditor *editor)
{
	gboolean close;
	
	g_return_val_if_fail (editor != NULL, FALSE);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	commit_all_fields (editor);
	
	close = prompt_to_save_changes (editor, TRUE);
	if (close)
		close_dialog (editor);

	return close;
}

/* Brings attention to a window by raising it and giving it focus */
static void
raise_and_focus (GtkWidget *widget)
{
	g_assert (GTK_WIDGET_REALIZED (widget));
	gdk_window_show (widget->window);
	gtk_widget_grab_focus (widget);
}

/**
 * comp_editor_focus:
 * @editor: A component editor
 *
 * Brings the editor window to the front and gives it focus
 **/
void
comp_editor_focus (CompEditor *editor)
{
	CompEditorPrivate *priv;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	gtk_widget_show (GTK_WIDGET (editor));
	raise_and_focus (GTK_WIDGET (editor));
}

/**
 * comp_editor_notify_client_changed:
 * @editor: A component editor.
 * 
 * Makes an editor emit the "client_changed" signal.
 **/
void
comp_editor_notify_client_changed (CompEditor *editor, ECal *client)
{
	GList *l;
	CompEditorPrivate *priv;
	gboolean read_only;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	priv->changed = TRUE;

	comp_editor_set_e_cal (editor, client);
	for (l = priv->pages; l != NULL; l = l->next)
		comp_editor_page_notify_client_changed (COMP_EDITOR_PAGE (l->data), client);

	if (!e_cal_is_read_only (client, &read_only, NULL))
		read_only = TRUE;

	gtk_dialog_set_response_sensitive (GTK_DIALOG (editor), GTK_RESPONSE_OK, !read_only);
}

static void
page_changed_cb (GtkObject *obj, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;

	priv = editor->priv;

	comp_editor_set_changed (editor, TRUE);

	if (!priv->warned && priv->existing_org && !priv->user_org) {
		e_notice (editor, GTK_MESSAGE_INFO,
			  _("Changes made to this item may be discarded if an update arrives"));
		priv->warned = TRUE;
	}
	
}

static void
needs_send_cb (GtkObject *obj, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;

	priv = editor->priv;

	comp_editor_set_needs_send (editor, TRUE);
}

/* Page signal callbacks */
static void
page_summary_changed_cb (GtkObject *obj, const char *summary, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	GList *l;

	priv = editor->priv;

	for (l = priv->pages; l != NULL; l = l->next)
		if (obj != l->data)
			comp_editor_page_set_summary (l->data, summary);

	if (!priv->warned && priv->existing_org && !priv->user_org) {
		e_notice (editor, GTK_MESSAGE_INFO,
			  _("Changes made to this item may be discarded if an update arrives"));
		priv->warned = TRUE;
	}

	set_title_from_string (editor, summary);
}

static void
page_dates_changed_cb (GtkObject *obj,
		       CompEditorPageDates *dates,
		       gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	GList *l;

	priv = editor->priv;

	for (l = priv->pages; l != NULL; l = l->next)
		if (obj != l->data)
			comp_editor_page_set_dates (l->data, dates);

	if (!priv->warned && priv->existing_org && !priv->user_org) {
		e_notice (editor, GTK_MESSAGE_INFO,
			  _("Changes made to this item may be discarded if an update arrives"));
		priv->warned = TRUE;
	}
}

static void
obj_modified_cb (ECal *client, GList *objects, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	ECalComponent *comp = NULL;

	priv = editor->priv;

	/* We queried based on a specific UID so we definitely changed */
	if (changed_component_dialog ((GtkWindow *) editor, priv->comp, FALSE, priv->changed)) {
		icalcomponent *icalcomp = icalcomponent_new_clone (objects->data);

		comp = e_cal_component_new ();
		if (e_cal_component_set_icalcomponent (comp, icalcomp)) {
			comp_editor_edit_comp (editor, comp);
		} else {
			GtkWidget *dlg;
			
			dlg = gnome_error_dialog (_("Unable to use current version!"));
			gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
			icalcomponent_free (icalcomp);
		}

		g_object_unref (comp);
	}
}

static void
obj_removed_cb (ECal *client, GList *uids, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;

	priv = editor->priv;

	if (changed_component_dialog ((GtkWindow *) editor, priv->comp, TRUE, priv->changed))
		close_dialog (editor);
}
