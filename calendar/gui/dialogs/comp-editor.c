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
#include <libgnomeui/gnome-window-icon.h>
#include <libgnomeui/gnome-messagebox.h>
#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-ui-util.h>
#include <gal/widgets/e-unicode.h>
#include <e-util/e-dialog-utils.h>
#include <evolution-shell-component-utils.h>
#include "../print.h"
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
	CalClient *client;

	/* Calendar object/uid we are editing; this is an internal copy */
	CalComponent *comp;

	/* The pages we have */
	GList *pages;

	/* UI Component for the dialog */
	BonoboUIComponent *uic;

	/* Notebook to hold the pages */
	GtkNotebook *notebook;

	GtkWidget *filesel;

	gboolean changed;
	gboolean needs_send;

	CalObjModType mod;
	
 	gboolean existing_org;
 	gboolean user_org;
	
 	gboolean warned;
 	
	gboolean updating;
};



static void comp_editor_class_init (CompEditorClass *class);
static void comp_editor_init (CompEditor *editor);
static gint comp_editor_key_press_event (GtkWidget *d, GdkEventKey *e);
static void comp_editor_finalize (GObject *object);

static void real_set_cal_client (CompEditor *editor, CalClient *client);
static void real_edit_comp (CompEditor *editor, CalComponent *comp);
static gboolean real_send_comp (CompEditor *editor, CalComponentItipMethod method);
static gboolean prompt_to_save_changes (CompEditor *editor, gboolean send);
static void delete_comp (CompEditor *editor);
static void close_dialog (CompEditor *editor);

static void page_changed_cb (GtkObject *obj, gpointer data);
static void page_summary_changed_cb (GtkObject *obj, const char *summary, gpointer data);
static void page_dates_changed_cb (GtkObject *obj, CompEditorPageDates *dates, gpointer data);

static void obj_updated_cb (CalClient *client, const char *uid, gpointer data);
static void obj_removed_cb (CalClient *client, const char *uid, gpointer data);

static void save_cmd (GtkWidget *widget, gpointer data);
static void save_close_cmd (GtkWidget *widget, gpointer data);
static void save_as_cmd (GtkWidget *widget, gpointer data);
static void delete_cmd (GtkWidget *widget, gpointer data);
static void print_cmd (GtkWidget *widget, gpointer data);
static void print_preview_cmd (GtkWidget *widget, gpointer data);
static void print_setup_cmd (GtkWidget *widget, gpointer data);
static void close_cmd (GtkWidget *widget, gpointer data);

static gint delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data);

static EPixmap pixmaps [] =
{
	E_PIXMAP ("/menu/File/FileSave",			"save-16.png"),
	E_PIXMAP ("/menu/File/FileSaveAndClose",		"save-16.png"),
	E_PIXMAP ("/menu/File/FileSaveAs",			"save-as-16.png"),

	E_PIXMAP ("/menu/File/FileDelete",			"evolution-trash-mini.png"),

	E_PIXMAP ("/menu/File/FilePrint",			"print.xpm"),
	E_PIXMAP ("/menu/File/FilePrintPreview",		"print-preview.xpm"),

	E_PIXMAP ("/Toolbar/FileSaveAndClose",		        "buttons/save-24.png"),
	E_PIXMAP ("/Toolbar/FilePrint",			        "buttons/print.png"),
	E_PIXMAP ("/Toolbar/FileDelete",			"buttons/delete-message.png"),

	E_PIXMAP_END
};

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("FileSave", save_cmd),
	BONOBO_UI_UNSAFE_VERB ("FileSaveAndClose", save_close_cmd),
	BONOBO_UI_UNSAFE_VERB ("FileSaveAs", save_as_cmd),
	BONOBO_UI_UNSAFE_VERB ("FileDelete", delete_cmd),
	BONOBO_UI_UNSAFE_VERB ("FilePrint", print_cmd),
	BONOBO_UI_UNSAFE_VERB ("FilePrintPreview", print_preview_cmd),
	BONOBO_UI_UNSAFE_VERB ("FilePrintSetup", print_setup_cmd),
	BONOBO_UI_UNSAFE_VERB ("FileClose", close_cmd),

	BONOBO_UI_VERB_END
};

static GtkObjectClass *parent_class;



E_MAKE_TYPE (comp_editor, "CompEditor", CompEditor, comp_editor_class_init, comp_editor_init,
	     BONOBO_TYPE_WINDOW);

