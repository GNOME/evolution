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
#include <e-util/e-util.h>
#include <cal-client/cal-client.h>
#include "task-editor.h"


typedef struct {
	/* Glade XML data */
	GladeXML *xml;

	/* UI handler */
	BonoboUIHandler *uih;

	/* Calendar object we are editing; this is an internal copy and is not
	 * one of the read-only objects from the parent calendar.
	 */
	iCalObject *ico;

	/* Widgets from the Glade file */

	GtkWidget *app;

} TaskEditorPrivate;


static void task_editor_class_init (TaskEditorClass *class);
static void task_editor_init (TaskEditor *tedit);
TaskEditor * task_editor_construct (TaskEditor *tedit);
static gboolean get_widgets (TaskEditor *tedit);
static void init_widgets (TaskEditor *tedit);
static void create_menu (TaskEditor *tedit);
static void create_toolbar (TaskEditor *tedit);
static void task_editor_destroy (GtkObject *object);

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

	priv->uih = bonobo_ui_handler_new ();
	if (!priv->uih) {
		g_message ("task_editor_construct(): Could not create the UI handler");
		goto error;
	}

	bonobo_ui_handler_set_app (priv->uih, GNOME_APP (priv->app));

	create_menu (tedit);
	create_toolbar (tedit);

	/* Hook to destruction of the dialog */

#if 0
	gtk_signal_connect (GTK_OBJECT (priv->app), "delete_event",
			    GTK_SIGNAL_FUNC (app_delete_event_cb), tedit);
#endif

	/* Show the dialog */

	gtk_widget_show (priv->app);

	return tedit;

 error:

	gtk_object_unref (GTK_OBJECT (tedit));
	return NULL;
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

	return TRUE;
}


/* Hooks the widget signals */
static void
init_widgets (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;

	priv = tedit->priv;

}


/* Menu bar */

