/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * TaskEditor - a GtkObject which handles a libglade-loaded dialog to edit
 * tasks.
 */

#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include <gal/util/e-util.h>
#include <e-util/e-dialog-widgets.h>
#include <widgets/misc/e-dateedit.h>
#include <cal-util/timeutil.h>
#include <cal-client/cal-client.h>
#include "task-editor.h"


typedef struct {
	/* Glade XML data */
	GladeXML *xml;

	/* UI handler */
	BonoboUIComponent *uic;

	/* Client to use */
	CalClient *client;
	
	/* Calendar component we are editing; this is an internal copy and is
	 * not one of the read-only objects from the parent calendar.
	 */
	CalComponent *comp;


	/* This is TRUE while we are setting the widget values. We just return
	   from any signal handlers. */
	gboolean ignore_callbacks;

	/* Widgets from the Glade file */

	GtkWidget *app;

	GtkWidget *summary;

	GtkWidget *due_date;
	GtkWidget *start_date;

	GtkWidget *percent_complete;

	GtkWidget *status;
	GtkWidget *priority;
	GtkWidget *classification;

	GtkWidget *description;

	GtkWidget *contacts;
	GtkWidget *categories;

	GtkWidget *completed_date;
	GtkWidget *url;
} TaskEditorPrivate;


/* Note that these two arrays must match. */
static const int status_map[] = {
	ICAL_STATUS_NEEDSACTION,
	ICAL_STATUS_INPROCESS,
	ICAL_STATUS_COMPLETED,
	ICAL_STATUS_CANCELLED,
	-1
};

static const char* status_string_map[] = {
	"NEEDS-ACTION",
	"IN-PROCESS",
	"COMPLETED",
	"CANCELLED",
	NULL
};


typedef enum {
	PRIORITY_HIGH,
	PRIORITY_NORMAL,
	PRIORITY_LOW,
	PRIORITY_UNDEFINED,
} TaskEditorPriority;

static const int priority_map[] = {
	PRIORITY_HIGH,
	PRIORITY_NORMAL,
	PRIORITY_LOW,
	PRIORITY_UNDEFINED,
	-1
};

static const int classification_map[] = {
	CAL_COMPONENT_CLASS_NONE,
	CAL_COMPONENT_CLASS_PUBLIC,
	CAL_COMPONENT_CLASS_PRIVATE,
	CAL_COMPONENT_CLASS_CONFIDENTIAL,
	-1
};


static void task_editor_class_init (TaskEditorClass *class);
static void task_editor_init (TaskEditor *tedit);
static gint app_delete_event_cb (GtkWidget *widget,
				 GdkEvent *event,
				 gpointer data);
static void close_dialog (TaskEditor *tedit);
static gboolean get_widgets (TaskEditor *tedit);
static void init_widgets (TaskEditor *tedit);
static void task_editor_destroy (GtkObject *object);
static char * make_title_from_comp (CalComponent *comp);
static void clear_widgets (TaskEditor *tedit);
static void fill_widgets (TaskEditor *tedit);

static void file_save_cb (GtkWidget *widget, gpointer data);
static void file_save_and_close_cb (GtkWidget *widget, gpointer data);
static void file_delete_cb (GtkWidget *widget, gpointer data);
static void file_close_cb (GtkWidget *widget, gpointer data);

static void debug_xml_cb (GtkWidget *widget, gpointer data);

static void save_todo_object (TaskEditor *tedit);
static void dialog_to_comp_object (TaskEditor *tedit);

static void obj_updated_cb (CalClient *client, const char *uid, gpointer data);
static void obj_removed_cb (CalClient *client, const char *uid, gpointer data);
static void raise_and_focus (GtkWidget *widget);

static TaskEditorPriority priority_value_to_index (int priority_value);
static int priority_index_to_value (TaskEditorPriority priority);

static int status_string_to_value	(const char *status_string);
static const char* status_value_to_string	(int status);

static void completed_date_changed	(EDateEdit	*dedit,
					 TaskEditor	*tedit);
static void status_changed		(GtkMenu	*menu,
					 TaskEditor	*tedit);
static void percent_complete_changed	(GtkAdjustment	*adj,
					 TaskEditor	*tedit);

GtkWidget * task_editor_create_date_edit (void);