/* Class initialization function for the calendar component editor */
static void
comp_editor_class_init (CompEditorClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_ref(BONOBO_TYPE_WINDOW);

	klass->set_cal_client = real_set_cal_client;
	klass->edit_comp = real_edit_comp;
	klass->send_comp = real_send_comp;

	widget_class->key_press_event = comp_editor_key_press_event;
	object_class->finalize = comp_editor_finalize;
}

/* Creates the basic in the editor */
static void
setup_widgets (CompEditor *editor)
{
	CompEditorPrivate *priv;
	BonoboUIContainer *container;
	GtkWidget *vbox;

	priv = editor->priv;

	/* Window and basic vbox */
	container = bonobo_ui_container_new ();
	editor = (CompEditor *) bonobo_window_construct (BONOBO_WINDOW (editor), container,
							 "event-editor", "iCalendar Editor");
	g_signal_connect((editor), "delete_event",
			    G_CALLBACK (delete_event_cb), editor);

	priv->uic = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (priv->uic,
					   bonobo_object_corba_objref (BONOBO_OBJECT (container)),
					   NULL);
	bonobo_ui_engine_config_set_path (bonobo_window_get_ui_engine (BONOBO_WINDOW (editor)),
					  "/evolution/UIConf/kvps");

	bonobo_ui_component_add_verb_list_with_data (priv->uic, verbs, editor);
	bonobo_ui_util_set_ui (priv->uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-comp-editor.xml",
			       "evolution-calendar", NULL);
	e_pixmaps_update (priv->uic, pixmaps);

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_widget_show (vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	bonobo_window_set_contents (BONOBO_WINDOW (editor), vbox);

	/* Notebook */
	priv->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_widget_show (GTK_WIDGET (priv->notebook));
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (priv->notebook),
			    TRUE, TRUE, 6);
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
}


static gint
comp_editor_key_press_event (GtkWidget *d, GdkEventKey *e)
{
	if (e->keyval == GDK_Escape) {
		if (prompt_to_save_changes (COMP_EDITOR (d), TRUE))
			close_dialog (COMP_EDITOR (d));
		return TRUE;
	}

	if (GTK_WIDGET_CLASS (parent_class)->key_press_event)
		return (* GTK_WIDGET_CLASS (parent_class)->key_press_event) (d, e);

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
		g_signal_handlers_disconnect_matched (priv->client, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
		g_object_unref (priv->client);
		priv->client = NULL;
	}
	
	/* We want to destroy the pages after the widgets get destroyed,
	   since they have lots of signal handlers connected to the widgets
	   with the pages as the data. */
	for (l = priv->pages; l != NULL; l = l->next)
		g_object_unref((l->data));

	if (priv->comp) {
		g_object_unref((priv->comp));
		priv->comp = NULL;
	}

	bonobo_object_unref (priv->uic);

	g_free (priv);
	editor->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static gboolean
save_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	CalComponent *clone;
	GList *l;
	CalClientResult result;

	priv = editor->priv;

	if (!priv->changed)
		return TRUE;

	clone = cal_component_clone (priv->comp);
	for (l = priv->pages; l != NULL; l = l->next) {
		if (!comp_editor_page_fill_component (l->data, clone)) {
			g_object_unref((clone));
			comp_editor_show_page (editor, COMP_EDITOR_PAGE (l->data));
			return FALSE;
		}
	}
	
	/* If we are not the organizer, we don't update the sequence number */
	if (!cal_component_has_organizer (clone) || itip_organizer_is_user (clone, priv->client))
		cal_component_commit_sequence (clone);
	else
		cal_component_abort_sequence (clone);

	g_object_unref((priv->comp));
	priv->comp = clone;

	priv->updating = TRUE;

	if (cal_component_is_instance (priv->comp))
		result = cal_client_update_object_with_mod (priv->client, priv->comp, priv->mod);
	else
		result = cal_client_update_object (priv->client, priv->comp);
	if (result != CAL_CLIENT_RESULT_SUCCESS) {
		GtkWidget *dlg;
		char *msg;

		switch (result) {
		case CAL_CLIENT_RESULT_INVALID_OBJECT :
			msg = g_strdup (_("Could not update invalid object"));
			break;
		case CAL_CLIENT_RESULT_NOT_FOUND :
			msg = g_strdup (_("Object not found, not updated"));
			break;
		case CAL_CLIENT_RESULT_PERMISSION_DENIED :
			msg = g_strdup (_("You don't have permissions to update this object"));
			break;
		default :
			msg = g_strdup (_("Could not update object"));
			break;
		}

		dlg = gnome_error_dialog (msg);
		gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		g_free (msg);

		return FALSE;
	} else {
		priv->changed = FALSE;
	}

	priv->updating = FALSE;

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
 			return comp_editor_send_comp (editor, CAL_COMPONENT_METHOD_REQUEST);
 		else
 			return comp_editor_send_comp (editor, CAL_COMPONENT_METHOD_REPLY);
 	}

	return TRUE;
}