static GnomeUIInfo file_new_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Task"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Task _Request"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Mail Message"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Appointment"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Meeting Re_quest"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Contact"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Task"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Task _Request"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Journal Entry"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Note"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Ch_oose Form..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo file_page_setup_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Memo Style"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Define Print _Styles..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_NEW_SUBTREE (file_new_menu),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: S_end"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_SAVE_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_SAVE_AS_ITEM (NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Save Attac_hments..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Delete"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Move to Folder..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Cop_y to Folder..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("Page Set_up"), file_page_setup_menu),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Print Pre_view"), NULL, NULL),
	GNOMEUIINFO_MENU_PRINT_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_PROPERTIES_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM (NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo edit_object_menu[] = {
	GNOMEUIINFO_ITEM_NONE ("FIXME: what goes here?", NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo edit_menu[] = {
	GNOMEUIINFO_MENU_UNDO_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_REDO_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CUT_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_COPY_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_PASTE_ITEM (NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Paste _Special..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLEAR_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_SELECT_ALL_ITEM (NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Mark as U_nread"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_FIND_ITEM (NULL, NULL),
	GNOMEUIINFO_MENU_FIND_AGAIN_ITEM (NULL, NULL),
	GNOMEUIINFO_SUBTREE (N_("_Object"), edit_object_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_previous_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Unread Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: In_complete Task"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Fi_rst Item in Folder"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_next_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Unread Item"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: In_complete Task"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Last Item in Folder"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_toolbars_menu[] = {
	{ GNOME_APP_UI_TOGGLEITEM, N_("FIXME: _Standard"), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	{ GNOME_APP_UI_TOGGLEITEM, N_("FIXME: __Formatting"), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL, 0, 0, NULL },
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Customize..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_menu[] = {
	GNOMEUIINFO_SUBTREE (N_("Pre_vious"), view_previous_menu),
	GNOMEUIINFO_SUBTREE (N_("Ne_xt"), view_next_menu),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("_Toolbars"), view_toolbars_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo insert_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _File..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: It_em..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Object..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo format_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Font..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Paragraph..."), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo tools_forms_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Ch_oose Form..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Desi_gn This Form"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: D_esign a Form..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Publish _Form..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Pu_blish Form As..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Script _Debugger"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo tools_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Spelling..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Chec_k Names"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Address _Book..."), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_SUBTREE (N_("_Forms"), tools_forms_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo actions_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _New Task"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: S_end Status Report"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Mark Complete"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Rec_urrence..."), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: S_kip Occurrence"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Assig_n Task"), NULL, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: _Reply"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Reply to A_ll"), NULL, NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: For_ward"), NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo help_menu[] = {
	GNOMEUIINFO_ITEM_NONE ("FIXME: fix Bonobo so it supports help items!", NULL, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo main_menu[] = {
	GNOMEUIINFO_MENU_FILE_TREE (file_menu),
	GNOMEUIINFO_MENU_EDIT_TREE (edit_menu),
	GNOMEUIINFO_MENU_VIEW_TREE (view_menu),
	GNOMEUIINFO_SUBTREE (N_("_Insert"), insert_menu),
	GNOMEUIINFO_SUBTREE (N_("F_ormat"), format_menu),
	GNOMEUIINFO_SUBTREE (N_("_Tools"), tools_menu),
	GNOMEUIINFO_SUBTREE (N_("Actio_ns"), actions_menu),
	GNOMEUIINFO_MENU_HELP_TREE (help_menu),
	GNOMEUIINFO_END
};


/* Creates the menu bar for the event editor */
static void
create_menu (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;
	BonoboUIHandlerMenuItem *list;

	priv = tedit->priv;

	bonobo_ui_handler_create_menubar (priv->uih);

	list = bonobo_ui_handler_menu_parse_uiinfo_list_with_data (main_menu,
								   tedit);
	bonobo_ui_handler_menu_add_list (priv->uih, "/", list);
}


/* Toolbar */

static GnomeUIInfo toolbar[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("FIXME: Save and Close"),
				N_("Save the task and close the dialog box"),
				NULL,
				GNOME_STOCK_PIXMAP_SAVE),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Print..."),
			       N_("Print this item"), NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Insert File..."),
			       N_("Insert a file as an attachment"), NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Recurrence..."),
			       N_("Configure recurrence rules"), NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Assign Task..."),
			       N_("Assign the task to someone"), NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Delete"),
			       N_("Delete this item"), NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Previous"),
			       N_("Go to the previous item"), NULL),
	GNOMEUIINFO_ITEM_NONE (N_("FIXME: Next"),
			       N_("Go to the next item"), NULL),
	GNOMEUIINFO_ITEM_STOCK (N_("FIXME: Help"),
				N_("See online help"), NULL, GNOME_STOCK_PIXMAP_HELP),
	GNOMEUIINFO_END
};


/* Creates the toolbar for the event editor */
static void
create_toolbar (TaskEditor *tedit)
{
	TaskEditorPrivate *priv;
	BonoboUIHandlerToolbarItem *list;
	GnomeDockItem *dock_item;
	GtkWidget *toolbar_child;

	priv = tedit->priv;

	bonobo_ui_handler_create_toolbar (priv->uih, "Toolbar");

	/* Fetch the toolbar.  What a pain in the ass. */

	dock_item = gnome_app_get_dock_item_by_name (GNOME_APP (priv->app), GNOME_APP_TOOLBAR_NAME);
	g_assert (dock_item != NULL);

	toolbar_child = gnome_dock_item_get_child (dock_item);
	g_assert (toolbar_child != NULL && GTK_IS_TOOLBAR (toolbar_child));

	/* Turn off labels as GtkToolbar sucks */
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar_child), GTK_TOOLBAR_ICONS);

	list = bonobo_ui_handler_toolbar_parse_uiinfo_list_with_data (toolbar,
								      tedit);
	bonobo_ui_handler_toolbar_add_list (priv->uih, "/Toolbar", list);
}


static void
task_editor_destroy (GtkObject *object)
{
	TaskEditor *tedit;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_EDITOR (object));

	tedit = TASK_EDITOR (object);


}