static GtkObjectClass *parent_class;

E_MAKE_TYPE(task_editor, "TaskEditor", TaskEditor,
	    task_editor_class_init, task_editor_init, GTK_TYPE_OBJECT)


static void
task_editor_class_init (TaskEditorClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = task_editor_destroy;
}


static void
task_editor_init (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;

	priv = g_new0 (TaskEditorPrivate, 1);
	tedit->priv = priv;

	priv->ignore_callbacks = FALSE;
}


/**
 * task_editor_new:
 * @Returns: a new #TaskEditor.
 *
 * Creates a new #TaskEditor.
 **/
TaskEditor *
task_editor_new (void)
{
	TaskEditor *tedit;

	tedit = TASK_EDITOR (gtk_type_new (task_editor_get_type ()));
	return task_editor_construct (tedit);
}

static BonoboUIVerb verbs [] = {

	BONOBO_UI_UNSAFE_VERB ("FileSave", file_save_cb),
	BONOBO_UI_UNSAFE_VERB ("FileDelete", file_delete_cb),
	BONOBO_UI_UNSAFE_VERB ("FileClose", file_close_cb),
	BONOBO_UI_UNSAFE_VERB ("FileSaveAndClose", file_save_and_close_cb),

	BONOBO_UI_UNSAFE_VERB ("DebugDumpXml", debug_xml_cb),

	BONOBO_UI_VERB_END
};

/**
 * task_editor_construct:
 * @tedit: A #TaskEditor.
 * 
 * Constructs a task editor by loading its Glade XML file.
 * 
 * Return value: The same object as @tedit, or NULL if the widgets could not be
 * created.  In the latter case, the task editor will automatically be
 * destroyed.
 **/
TaskEditor *
task_editor_construct (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;
	GtkWidget         *bonobo_win;

	g_return_val_if_fail (tedit != NULL, NULL);
	g_return_val_if_fail (IS_TASK_EDITOR (tedit), NULL);

	priv = tedit->priv;

	/* Load the content widgets */

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/task-editor-dialog.glade", NULL);
	if (!priv->xml) {
		g_message ("task_editor_construct(): Could not load the Glade XML file!");
		goto error;
	}

	if (!get_widgets (tedit)) {
		g_message ("task_editor_construct(): Could not find all widgets in the XML file!");
		goto error;
	}

	init_widgets (tedit);

	/* Construct the app */

	priv->uic = bonobo_ui_component_new ("task-editor-dialog");
	if (!priv->uic) {
		g_message ("task_editor_construct(): Could not create the UI component");
		goto error;
	}

	bonobo_win = bonobo_win_new ("event-editor-dialog", "Event Editor");

	/* FIXME: The sucking bit */
	{
		GtkWidget *contents;

		contents = gnome_dock_get_client_area (
			GNOME_DOCK (GNOME_APP (priv->app)->dock));
		if (!contents) {
			g_message ("event_editor_construct(): Could not get contents");
			goto error;
		}
		gtk_widget_ref (contents);
		gtk_container_remove (GTK_CONTAINER (contents->parent), contents);
		bonobo_win_set_contents (BONOBO_WIN (bonobo_win), contents);
		gtk_widget_destroy (priv->app);
		priv->app = GTK_WIDGET (bonobo_win);
	}

	{
		BonoboUIContainer *container = bonobo_ui_container_new ();
		bonobo_ui_container_set_win (container, BONOBO_WIN (priv->app));
		bonobo_ui_component_set_container (
			priv->uic, bonobo_object_corba_objref (BONOBO_OBJECT (container)));
	}

	bonobo_ui_component_add_verb_list_with_data (
		priv->uic, verbs, tedit);

	bonobo_ui_util_set_ui (priv->uic, EVOLUTION_DATADIR,
			       "evolution-task-editor-dialog.xml",
			       "evolution-task-editor");

	/* Hook to destruction of the dialog */
	gtk_signal_connect (GTK_OBJECT (priv->app), "delete_event",
			    GTK_SIGNAL_FUNC (app_delete_event_cb), tedit);

	/* Show the dialog */
	gtk_widget_show (priv->app);

	return tedit;

 error:

	gtk_object_unref (GTK_OBJECT (tedit));
	return NULL;
}


