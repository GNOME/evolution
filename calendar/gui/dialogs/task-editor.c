/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Task editor dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
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

#include <config.h>
#include <string.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include <libgnome/gnome-i18n.h>

#include "task-page.h"
#include "task-details-page.h"
#include "recurrence-page.h"
#include "cancel-comp.h"
#include "task-editor.h"

struct _TaskEditorPrivate {
	TaskPage *task_page;
	TaskDetailsPage *task_details_page;
};



static void task_editor_class_init (TaskEditorClass *class);
static void task_editor_init (TaskEditor *te);
static void task_editor_edit_comp (CompEditor *editor, CalComponent *comp);
static void task_editor_destroy (GtkObject *object);

static void delegate_task_cmd (GtkWidget *widget, gpointer data);
static void refresh_task_cmd (GtkWidget *widget, gpointer data);
static void cancel_task_cmd (GtkWidget *widget, gpointer data);
static void forward_cmd (GtkWidget *widget, gpointer data);

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ActionDelegateTask", delegate_task_cmd),
	BONOBO_UI_UNSAFE_VERB ("ActionRefreshTask", refresh_task_cmd),
	BONOBO_UI_UNSAFE_VERB ("ActionCancelTask", cancel_task_cmd),
	BONOBO_UI_UNSAFE_VERB ("ActionForward", forward_cmd),

	BONOBO_UI_VERB_END
};

static CompEditorClass *parent_class;



/**
 * task_editor_get_type:
 *
 * Registers the #TaskEditor class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #TaskEditor class.
 **/
GtkType
task_editor_get_type (void)
{
	static GtkType task_editor_type = 0;

	if (!task_editor_type) {
		static const GtkTypeInfo task_editor_info = {
			"TaskEditor",
			sizeof (TaskEditor),
			sizeof (TaskEditorClass),
			(GtkClassInitFunc) task_editor_class_init,
			(GtkObjectInitFunc) task_editor_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		task_editor_type = gtk_type_unique (TYPE_COMP_EDITOR,
						     &task_editor_info);
	}

	return task_editor_type;
}

/* Class initialization function for the event editor */
static void
task_editor_class_init (TaskEditorClass *klass)
{
	GtkObjectClass *object_class;
	CompEditorClass *editor_class;

	object_class = (GtkObjectClass *) klass;
	editor_class = (CompEditorClass *) klass;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR);

	editor_class->edit_comp = task_editor_edit_comp;

	object_class->destroy = task_editor_destroy;
}

/* Object initialization function for the event editor */
static void
task_editor_init (TaskEditor *te)
{
	TaskEditorPrivate *priv;
	
	priv = g_new0 (TaskEditorPrivate, 1);
	te->priv = priv;

	priv->task_page = task_page_new ();
	comp_editor_append_page (COMP_EDITOR (te), 
				 COMP_EDITOR_PAGE (priv->task_page),
				 _("Task"));

	priv->task_details_page = task_details_page_new ();
	comp_editor_append_page (COMP_EDITOR (te),
				 COMP_EDITOR_PAGE (priv->task_details_page),
				 _("Details"));

	comp_editor_merge_ui (COMP_EDITOR (te), EVOLUTION_DATADIR 
			      "/gnome/ui/evolution-task-editor.xml",
			      verbs);
}

static void
task_editor_edit_comp (CompEditor *editor, CalComponent *comp)
{
	TaskEditor *te;
	TaskEditorPrivate *priv;
	GSList *attendees = NULL;
	
	te = TASK_EDITOR (editor);
	priv = te->priv;
	
	cal_component_get_attendee_list (comp, &attendees);
	if (attendees == NULL)
		task_details_page_show_delegation (priv->task_details_page, FALSE);
	else
		task_details_page_show_delegation (priv->task_details_page, TRUE);
	cal_component_free_attendee_list (attendees);

	if (parent_class->edit_comp)
		parent_class->edit_comp (editor, comp);
}

/* Destroy handler for the event editor */
static void
task_editor_destroy (GtkObject *object)
{
	TaskEditor *te;
	TaskEditorPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_EDITOR (object));

	te = TASK_EDITOR (object);
	priv = te->priv;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/**
 * task_editor_new:
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
TaskEditor *
task_editor_new (void)
{
	return TASK_EDITOR (gtk_type_new (TYPE_TASK_EDITOR));
}

static void
delegate_task_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	TaskEditorPrivate *priv;

	priv = te->priv;

	task_details_page_show_delegation (priv->task_details_page, TRUE);
	comp_editor_show_page (COMP_EDITOR (te),
			       COMP_EDITOR_PAGE (priv->task_details_page));
}

static void
refresh_task_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	
	comp_editor_send_comp (COMP_EDITOR (te), CAL_COMPONENT_METHOD_REFRESH);
}

static void
cancel_task_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	CalComponent *comp;
	
	comp = comp_editor_get_current_comp (COMP_EDITOR (te));
	if (cancel_component_dialog (comp)) {
		comp_editor_send_comp (COMP_EDITOR (te), CAL_COMPONENT_METHOD_CANCEL);
		comp_editor_delete_comp (COMP_EDITOR (te));
	}
}

static void
forward_cmd (GtkWidget *widget, gpointer data)
{
	TaskEditor *te = TASK_EDITOR (data);
	
	comp_editor_send_comp (COMP_EDITOR (te), CAL_COMPONENT_METHOD_PUBLISH);
}
