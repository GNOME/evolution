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

#include <gnome.h>
#include <gal/widgets/e-unicode.h>
#include "save-comp.h"
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

	/* Notebook to hold the pages */
	GtkNotebook *notebook;

	gboolean changed;
};



static void comp_editor_class_init (CompEditorClass *class);
static void comp_editor_init (CompEditor *editor);
static void comp_editor_destroy (GtkObject *object);

static void page_summary_changed_cb (GtkWidget *widget, const char *summary, gpointer data);
static void page_dates_changed_cb (GtkWidget *widget, CompEditorPageDates *dates, gpointer data);
static void page_changed_cb (GtkWidget *widget, gpointer data);

static void save_clicked_cb (GtkWidget *widget, gpointer data);
static void close_clicked_cb (GtkWidget *widget, gpointer data);
static void help_clicked_cb (GtkWidget *widget, gpointer data);
static gint delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data);

static GtkObjectClass *parent_class;



GtkType
comp_editor_get_type (void)
{
	static GtkType comp_editor_type = 0;

	if (!comp_editor_type) {
		GtkTypeInfo comp_editor_info = {
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
comp_editor_class_init (CompEditorClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = comp_editor_destroy;
}

/* Creates the basic in the editor */
static void
setup_widgets (CompEditor *editor)
{
	CompEditorPrivate *priv;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *pixmap;
	GtkWidget *button;

	priv = editor->priv;

	/* Window and basic vbox */

	priv->window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_signal_connect (GTK_OBJECT (priv->window), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event_cb), editor);

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (priv->window), vbox);

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
 * @page: Top level widget of the page
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

	/* Only allow adding the pages while a component has not been set */
	g_return_if_fail (priv->comp == NULL);

	page_widget = comp_editor_page_get_widget (page);
	g_assert (page_widget != NULL);
	
	label_widget = gtk_label_new (label);

	priv->pages = g_list_append (priv->pages, page);
	gtk_notebook_append_page (priv->notebook, page_widget, label_widget);

	/* Listen for things happening on the page */
	gtk_signal_connect (GTK_OBJECT (page), "summary_changed",
			    GTK_SIGNAL_FUNC (page_summary_changed_cb), editor);
	gtk_signal_connect (GTK_OBJECT (page), "dates_changed",
			    GTK_SIGNAL_FUNC (page_dates_changed_cb), editor);
	gtk_signal_connect (GTK_OBJECT (page), "changed",
			    GTK_SIGNAL_FUNC (page_changed_cb), editor);
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

	if (!cal_client_update_object (priv->client, priv->comp))
		g_message ("save_comp (): Could not update the object!");
	else
		priv->changed = FALSE;
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
		break;
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

static void
help_clicked_cb (GtkWidget *widget, gpointer data)
{
}

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


static void
page_changed_cb (GtkWidget *widget, gpointer data)
{
	CompEditor *editor = COMP_EDITOR (data);
	CompEditorPrivate *priv;
	
	priv = editor->priv;
	
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