/* Called by libglade to create our custom EDateEdit widgets. */
GtkWidget *
task_editor_create_date_edit (void)
{
	GtkWidget *dedit;

	dedit = e_date_edit_new ();
	/* FIXME: Set other options. */
	e_date_edit_set_time_popup_range (E_DATE_EDIT (dedit), 8, 18);
	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (dedit), TRUE);
	return dedit;
}


/* Callback used when the dialog box is destroyed */
static gint
app_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	TaskEditor *tedit;

	/* FIXME: need to check for a dirty object */

	tedit = TASK_EDITOR (data);
	close_dialog (tedit);

	return TRUE;
}


/* Closes the dialog box and emits the appropriate signals */
static void
close_dialog (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;

	priv = tedit->priv;

	g_assert (priv->app != NULL);

	gtk_object_destroy (GTK_OBJECT (tedit));
}


/* Gets the widgets from the XML file and returns if they are all available.
 * For the widgets whose values can be simply set with e-dialog-utils, it does
 * that as well.
 */
static gboolean
get_widgets (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;

	priv = tedit->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->app = GW ("task-editor-dialog");

	priv->summary = GW ("summary");

	priv->due_date = GW ("due-date");
	priv->start_date = GW ("start-date");

	priv->percent_complete = GW ("percent-complete");

	priv->status = GW ("status");
	priv->priority = GW ("priority");
	priv->classification = GW ("classification");

	priv->description = GW ("description");

	priv->contacts = GW ("contacts");
	priv->categories = GW ("categories");

	priv->completed_date = GW ("completed-date");
	priv->url = GW ("url");

#undef GW

	return (priv->app
		&& priv->summary
		&& priv->due_date
		&& priv->start_date
		&& priv->percent_complete
		&& priv->status
		&& priv->priority
		&& priv->classification
		&& priv->description
		&& priv->contacts
		&& priv->categories
		&& priv->completed_date
		&& priv->url);
}


/* Hooks the widget signals */
static void
init_widgets (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;

	priv = tedit->priv;

	/* Connect signals. The Status, Percent Complete & Date Completed
	   properties are closely related so whenever one changes we may need
	   to update the other 2. */
	gtk_signal_connect (GTK_OBJECT (priv->completed_date), "date_changed",
			    GTK_SIGNAL_FUNC (completed_date_changed), tedit);
	gtk_signal_connect (GTK_OBJECT (priv->completed_date), "time_changed",
			    GTK_SIGNAL_FUNC (completed_date_changed), tedit);

	gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (priv->status)->menu),
			    "deactivate",
			    GTK_SIGNAL_FUNC (status_changed), tedit);

	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (priv->percent_complete)->adjustment),
			    "value_changed",
			    GTK_SIGNAL_FUNC (percent_complete_changed), tedit);
}

static void
task_editor_destroy (GtkObject *object)
{
	TaskEditor *tedit;
	TaskEditorPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_EDITOR (object));

	tedit = TASK_EDITOR (object);
	priv = tedit->priv;

	if (priv->uic) {
		bonobo_object_unref (BONOBO_OBJECT (priv->uic));
		priv->uic = NULL;
	}

	if (priv->app) {
		gtk_widget_destroy (priv->app);
		priv->app = NULL;
	}

	if (priv->comp) {
		gtk_object_unref (GTK_OBJECT (priv->comp));
		priv->comp = NULL;
	}

	if (priv->client) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->client),
					       tedit);
		gtk_object_unref (GTK_OBJECT (priv->client));
		priv->client = NULL;
	}

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	g_free (priv);
	tedit->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


void 
task_editor_set_cal_client (TaskEditor *tedit,
			    CalClient *client)
{
	TaskEditorPrivate *priv;

	g_return_if_fail (tedit != NULL);
	g_return_if_fail (IS_TASK_EDITOR (tedit));

	priv = tedit->priv;

	if (client == priv->client)
		return;

	if (client)
		g_return_if_fail (IS_CAL_CLIENT (client));

	if (client)
		g_return_if_fail (cal_client_is_loaded (client));	
	
	if (client)
		gtk_object_ref (GTK_OBJECT (client));

	if (priv->client) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->client),
					       tedit);
		gtk_object_unref (GTK_OBJECT (priv->client));
	}

	priv->client = client;

	if (priv->client) {
		gtk_signal_connect (GTK_OBJECT (priv->client), "obj_updated",
				    GTK_SIGNAL_FUNC (obj_updated_cb), tedit);
		gtk_signal_connect (GTK_OBJECT (priv->client), "obj_removed",
				    GTK_SIGNAL_FUNC (obj_removed_cb), tedit);
	}
}


