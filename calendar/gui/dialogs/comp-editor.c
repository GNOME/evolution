/* Evolution calendar - Framework for a calendar component editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <gnome.h>
#include <bonobo/bonobo-win.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-ui-util.h>
#include <gal/widgets/e-unicode.h>
#include "save-comp.h"
#include "delete-comp.h"
#include "send-comp.h"
#include "comp-editor.h"



/* Private part of the CompEditor structure */
struct _CompEditorPrivate {
	/* Client to use */
	CalClient *client;

	/* Calendar object/uid we are editing; this is an internal copy */
	CalComponent *comp;

	/* The pages we have */
	GList *pages;

	/* Toplevel window for the dialog */
	GtkWidget *window;
	BonoboUIComponent *uic;
	
	/* Notebook to hold the pages */
	GtkNotebook *notebook;

	GtkWidget *filesel;
	
	gboolean changed;
	gboolean needs_send;
};



static void comp_editor_class_init (CompEditorClass *class);
static void comp_editor_init (CompEditor *editor);
static void comp_editor_destroy (GtkObject *object);

static void real_edit_comp (CompEditor *editor, CalComponent *comp);
static void save_comp (CompEditor *editor);
static void delete_comp (CompEditor *editor);
static void close_dialog (CompEditor *editor);

static void page_changed_cb (GtkWidget *widget, gpointer data);
static void page_needs_send_cb (GtkWidget *widget, gpointer data);
static void page_summary_changed_cb (GtkWidget *widget, const char *summary, gpointer data);
static void page_dates_changed_cb (GtkWidget *widget, CompEditorPageDates *dates, gpointer data);

static void save_close_cmd (GtkWidget *widget, gpointer data);
static void save_as_cmd (GtkWidget *widget, gpointer data);
static void delete_cmd (GtkWidget *widget, gpointer data);
static void close_cmd (GtkWidget *widget, gpointer data);

static void save_clicked_cb (GtkWidget *widget, gpointer data);
static void close_clicked_cb (GtkWidget *widget, gpointer data);
static void help_clicked_cb (GtkWidget *widget, gpointer data);
static gint delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data);

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("FileSaveAndClose", save_close_cmd), 
	BONOBO_UI_UNSAFE_VERB ("FileSaveAs", save_as_cmd),
	BONOBO_UI_UNSAFE_VERB ("FileDelete", delete_cmd), 
	BONOBO_UI_UNSAFE_VERB ("FileClose", close_cmd), 
	
	BONOBO_UI_VERB_END
};

#define CLASS(page) (COMP_EDITOR_CLASS (GTK_OBJECT (page)->klass))

static GtkObjectClass *parent_class;



GtkType
comp_editor_get_type (void)
{
	static GtkType comp_editor_type = 0;

	if (!comp_editor_type) {
		static const GtkTypeInfo comp_editor_info = {
			"CompEditor",
			sizeof (CompEditor),
			sizeof (CompEditorClass),
			(GtkClassInitFunc) comp_editor_class_init,
			(GtkObjectInitFunc) comp_editor_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		comp_editor_type = gtk_type_unique (GTK_TYPE_OBJECT,
						    &comp_editor_info); 
	}

	return comp_editor_type;
}

/* Class initialization function for the calendar component editor */
static void
comp_editor_class_init (CompEditorClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	klass->edit_comp = real_edit_comp;

	object_class->destroy = comp_editor_destroy;
}

/* Creates the basic in the editor */
static void
setup_widgets (CompEditor *editor)
{
	CompEditorPrivate *priv;
	BonoboUIContainer *container;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *pixmap;
	GtkWidget *button;

	priv = editor->priv;

	/* Window and basic vbox */
	priv->window = bonobo_window_new ("event-editor", "iCalendar Editor");
	gtk_signal_connect (GTK_OBJECT (priv->window), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event_cb), editor);

	priv->uic = bonobo_ui_component_new ("comp-editor");
	container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (container, BONOBO_WINDOW (priv->window));
	bonobo_ui_component_set_container (priv->uic, BONOBO_OBJREF (container));

	bonobo_ui_component_add_verb_list_with_data (priv->uic, verbs, editor);
	bonobo_ui_util_set_ui (priv->uic, EVOLUTION_DATADIR "/gnome/gui",
			       "evolution-comp-editor.xml", "evolution-calendar");

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	bonobo_window_set_contents (BONOBO_WINDOW (priv->window), vbox);

	/* Notebook */
	priv->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (priv->notebook),
			    TRUE, TRUE, 0);

	/* Buttons */
	bbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);

	pixmap = gnome_stock_pixmap_widget (NULL, GNOME_STOCK_PIXMAP_SAVE);
	button = gnome_pixmap_button (pixmap, _("Save"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (save_clicked_cb), editor);
	
	button = gnome_stock_button (GNOME_STOCK_BUTTON_CLOSE);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (close_clicked_cb), editor);

	button = gnome_stock_button (GNOME_STOCK_BUTTON_HELP);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (help_clicked_cb), editor);
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
}