static void
delete_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	const char *uid;

	priv = editor->priv;

	cal_component_get_uid (priv->comp, &uid);
	priv->updating = TRUE;
	cal_client_remove_object (priv->client, uid);
	priv->updating = FALSE;
	close_dialog (editor);
}

static gboolean
prompt_to_save_changes (CompEditor *editor, gboolean send)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

	if (!priv->changed)
		return TRUE;

	switch (save_component_dialog (GTK_WINDOW (editor))) {
	case GTK_RESPONSE_YES: /* Save */
		if (cal_component_is_instance (priv->comp))
			if (!recur_component_dialog (priv->comp, &priv->mod, GTK_WINDOW (editor)))
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

/* Closes the dialog box and emits the appropriate signals */
static void
close_dialog (CompEditor *editor)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

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

	g_object_ref((page));

	/* If we are editing something, fill the widgets with current info */
	if (priv->comp != NULL) {
		CalComponent *comp;

		comp = comp_editor_get_current_comp (editor);
		comp_editor_page_fill_widgets (page, comp);
		g_object_unref((comp));
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
	g_object_unref((page));
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
 * comp_editor_set_cal_client:
 * @editor: A component editor
 * @client: The calendar client to use
 *
 * Sets the calendar client used by the editor to update components
 **/
void
comp_editor_set_cal_client (CompEditor *editor, CalClient *client)
{
	CompEditorClass *klass;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	klass = COMP_EDITOR_CLASS (G_OBJECT_GET_CLASS (editor));

	if (klass->set_cal_client)
		klass->set_cal_client (editor, client);
}

/**
 * comp_editor_get_cal_client:
 * @editor: A component editor
 *
 * Returns the calendar client of the editor
 *
 * Return value: The calendar client of the editor
 **/
CalClient *
comp_editor_get_cal_client (CompEditor *editor)
{
	CompEditorPrivate *priv;

	g_return_val_if_fail (editor != NULL, NULL);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	priv = editor->priv;

	return priv->client;
}

/* Creates an appropriate title for the event editor dialog */
static char *
make_title_from_comp (CalComponent *comp)
{
	char *title;
	const char *type_string;
	CalComponentVType type;
	CalComponentText text;

	if (!comp)
		return g_strdup (_("Edit Appointment"));

	type = cal_component_get_vtype (comp);
	switch (type) {
	case CAL_COMPONENT_EVENT:
		type_string = _("Appointment - %s");
		break;
	case CAL_COMPONENT_TODO:
		type_string = _("Task - %s");
		break;
	case CAL_COMPONENT_JOURNAL:
		type_string = _("Journal entry - %s");
		break;
	default:
		g_message ("make_title_from_comp(): Cannot handle object of type %d", type);
		return NULL;
	}

	cal_component_get_summary (comp, &text);
	if (text.value) {
		title = g_strdup_printf (type_string, text.value);
	} else {
		title = g_strdup_printf (type_string, _("No summary"));
	}

	return title;
}

/* Creates an appropriate title for the event editor dialog */
static char *
make_title_from_string (CalComponent *comp, const char *str)
{
	char *title;
	const char *type_string;
	CalComponentVType type;

	if (!comp)
		return g_strdup (_("Edit Appointment"));

	type = cal_component_get_vtype (comp);
	switch (type) {
	case CAL_COMPONENT_EVENT:
		type_string = _("Appointment - %s");
		break;
	case CAL_COMPONENT_TODO:
		type_string = _("Task - %s");
		break;
	case CAL_COMPONENT_JOURNAL:
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
make_icon_from_comp (CalComponent *comp)
{
	CalComponentVType type;

	if (!comp)
		return EVOLUTION_IMAGESDIR "/evolution-calendar-mini.png";

	type = cal_component_get_vtype (comp);
	switch (type) {
	case CAL_COMPONENT_EVENT:
		return EVOLUTION_IMAGESDIR "/buttons/new_appointment.png";
		break;
	case CAL_COMPONENT_TODO:
		return EVOLUTION_IMAGESDIR "/buttons/new_task.png";
		break;
	default:
		return EVOLUTION_IMAGESDIR "/evolution-calendar-mini.png";
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
	const char *file;

	priv = editor->priv;
	file = make_icon_from_comp (priv->comp);
	gnome_window_icon_set_from_file (GTK_WINDOW (editor), file);
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
real_set_cal_client (CompEditor *editor, CalClient *client)
{
	CompEditorPrivate *priv;
	GList *elem;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	if (client == priv->client)
		return;

	if (client) {
		g_return_if_fail (IS_CAL_CLIENT (client));
		g_return_if_fail (cal_client_get_load_state (client) ==
				  CAL_CLIENT_LOAD_LOADED);
		g_object_ref((client));
	}

	if (priv->client) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->client),
					       editor);
		g_object_unref((priv->client));
	}

	priv->client = client;

	/* Pass the client to any pages that need it. */
	for (elem = priv->pages; elem; elem = elem->next)
		comp_editor_page_set_cal_client (elem->data, client);

	g_signal_connect((priv->client), "obj_updated",
			    G_CALLBACK (obj_updated_cb), editor);

	g_signal_connect((priv->client), "obj_removed",
			    G_CALLBACK (obj_removed_cb), editor);
}

static void
real_edit_comp (CompEditor *editor, CalComponent *comp)
{
	CompEditorPrivate *priv;
	
	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	if (priv->comp) {
		g_object_unref((priv->comp));
		priv->comp = NULL;
	}

	if (comp)
		priv->comp = cal_component_clone (comp);
	
 	priv->existing_org = cal_component_has_organizer (comp);
 	priv->user_org = itip_organizer_is_user (comp, priv->client);
 	priv->warned = FALSE;
 		
	set_title_from_comp (editor);
	set_icon_from_comp (editor);
	fill_widgets (editor);
}


static gboolean
real_send_comp (CompEditor *editor, CalComponentItipMethod method)
{
	CompEditorPrivate *priv;
	CalComponent *tmp_comp;
	
	g_return_val_if_fail (editor != NULL, FALSE);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	priv = editor->priv;

	if (itip_send_comp (method, priv->comp, priv->client, NULL)) {
		tmp_comp = priv->comp;
		g_object_ref((tmp_comp));
		comp_editor_edit_comp (editor, tmp_comp);
		g_object_unref((tmp_comp));
		
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
comp_editor_edit_comp (CompEditor *editor, CalComponent *comp)
{
	CompEditorClass *klass;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	klass = COMP_EDITOR_CLASS (G_OBJECT_GET_CLASS (editor));

	if (klass->edit_comp)
		klass->edit_comp (editor, comp);
}

CalComponent *
comp_editor_get_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;

	g_return_val_if_fail (editor != NULL, NULL);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	priv = editor->priv;

	return priv->comp;
}

CalComponent *
comp_editor_get_current_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	CalComponent *comp;
	GList *l;

	g_return_val_if_fail (editor != NULL, NULL);
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	priv = editor->priv;

	comp = cal_component_clone (priv->comp);
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
comp_editor_send_comp (CompEditor *editor, CalComponentItipMethod method)
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

/**
 * comp_editor_merge_ui:
 * @editor:
 * @filename:
 * @verbs:
 *
 *
 **/
void
comp_editor_merge_ui (CompEditor *editor,
		      const char *filename,
		      BonoboUIVerb *verbs,
		      EPixmap *component_pixmaps)
{
	CompEditorPrivate *priv;
	char *path;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	path = g_strconcat (EVOLUTION_UIDIR "/", filename, NULL);

	bonobo_ui_util_set_ui (priv->uic, EVOLUTION_DATADIR, path, "evolution-calendar", NULL);
	bonobo_ui_component_add_verb_list_with_data (priv->uic, verbs, editor);

	g_free (path);

	if (component_pixmaps != NULL)
		e_pixmaps_update (priv->uic, component_pixmaps);
}

/**
 * comp_editor_set_ui_prop:
 * @editor:
 * @path:
 * @attr:
 * @val:
 *
 *
 **/
void
comp_editor_set_ui_prop (CompEditor *editor,
			 const char *path,
			 const char *attr,
			 const char *val)
{
	CompEditorPrivate *priv;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	bonobo_ui_component_set_prop (priv->uic, path, attr, val, NULL);
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

/* Menu Commands */
static void
save_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	
	priv = editor->priv;

	commit_all_fields (editor);

	if (cal_component_is_instance (priv->comp))
		if (!recur_component_dialog (priv->comp, &priv->mod, GTK_WINDOW (editor)))
			return;

	save_comp_with_send (editor);
}

static void
save_close_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	
	priv = editor->priv;
	
	commit_all_fields (editor);

	if (cal_component_is_instance (priv->comp))
		if (!recur_component_dialog (priv->comp, &priv->mod, GTK_WINDOW (editor)))
			return;

	if (save_comp_with_send (editor))
		close_dialog (editor);
}

static void
save_as_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	char *filename;
	char *ical_string;
	FILE *file;
	
	priv = editor->priv;

	commit_all_fields (editor);

	filename = e_file_dialog_save (_("Save as..."));
	if (filename == NULL)
		return;
	
	ical_string = cal_client_get_component_as_string (priv->client, priv->comp);
	if (ical_string == NULL) {
		g_warning ("Couldn't convert item to a string");
		return;
	}
	
	file = fopen (filename, "w");
	if (file == NULL) {
		g_warning ("Couldn't save item");
		return;
	}
	
	fprintf (file, ical_string);
	g_free (ical_string);
	fclose (file);
}

static void
delete_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	CalComponentVType vtype;

	priv = editor->priv;

	vtype = cal_component_get_vtype (priv->comp);

	if (delete_component_dialog (priv->comp, FALSE, 1, vtype, GTK_WIDGET (editor))) {
		if (itip_organizer_is_user (priv->comp, priv->client) 
		    && cancel_component_dialog ((GtkWindow *) editor,
						priv->client, priv->comp, TRUE))
			itip_send_comp (CAL_COMPONENT_METHOD_CANCEL, priv->comp, priv->client, NULL);

		delete_comp (editor);
	}
}

static void
print_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CalComponent *comp;

	commit_all_fields (editor);

	comp = comp_editor_get_current_comp (editor);
	print_comp (comp, editor->priv->client, FALSE);
	g_object_unref((comp));
}