/* Callback used when the calendar client tells us that an object changed */
static void
obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	TaskEditor *tedit;
	TaskEditorPrivate *priv;
	CalComponent *comp;
	CalClientGetStatus status;
	const gchar *editing_uid;

	tedit = TASK_EDITOR (data);

	g_return_if_fail (IS_TASK_EDITOR (tedit));

	priv = tedit->priv;
	
	/* If we aren't showing the object which has been updated, return. */
	if (!priv->comp)
	  return;
	cal_component_get_uid (priv->comp, &editing_uid);
	if (strcmp (uid, editing_uid))
	  return;


	/* Get the task from the server. */
	status = cal_client_get_object (priv->client, uid, &comp);

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		/* Everything is fine */
		break;

	case CAL_CLIENT_GET_SYNTAX_ERROR:
		g_message ("obj_updated_cb(): Syntax error when getting object `%s'", uid);
		return;

	case CAL_CLIENT_GET_NOT_FOUND:
		/* The object is no longer in the server, so do nothing */
		return;

	default:
		g_assert_not_reached ();
		return;
	}

	raise_and_focus (priv->app);
}

/* Callback used when the calendar client tells us that an object was removed */
static void
obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	TaskEditor *tedit;
	TaskEditorPrivate *priv;
	const gchar *editing_uid;

	tedit = TASK_EDITOR (data);

	g_return_if_fail (tedit != NULL);
	g_return_if_fail (IS_TASK_EDITOR (tedit));

	priv = tedit->priv;

	/* If we aren't showing the object which has been updated, return. */
	if (!priv->comp)
	  return;
	cal_component_get_uid (priv->comp, &editing_uid);
	if (strcmp (uid, editing_uid))
	  return;


	raise_and_focus (priv->app);
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
 * task_editor_set_todo_object:
 * @tedit: A #TaskEditor.
 * @comp: A todo object.
 * 
 * Sets the todo object that a task editor dialog will manipulate.
 **/
void
task_editor_set_todo_object	(TaskEditor	*tedit,
				 CalComponent	*comp)
{
	TaskEditorPrivate *priv;
	char *title;

	g_return_if_fail (tedit != NULL);
	g_return_if_fail (IS_TASK_EDITOR (tedit));

	priv = tedit->priv;

	if (priv->comp) {
		gtk_object_unref (GTK_OBJECT (priv->comp));
		priv->comp = NULL;
	}

	if (comp)
		priv->comp = cal_component_clone (comp);

	title = make_title_from_comp (priv->comp);
	gtk_window_set_title (GTK_WINDOW (priv->app), title);
	g_free (title);

	fill_widgets (tedit);
}


/* Creates an appropriate title for the task editor dialog */
static char *
make_title_from_comp (CalComponent *comp)
{
	const char *summary;
	CalComponentVType type;
	CalComponentText text;
	
	if (!comp)
		return g_strdup (_("Edit Task"));

	cal_component_get_summary (comp, &text);
	if (text.value)
		summary = text.value;
	else
		summary =  _("No summary");

	
	type = cal_component_get_vtype (comp);
	switch (type) {
	case CAL_COMPONENT_EVENT:
		return g_strdup_printf (_("Appointment - %s"), summary);

	case CAL_COMPONENT_TODO:
		return g_strdup_printf (_("Task - %s"), summary);

	case CAL_COMPONENT_JOURNAL:
		return g_strdup_printf (_("Journal entry - %s"), summary);

	default:
		g_message ("make_title_from_comp(): Cannot handle object of type %d", type);
		return NULL;
	}
}


/* Fills the widgets with default values */
static void
clear_widgets (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;

	priv = tedit->priv;


}