/* Destroy handler for the calendar component editor */
static void
comp_editor_destroy (GtkObject *object)
{
	CompEditor *editor;
	CompEditorPrivate *priv;

	editor = COMP_EDITOR (object);
	priv = editor->priv;

	if (priv->window) {
		gtk_widget_destroy (priv->window);
		priv->window = NULL;
	}

	g_free (priv);
	editor->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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
	
	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (page != NULL);
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (label != NULL);

	priv = editor->priv;

	/* If we are editing something, fill the widgets with current info */
	if (priv->comp != NULL) {
		CalComponent *comp;
		
		comp = comp_editor_get_current_comp (editor);
		comp_editor_page_fill_widgets (page, comp);
		gtk_object_unref (GTK_OBJECT (comp));
	}
	
	page_widget = comp_editor_page_get_widget (page);
	g_assert (page_widget != NULL);
	
	label_widget = gtk_label_new (label);

	priv->pages = g_list_append (priv->pages, page);
	gtk_notebook_append_page (priv->notebook, page_widget, label_widget);

	/* Listen for things happening on the page */
	gtk_signal_connect (GTK_OBJECT (page), "needs_send",
			    GTK_SIGNAL_FUNC (page_needs_send_cb), editor);
	gtk_signal_connect (GTK_OBJECT (page), "changed",
			    GTK_SIGNAL_FUNC (page_changed_cb), editor);
	gtk_signal_connect (GTK_OBJECT (page), "summary_changed",
			    GTK_SIGNAL_FUNC (page_summary_changed_cb), editor);
	gtk_signal_connect (GTK_OBJECT (page), "dates_changed",
			    GTK_SIGNAL_FUNC (page_dates_changed_cb), editor);

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
	gtk_notebook_remove_page (priv->notebook, page_num);
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
		gtk_object_ref (GTK_OBJECT (client));
	}
	
	if (priv->client) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->client), 
					       editor);
		gtk_object_unref (GTK_OBJECT (priv->client));
	}

	priv->client = client;

	/* Pass the client to any pages that need it. */
	for (elem = priv->pages; elem; elem = elem->next)
	  comp_editor_page_set_cal_client (elem->data, client);
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
		char *summary;
		summary = e_utf8_to_locale_string (text.value);
		title = g_strdup_printf (type_string, summary);
		g_free (summary);
	} else
		title = g_strdup_printf (type_string, _("No summary"));

	return title;
}

/* Sets the event editor's window title from a calendar component */
static void
set_title_from_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	char *title;

	priv = editor->priv;
	title = make_title_from_comp (priv->comp);
	gtk_window_set_title (GTK_WINDOW (priv->window), title);
	g_free (title);
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
real_edit_comp (CompEditor *editor, CalComponent *comp) 
{
	CompEditorPrivate *priv;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	if (priv->comp) {
		gtk_object_unref (GTK_OBJECT (priv->comp));
		priv->comp = NULL;
	}

	if (comp)
		priv->comp = cal_component_clone (comp);

	set_title_from_comp (editor);
	fill_widgets (editor);	
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

	klass = COMP_EDITOR_CLASS (GTK_OBJECT (editor)->klass);
	
	if (klass->edit_comp)
		klass->edit_comp (editor, comp);
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
	for (l = priv->pages; l != NULL; l = l->next)
		comp_editor_page_fill_component (l->data, comp);

	return comp;
}