static void
print_preview_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CalComponent *comp;

	commit_all_fields (editor);

	comp = comp_editor_get_current_comp (editor);
	print_comp (comp, editor->priv->client, TRUE);
	g_object_unref((comp));
}

static void
print_setup_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;

	priv = editor->priv;

	print_setup ();
}

static void
close_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);

	commit_all_fields (editor);

	if (prompt_to_save_changes (editor, TRUE))
		close_dialog (editor);
}

static void
page_changed_cb (GtkObject *obj, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;

	priv = editor->priv;

	priv->changed = TRUE;

	if (!priv->warned && priv->existing_org && !priv->user_org) {
		e_notice (editor, GTK_MESSAGE_INFO,
			  _("Changes made to this item may be discarded if an update arrives"));
		priv->warned = TRUE;
	}
	
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

	priv->changed = TRUE;

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

	priv->changed = TRUE;

	if (!priv->warned && priv->existing_org && !priv->user_org) {
		e_notice (editor, GTK_MESSAGE_INFO,
			  _("Changes made to this item may be discarded if an update arrives"));
		priv->warned = TRUE;
	}
}

static void
obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	CalComponent *comp = NULL;
	CalClientGetStatus status;
	const char *edit_uid;

	priv = editor->priv;

	cal_component_get_uid (priv->comp, &edit_uid);

	if (!strcmp (uid, edit_uid) && !priv->updating) {
		if (changed_component_dialog ((GtkWindow *) editor, priv->comp, FALSE, priv->changed)) {
			icalcomponent *icalcomp;

			status = cal_client_get_object (priv->client, uid, &icalcomp);
			if (status == CAL_CLIENT_GET_SUCCESS) {
				comp = cal_component_new ();
				if (cal_component_set_icalcomponent (comp, icalcomp))
					comp_editor_edit_comp (editor, comp);
				else {
					GtkWidget *dlg;

					dlg = gnome_error_dialog (_("Unable to obtain current version!"));
					gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
					icalcomponent_free (icalcomp);
				}

				g_object_unref((comp));
			} else {
				GtkWidget *dlg;

				dlg = gnome_error_dialog (_("Unable to obtain current version!"));
				gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
			}
		}
	}
}

static void
obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	const char *edit_uid;

	priv = editor->priv;

	cal_component_get_uid (priv->comp, &edit_uid);

	if (!strcmp (uid, edit_uid) && !priv->updating) {
		if (changed_component_dialog ((GtkWindow *) editor, priv->comp, TRUE, priv->changed))
			close_dialog (editor);
	}
}

static gint
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);

	if (prompt_to_save_changes (editor, TRUE))
		close_dialog (editor);

	return TRUE;
}