/* Fills in the widgets with the proper values */
static void
fill_widgets (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;
	CalComponentText text;
	CalComponentDateTime d;
	struct icaltimetype *completed;
	CalComponentClassification classification;
	GSList *l;
	time_t t;
	int *priority_value, *percent;
	icalproperty_status status;
	TaskEditorPriority priority;
	const char *url, *status_string;

	priv = tedit->priv;

	clear_widgets (tedit);

	if (!priv->comp)
		return;

	/* We want to ignore any signals emitted while changing fields. */
	priv->ignore_callbacks = TRUE;


	cal_component_get_summary (priv->comp, &text);
	e_dialog_editable_set (priv->summary, text.value);

	cal_component_get_description_list (priv->comp, &l);
	if (l) {
		text = *(CalComponentText *)l->data;
		e_dialog_editable_set (priv->description, text.value);
	} else {
		e_dialog_editable_set (priv->description, NULL);
	}
	cal_component_free_text_list (l);
	
	/* Due Date. */
	cal_component_get_due (priv->comp, &d);
	if (d.value) {
		t = icaltime_as_timet (*d.value);
	} else {
		t = -1;
	}
	e_date_edit_set_time (E_DATE_EDIT (priv->due_date), t);

	/* Start Date. */
	cal_component_get_dtstart (priv->comp, &d);
	if (d.value) {
		t = icaltime_as_timet (*d.value);
	} else {
		t = -1;
	}
	e_date_edit_set_time (E_DATE_EDIT (priv->start_date), t);

	/* Completed Date. */
	cal_component_get_completed (priv->comp, &completed);
	if (completed) {
		t = icaltime_as_timet (*completed);
		cal_component_free_icaltimetype (completed);
	} else {
		t = -1;
	}
	e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), t);

	/* Percent Complete. */
	cal_component_get_percent (priv->comp, &percent);
	if (percent) {
		e_dialog_spin_set (priv->percent_complete, *percent);
		cal_component_free_percent (percent);
	} else {
		/* FIXME: Could check if task is completed and set 100%. */
		e_dialog_spin_set (priv->percent_complete, 0);
	}

	/* Status. */
	cal_component_get_status (priv->comp, &status_string);
	if (status_string) {
		status = status_string_to_value (status_string);
	} else {
		/* Try to user the percent value. */
		if (percent) {
			if (*percent == 0)
				status = ICAL_STATUS_NEEDSACTION;
			else if (*percent == 100)
				status = ICAL_STATUS_COMPLETED;
			else
				status = ICAL_STATUS_INPROCESS;
		} else {
			status = ICAL_STATUS_NEEDSACTION;
		}
	}
	g_print ("Setting status\n");
	e_dialog_option_menu_set (priv->status, status, status_map);

	/* Priority. */
	cal_component_get_priority (priv->comp, &priority_value);
	if (priority_value) {
		priority = priority_value_to_index (*priority_value);
		cal_component_free_priority (priority_value);
	} else {
		priority = PRIORITY_UNDEFINED;
	}
	g_print ("Setting priority\n");
	e_dialog_option_menu_set (priv->priority, priority, priority_map);


	/* Classification. */
	cal_component_get_classification (priv->comp, &classification);
	g_print ("Setting classification\n");
	e_dialog_option_menu_set (priv->classification, classification,
				  classification_map);


	/* URL. */
	cal_component_get_url (priv->comp, &url);
	e_dialog_editable_set (priv->url, url);

	priv->ignore_callbacks = FALSE;
}


static void
save_todo_object (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;
	char *title;

	priv = tedit->priv;

	g_return_if_fail (priv->client != NULL);

	if (!priv->comp)
		return;

	dialog_to_comp_object (tedit);

	title = make_title_from_comp (priv->comp);
	gtk_window_set_title (GTK_WINDOW (priv->app), title);
	g_free (title);

	if (!cal_client_update_object (priv->client, priv->comp))
		g_message ("save_todo_object(): Could not update the object!");
}