/**
 * comp_editor_save_comp:
 * @editor: 
 * 
 * 
 **/
void
comp_editor_save_comp (CompEditor *editor)
{
	save_comp (editor);
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
void
comp_editor_send_comp (CompEditor *editor, CalComponentItipMethod method)
{
	CalComponent *comp;

	comp = comp_editor_get_current_comp (editor);

	cal_component_commit_sequence (comp);
	itip_send_comp (method, comp);

	gtk_object_unref (GTK_OBJECT (comp));
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
comp_editor_merge_ui (CompEditor *editor, const char *filename, BonoboUIVerb *verbs)
{
	CompEditorPrivate *priv;
	BonoboUIEngine *engine;
	BonoboUINode *node;
	
	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));
	
	priv = editor->priv;

	engine = bonobo_window_get_ui_engine (BONOBO_WINDOW (priv->window));
	node = bonobo_ui_util_new_ui (priv->uic, filename, "",
				      "evolution-calendar");
	g_assert (node != NULL);

	bonobo_ui_engine_xml_merge_tree (engine, "/", node, "comp-editor");
	bonobo_ui_component_add_verb_list_with_data (priv->uic, verbs, editor);
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

	gtk_widget_show_all (priv->window);
	raise_and_focus (priv->window);
}

static void
save_comp (CompEditor *editor) 
{
	CompEditorPrivate *priv;
	GList *l;
	
	priv = editor->priv;
	
	for (l = priv->pages; l != NULL; l = l->next)
		comp_editor_page_fill_component (l->data, priv->comp);

	if (priv->needs_send && send_component_dialog (priv->comp)) {
		cal_component_commit_sequence (priv->comp);
		itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, priv->comp);
	}
	
	if (!cal_client_update_object (priv->client, priv->comp))
		g_message ("save_comp (): Could not update the object!");
	else
		priv->changed = FALSE;
}

static void
delete_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	const char *uid;

	priv = editor->priv;

	cal_component_get_uid (priv->comp, &uid);
	cal_client_remove_object (priv->client, uid);
	close_dialog (editor);
}

static gboolean
prompt_to_save_changes (CompEditor *editor)
{
	CompEditorPrivate *priv;
	
	priv = editor->priv;

	if (!priv->changed)
		return TRUE;

	switch (save_component_dialog (GTK_WINDOW (priv->window))) {
	case 0: /* Save */
		/* FIXME: If an error occurs here, we should popup a dialog
		   and then return FALSE. */
		save_comp (editor);
		return TRUE;
	case 1: /* Discard */
		return TRUE;
	case 2: /* Cancel */
	default:
		return FALSE;
	}
}

/* Closes the dialog box and emits the appropriate signals */
static void
close_dialog (CompEditor *editor)
{
	CompEditorPrivate *priv;

	priv = editor->priv;

	g_assert (priv->window != NULL);

	gtk_object_destroy (GTK_OBJECT (editor));
}

/* Menu Commands */
static void
save_close_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);

	save_comp (editor);
	close_dialog (editor);
}

static void
save_as_ok (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	struct stat s;
	char *path;
	int ret = 0;
	
	priv = editor->priv;
	
	path = gtk_file_selection_get_filename (GTK_FILE_SELECTION (priv->filesel));
	
	if (stat (path, &s) == 0) {
		GtkWidget *dlg;
		GtkWidget *text;
		
		dlg = gnome_dialog_new (_("Overwrite file?"),
					GNOME_STOCK_BUTTON_YES, 
					GNOME_STOCK_BUTTON_NO,
					NULL);
		text = gtk_label_new (_("A file by that name already exists.\nOverwrite it?"));
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox), text, TRUE, TRUE, 4);
		gtk_window_set_policy (GTK_WINDOW (dlg), FALSE, TRUE, FALSE);
		gtk_widget_show (text);
		
		ret = gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
	}
	
	if (ret == 0) {
		FILE *file;
		gchar *ical_string;

		icalcomponent *top_level;
		icalcomponent *icalcomp;
		icalproperty *prop;

		top_level = icalcomponent_new (ICAL_VCALENDAR_COMPONENT);

		/* RFC 2445, section 4.7.1 */
		prop = icalproperty_new_calscale ("GREGORIAN");
		icalcomponent_add_property (top_level, prop);
		
		/* RFC 2445, section 4.7.3 */
		prop = icalproperty_new_prodid ("-//Ximian//NONSGML Evolution Calendar//EN");
		icalcomponent_add_property (top_level, prop);
		
		/* RFC 2445, section 4.7.4.  This is the iCalendar spec version, *NOT*
		 * the product version!  Do not change this!
		 */
		prop = icalproperty_new_version ("2.0");
		icalcomponent_add_property (top_level, prop);
				
		icalcomp = cal_component_get_icalcomponent (priv->comp);
		g_assert (icalcomp != NULL);

		icalcomponent_add_component (top_level, icalcomp);
		
		ical_string = icalcomponent_as_ical_string (top_level);

		file = fopen (path, "w");
		if (file == NULL) {			
			g_warning ("Couldn't save item");
			gtk_main_quit ();
			return;
		}
		
		fprintf (file, ical_string);
		fclose (file);

		gtk_main_quit ();
	}
}

static void
save_as_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	GtkFileSelection *fs;
	char *path;
	
	priv = editor->priv;
	
	fs = GTK_FILE_SELECTION (gtk_file_selection_new (_("Save As...")));
	path = g_strdup_printf ("%s/", g_get_home_dir ());
	gtk_file_selection_set_filename (fs, path);
	g_free (path);

	gtk_signal_connect (GTK_OBJECT (fs->ok_button), "clicked", 
			    GTK_SIGNAL_FUNC (save_as_ok), editor);
	gtk_signal_connect (GTK_OBJECT (fs->cancel_button), "clicked", 
			    GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
	
	priv->filesel = GTK_WIDGET (fs);
	gtk_widget_show (priv->filesel);
	gtk_grab_add (priv->filesel);
	gtk_main ();

	gtk_widget_destroy (priv->filesel);
	priv->filesel = NULL;
}

static void
delete_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	CalComponentVType vtype;

	priv = editor->priv;
	
	vtype = cal_component_get_vtype (priv->comp);

	if (delete_component_dialog (priv->comp, 1, vtype, priv->window))
		delete_comp (editor);
}

static void
close_cmd (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);

	if (prompt_to_save_changes (editor))
		close_dialog (editor);
}

static void
save_clicked_cb (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);

	save_comp (editor);
	close_dialog (editor);
}

static void
close_clicked_cb (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);

	if (prompt_to_save_changes (editor))
		close_dialog (editor);
}

/* Button callbacks */
static void
help_clicked_cb (GtkWidget *widget, gpointer data)
{
}

static void
page_changed_cb (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	
	priv = editor->priv;
	
	priv->changed = TRUE;
}

static void
page_needs_send_cb (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	
	priv = editor->priv;
	
	priv->needs_send = TRUE;	
}

/* Page signal callbacks */
static void
page_summary_changed_cb (GtkWidget *widget, const char *summary, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	GList *l;
	
	priv = editor->priv;
	
	for (l = priv->pages; l != NULL; l = l->next)
		comp_editor_page_set_summary (l->data, summary);
	
	priv->changed = TRUE;
}

static void
page_dates_changed_cb (GtkWidget *widget,
		       CompEditorPageDates *dates,
		       gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	GList *l;
	
	priv = editor->priv;

	for (l = priv->pages; l != NULL; l = l->next)
		comp_editor_page_set_dates (l->data, dates);

	priv->changed = TRUE;
}

static gint
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);

	if (prompt_to_save_changes (editor))
		close_dialog (editor);

	return TRUE;
}