/* Get the values of the widgets in the event editor and put them in the iCalObject */
static void
dialog_to_comp_object (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;
	CalComponent *comp;
	CalComponentText *text;
	CalComponentDateTime date;
	time_t t;
	GSList *list;
	icalproperty_status status;
	TaskEditorPriority priority;
	int priority_value, percent;
	CalComponentClassification classification;
	char *url;
	const char *status_string;
	
	priv = tedit->priv;
	comp = priv->comp;

	/* Summary. */
	text = g_new0 (CalComponentText, 1);
	text->value = e_dialog_editable_get (priv->summary);
	cal_component_set_summary (comp, text);

	/* Description. Note that we use the text variable again, and it is
	   freed in cal_component_free_text_list(). */
	list = NULL;
	text->value  = e_dialog_editable_get (priv->description);
	list = g_slist_prepend (list, text);
	cal_component_set_description_list (comp, list);
	cal_component_free_text_list (list);
	

	date.value = g_new (struct icaltimetype, 1);
	date.tzid = NULL;

	/* Due Date. */
	t = e_date_edit_get_time (E_DATE_EDIT (priv->due_date));
	if (t != -1) {
		*date.value = icaltime_from_timet (t, FALSE, FALSE);
		cal_component_set_due (comp, &date);
	} else {
		cal_component_set_due (comp, NULL);
	}

	/* Start Date. */
	t = e_date_edit_get_time (E_DATE_EDIT (priv->start_date));
	if (t != -1) {
		*date.value = icaltime_from_timet (t, FALSE, FALSE);
		cal_component_set_dtstart (comp, &date);
	} else {
		cal_component_set_dtstart (comp, NULL);
	}

	/* Completed Date. */
	t = e_date_edit_get_time (E_DATE_EDIT (priv->completed_date));
	if (t != -1) {
		*date.value = icaltime_from_timet (t, FALSE, FALSE);
		cal_component_set_completed (comp, date.value);
	} else {
		cal_component_set_completed (comp, NULL);
	}

	g_free (date.value);

	/* Percent Complete. */
	percent = e_dialog_spin_get_int (priv->percent_complete);
	cal_component_set_percent (comp, &percent);

	/* Status. */
	status = e_dialog_option_menu_get (priv->status, status_map);
	status_string = status_value_to_string (status);
	cal_component_set_status (comp, status_string);

	/* Priority. */
	priority = e_dialog_option_menu_get (priv->priority, priority_map);
	priority_value = priority_index_to_value (priority);
	cal_component_set_priority (comp, &priority_value);

	/* Classification. */
	classification = e_dialog_option_menu_get (priv->classification,
						   classification_map);
	cal_component_set_classification (comp, classification);


	/* URL. */
	url = e_dialog_editable_get (priv->url);
	cal_component_set_url (comp, url);




	cal_component_commit_sequence (comp);
}

static void
debug_xml_cb (GtkWidget *widget, gpointer data)
{
	TaskEditor *tedit = TASK_EDITOR (data);
	TaskEditorPrivate *priv = tedit->priv;
	
	bonobo_win_dump (BONOBO_WIN (priv->app), "on demand");
}

/* File/Save callback */
static void
file_save_cb (GtkWidget *widget, gpointer data)
{
	TaskEditor *tedit;

	tedit = TASK_EDITOR (data);
	save_todo_object (tedit);
}

/* File/Save and Close callback */
static void
file_save_and_close_cb (GtkWidget *widget, gpointer data)
{
	TaskEditor *tedit;

	tedit = TASK_EDITOR (data);
	save_todo_object (tedit);
	close_dialog (tedit);
}

/* File/Delete callback */
static void
file_delete_cb (GtkWidget *widget, gpointer data)
{
	TaskEditor *tedit;
	TaskEditorPrivate *priv;
	const char *uid;
	
	tedit = TASK_EDITOR (data);

	g_return_if_fail (IS_TASK_EDITOR (tedit));

	priv = tedit->priv;
	
	g_return_if_fail (priv->comp);

	cal_component_get_uid (priv->comp, &uid);

	/* We don't check the return value; FALSE can mean the object was not in
	 * the server anyways.
	 */
	cal_client_remove_object (priv->client, uid);

	close_dialog (tedit);
}

/* File/Close callback */
static void
file_close_cb (GtkWidget *widget, gpointer data)
{
	TaskEditor *tedit;

	tedit = TASK_EDITOR (data);

	g_return_if_fail (IS_TASK_EDITOR (tedit));

	close_dialog (tedit);
}


static TaskEditorPriority
priority_value_to_index (int priority_value)
{
	TaskEditorPriority retval;

	if (priority_value == 0)
		retval = PRIORITY_UNDEFINED;
	else if (priority_value <= 4)
		retval = PRIORITY_HIGH;
	else if (priority_value == 5)
		retval = PRIORITY_NORMAL;
	else 
		retval = PRIORITY_LOW;

	return retval;
}


static int
priority_index_to_value (TaskEditorPriority priority)
{
	int retval;

	switch (priority) {
	case PRIORITY_UNDEFINED:
		retval = 0;
		break;
	case PRIORITY_HIGH:
		retval = 3;
		break;
	case PRIORITY_NORMAL:
		retval = 5;
		break;
	case PRIORITY_LOW:
		retval = 7;
		break;
	default:
		retval = -1;
		g_assert_not_reached ();
		break;
	}

	return retval;
}


static int
status_string_to_value	(const char *status_string)
{
	int i;

	for (i = 0; status_map[i] != -1; i++) {
		if (!strcmp (status_string_map[i], status_string))
			return status_map[i];
	}

	g_warning ("Invalid todo status string");
	return ICAL_STATUS_NEEDSACTION;
}


static const char*
status_value_to_string	(int status)
{
	int i;

	for (i = 0; status_map[i] != -1; i++) {
		if (status_map[i] == status)
			return status_string_map[i];
	}

	g_warning ("Invalid todo status value");
	return NULL;
}


static void
completed_date_changed	(EDateEdit	*dedit,
			 TaskEditor	*tedit)
{
	TaskEditorPrivate *priv;
	time_t t;

	g_return_if_fail (IS_TASK_EDITOR (tedit));

	priv = tedit->priv;

	if (priv->ignore_callbacks)
		return;

	t = e_date_edit_get_time (E_DATE_EDIT (priv->completed_date));
	priv->ignore_callbacks = TRUE;
	if (t == -1) {
		/* If the 'Completed Date' is set to 'None', we set the
		   status to 'Not Started' and the percent-complete to 0.
		   The task may actually be partially-complete, but we leave
		   it to the user to set those fields. */
		e_dialog_option_menu_set (priv->status, ICAL_STATUS_NEEDSACTION,
					  status_map);
		e_dialog_spin_set (priv->percent_complete, 0);
	} else {
		e_dialog_option_menu_set (priv->status, ICAL_STATUS_COMPLETED,
					  status_map);
		e_dialog_spin_set (priv->percent_complete, 100);
	}
	priv->ignore_callbacks = FALSE;
}


static void
status_changed		(GtkMenu	*menu,
			 TaskEditor	*tedit)
{
	TaskEditorPrivate *priv;
	icalproperty_status status;

	g_return_if_fail (IS_TASK_EDITOR (tedit));

	priv = tedit->priv;

	if (priv->ignore_callbacks)
		return;

	status = e_dialog_option_menu_get (priv->status, status_map);
	priv->ignore_callbacks = TRUE;
	if (status == ICAL_STATUS_NEEDSACTION) {
		e_dialog_spin_set (priv->percent_complete, 0);
		e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), -1);
	} else if (status == ICAL_STATUS_COMPLETED) {
		e_dialog_spin_set (priv->percent_complete, 100);
		e_date_edit_set_time (E_DATE_EDIT (priv->completed_date),
				      time (NULL));
	}
	priv->ignore_callbacks = FALSE;
}


static void
percent_complete_changed	(GtkAdjustment	*adj,
				 TaskEditor	*tedit)
{
	TaskEditorPrivate *priv;
	gint percent;
	icalproperty_status status;
	time_t date_completed;

	g_return_if_fail (IS_TASK_EDITOR (tedit));

	priv = tedit->priv;

	if (priv->ignore_callbacks)
		return;

	percent = e_dialog_spin_get_int (priv->percent_complete);
	priv->ignore_callbacks = TRUE;

	if (percent == 100) {
		date_completed = time (NULL);
		status = ICAL_STATUS_COMPLETED;
	} else {
		/* FIXME: Set to 'None'. */
		date_completed = time (NULL);

		if (percent == 0)
			status = ICAL_STATUS_NEEDSACTION;
		else
			status = ICAL_STATUS_INPROCESS;
	}

	e_date_edit_set_time (E_DATE_EDIT (priv->completed_date),
			      date_completed);
	e_dialog_option_menu_set (priv->status, status, status_map);

	priv->ignore_callbacks = FALSE;
}

