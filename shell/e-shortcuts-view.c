/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts-view.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
 *
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shortcuts-view.h"

#include "e-folder-dnd-bridge.h"
#include "e-shell-constants.h"
#include "e-shell-marshal.h"
#include "e-shortcuts-view-model.h"

#include "e-util/e-request.h"

#include <glib.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>

#include <libgnome/gnome-i18n.h>

#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <libgnomeui/gnome-popup-menu.h>
#include <libgnomeui/gnome-uidefs.h>

#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>

#include <gal/util/e-util.h>

#include <string.h>


#define PARENT_TYPE E_TYPE_SHORTCUT_BAR
static EShortcutBarClass *parent_class = NULL;

struct _EShortcutsViewPrivate {
	EShortcuts *shortcuts;
};

enum {
	ACTIVATE_SHORTCUT,
	HIDE_REQUESTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Utility functions.  */

static void
show_new_group_dialog (EShortcutsView *view)
{
	char *group_name;

	group_name = e_request_string (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
				       _("Create New Shortcut Group"),
				       _("Group name:"),
				       NULL);

	if (group_name == NULL)
		return;

	e_shortcuts_add_group (view->priv->shortcuts, -1, group_name);

	g_free (group_name);
}


/* Shortcut bar right-click menu.  */

struct _RightClickMenuData {
	EShortcutsView *shortcuts_view;
	int group_num;
};
typedef struct _RightClickMenuData RightClickMenuData;

static void
toggle_large_icons_cb (GtkWidget *widget,
		       void *data)
{
	RightClickMenuData *menu_data;

	menu_data = (RightClickMenuData *) data;

	if (menu_data == NULL)
		return;

	if (! GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	e_shortcuts_set_group_uses_small_icons (menu_data->shortcuts_view->priv->shortcuts, menu_data->group_num, FALSE);
}

static void
toggle_small_icons_cb (GtkWidget *widget,
		       void *data)
{
	RightClickMenuData *menu_data;

	menu_data = (RightClickMenuData *) data;
	if (menu_data == NULL)
		return;

	if (! GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	e_shortcuts_set_group_uses_small_icons (menu_data->shortcuts_view->priv->shortcuts, menu_data->group_num, TRUE);
}

static void
hide_shortcut_bar_cb (GtkWidget *widget,
		      void *data)
{
	RightClickMenuData *menu_data;
	EShortcutsView *shortcut_view;

	menu_data = (RightClickMenuData *) data;

	shortcut_view = E_SHORTCUTS_VIEW (menu_data->shortcuts_view);

	g_signal_emit (shortcut_view, signals[HIDE_REQUESTED], 0);
}

static void
create_new_group_cb (GtkWidget *widget,
		     void *data)
{
	RightClickMenuData *menu_data;

	menu_data = (RightClickMenuData *) data;

	show_new_group_dialog (menu_data->shortcuts_view);
}

static void
destroy_group_cb (GtkWidget *widget,
		  void *data)
{
	RightClickMenuData *menu_data;
	EShortcuts *shortcuts;
	EShortcutsView *shortcuts_view;
	EShortcutsViewPrivate *priv;
	GtkWidget *message_dialog;
	GtkResponseType response;

	menu_data = (RightClickMenuData *) data;
	shortcuts_view = menu_data->shortcuts_view;
	priv = shortcuts_view->priv;
	shortcuts = priv->shortcuts;

	message_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (menu_data->shortcuts_view))),
						 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
						 GTK_MESSAGE_QUESTION,
						 GTK_BUTTONS_NONE,
						 _("Do you really want to remove group "
						   "\"%s\" from the shortcut bar?"),
						 e_shortcuts_get_group_title (shortcuts, menu_data->group_num));

	gtk_dialog_add_buttons (GTK_DIALOG (message_dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_DELETE, GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_title (GTK_WINDOW (message_dialog), "Remove Shortcut Group"); 

	gtk_container_set_border_width (GTK_CONTAINER (message_dialog), 6); 
	
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (message_dialog)->vbox), 6);

	gtk_dialog_set_default_response (GTK_DIALOG (message_dialog), GTK_RESPONSE_OK);

	response = gtk_dialog_run (GTK_DIALOG (message_dialog));
	gtk_widget_destroy (message_dialog);

	if (response == GTK_RESPONSE_OK)
		e_shortcuts_remove_group (shortcuts, menu_data->group_num);
}

static void
rename_group_cb (GtkWidget *widget,
		 void *data)
{
	RightClickMenuData *menu_data;
	EShortcuts *shortcuts;
	EShortcutsView *shortcuts_view;
	EIconBarViewType original_view_type;
	const char *old_name;
	char *new_name;
	int group;

	menu_data = (RightClickMenuData *) data;
	shortcuts_view = menu_data->shortcuts_view;
	shortcuts = shortcuts_view->priv->shortcuts;

	old_name = e_shortcuts_get_group_title (shortcuts, menu_data->group_num);

	new_name = e_request_string (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (shortcuts_view))),
				     _("Rename Shortcut Group"),
				     _("Rename selected shortcut group to:"),
				     old_name);

	if (new_name == NULL)
		return;

	/* Remember the group and flip back to it.  FIXME: This is a workaround
	   to an actual ShortcutBar bug.  */

	group = e_group_bar_get_current_group_num (E_GROUP_BAR (shortcuts_view));
	original_view_type = e_shortcut_bar_get_view_type (E_SHORTCUT_BAR (menu_data->shortcuts_view), group);
	e_shortcuts_rename_group (shortcuts, menu_data->group_num, new_name);

	g_free (new_name);
	e_group_bar_set_current_group_num (E_GROUP_BAR (shortcuts_view), group, FALSE);
	e_shortcut_bar_set_view_type (E_SHORTCUT_BAR (menu_data->shortcuts_view), group, original_view_type);
}

static void
create_default_shortcuts_cb (GtkWidget *widget,
			     void *data)
{
	RightClickMenuData *menu_data;
	EShortcutsView *shortcuts_view;

	menu_data = (RightClickMenuData *) data;
	shortcuts_view = menu_data->shortcuts_view;
	e_shortcuts_add_default_shortcuts (shortcuts_view->priv->shortcuts,
					   e_group_bar_get_current_group_num (E_GROUP_BAR (shortcuts_view)));
}

static GnomeUIInfo icon_size_radio_group_uiinfo[] = {
	{ GNOME_APP_UI_ITEM, N_("_Small Icons"),
	  N_("Show the shortcuts as small icons"), toggle_small_icons_cb, NULL,
	  NULL, 0, 0, 0, 0 },
	{ GNOME_APP_UI_ITEM, N_("_Large Icons"),
	  N_("Show the shortcuts as large icons"), toggle_large_icons_cb, NULL,
	  NULL, 0, 0, 0, 0 },

	GNOMEUIINFO_END
};

static GnomeUIInfo right_click_menu_uiinfo[] = {
	GNOMEUIINFO_RADIOLIST (icon_size_radio_group_uiinfo),

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("_Add Group..."),
	  N_("Create a new shortcut group"), create_new_group_cb, NULL,
	  NULL, 0, 0, 0, 0 },
	{ GNOME_APP_UI_ITEM, N_("_Remove this Group..."),
	  N_("Remove this shortcut group"), destroy_group_cb, NULL,
	  NULL, 0, 0, 0, 0 },
	{ GNOME_APP_UI_ITEM, N_("Re_name this Group..."),
	  N_("Rename this shortcut group"), rename_group_cb, NULL,
	  NULL, 0, 0, 0, 0 },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("_Hide the Shortcut Bar"), 
	  N_("Hide the shortcut bar"), hide_shortcut_bar_cb, NULL,
	  NULL, 0, 0, 0, 0 },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("Create _Default Shortcuts"), 
	  N_("Create Default Shortcuts"), create_default_shortcuts_cb, NULL,
	  NULL, 0, 0, 0, 0 },

	GNOMEUIINFO_END
};

static void
pop_up_right_click_menu_for_group (EShortcutsView *shortcuts_view,
				   GdkEventButton *event,
				   int group_num)
{
	RightClickMenuData *menu_data;
	GtkWidget *popup_menu;

	menu_data = g_new (RightClickMenuData, 1);
	menu_data->shortcuts_view = shortcuts_view;
	menu_data->group_num      = group_num;

	popup_menu = gnome_popup_menu_new (right_click_menu_uiinfo);

	if (e_shortcut_bar_get_view_type (E_SHORTCUT_BAR (shortcuts_view), group_num)
	    == E_ICON_BAR_SMALL_ICONS)
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (icon_size_radio_group_uiinfo[0].widget),
						TRUE);
	else
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (icon_size_radio_group_uiinfo[1].widget),
						TRUE);

	if (group_num == 0)
		gtk_widget_set_sensitive (right_click_menu_uiinfo[3].widget, FALSE);

	gnome_popup_menu_do_popup_modal (popup_menu, NULL, NULL, event, menu_data, GTK_WIDGET (shortcuts_view));

	g_free (menu_data);
	gtk_widget_destroy (popup_menu);
}


/* Data to be passed around for the shortcut right-click menu items.  */

struct _ShortcutRightClickMenuData {
	EShortcutsView *shortcuts_view;
	int group_num;
	int item_num;
};
typedef struct _ShortcutRightClickMenuData ShortcutRightClickMenuData;


/* "Open Shortcut" and "Open Shortcut in New Window" commands.  */

static void
open_shortcut_helper (ShortcutRightClickMenuData *menu_data,
		      gboolean in_new_window)
{
	EShortcutsView *shortcuts_view;
	EShortcuts *shortcuts;
	const EShortcutItem *shortcut_item;

	shortcuts_view = menu_data->shortcuts_view;
	shortcuts = shortcuts_view->priv->shortcuts;

	shortcut_item = e_shortcuts_get_shortcut (shortcuts, menu_data->group_num, menu_data->item_num);
	if (shortcut_item == NULL)
		return;

	g_signal_emit (shortcuts_view, signals[ACTIVATE_SHORTCUT], 0,
		       shortcuts, shortcut_item->uri, in_new_window);
}

static void
open_shortcut_cb (GtkWidget *widget,
		  void *data)
{
	open_shortcut_helper ((ShortcutRightClickMenuData *) data, FALSE);
}

static void
open_shortcut_in_new_window_cb (GtkWidget *widget,
				void *data)
{
	open_shortcut_helper ((ShortcutRightClickMenuData *) data, TRUE);
}


static void
remove_shortcut_cb (GtkWidget *widget,
		    void *data)
{
	ShortcutRightClickMenuData *menu_data;
	EShortcutsView *shortcuts_view;
	EShortcuts *shortcuts;

	menu_data = (ShortcutRightClickMenuData *) data;
	shortcuts_view = menu_data->shortcuts_view;
	shortcuts = shortcuts_view->priv->shortcuts;

	e_shortcuts_remove_shortcut (shortcuts, menu_data->group_num, menu_data->item_num);
}


/* "Rename Shortcut"  command.  */

static void
rename_shortcut_cb (GtkWidget *widget,
		    void *data)
{
	ShortcutRightClickMenuData *menu_data;
	EShortcutsView *shortcuts_view;
	EShortcuts *shortcuts;
	const EShortcutItem *shortcut_item;
	char *new_name;

	menu_data = (ShortcutRightClickMenuData *) data;
	shortcuts_view = menu_data->shortcuts_view;
	shortcuts = shortcuts_view->priv->shortcuts;

	shortcut_item = e_shortcuts_get_shortcut (shortcuts, menu_data->group_num, menu_data->item_num);

	new_name = e_request_string (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (shortcuts_view))),
				     _("Rename Shortcut"),
				     _("Rename selected shortcut to:"),
				     shortcut_item->name);

	if (new_name == NULL)
		return;

	e_shortcuts_update_shortcut (shortcuts, menu_data->group_num, menu_data->item_num,
				     shortcut_item->uri, new_name, shortcut_item->unread_count,
				     shortcut_item->type, shortcut_item->custom_icon_name);
	g_free (new_name);
}

static GnomeUIInfo shortcut_right_click_menu_uiinfo[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("_Open"), N_("Open the folder linked to this shortcut"),
				open_shortcut_cb, GTK_STOCK_OPEN), 
	GNOMEUIINFO_ITEM_NONE  (N_("Open in New _Window"), N_("Open the folder linked to this shortcut in a new window"),
				open_shortcut_in_new_window_cb),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("_Rename"), N_("Rename this shortcut"),
			       rename_shortcut_cb),
	GNOMEUIINFO_ITEM_STOCK (N_("Re_move"), N_("Remove this shortcut from the shortcut bar"),
				remove_shortcut_cb, GTK_STOCK_REMOVE),
	GNOMEUIINFO_END
};

static void
pop_up_right_click_menu_for_shortcut (EShortcutsView *shortcuts_view,
				      GdkEventButton *event,
				      int group_num,
				      int item_num)
{
	ShortcutRightClickMenuData *menu_data;
	GtkWidget *popup_menu;

	menu_data = g_new (ShortcutRightClickMenuData, 1);
	menu_data->shortcuts_view = shortcuts_view;
	menu_data->group_num 	  = group_num;
	menu_data->item_num  	  = item_num;

	popup_menu = gnome_popup_menu_new (shortcut_right_click_menu_uiinfo);

	gnome_popup_menu_do_popup_modal (popup_menu, NULL, NULL, event, menu_data, GTK_WIDGET (shortcuts_view));

	g_free (menu_data);
	gtk_widget_destroy (popup_menu);
}


/* View callbacks.  This part exists mostly because of breakage in the
   EShortcutBar design.  */

static void
group_change_icon_size_callback (EShortcuts *shortucts,
				 int group_num,
				 gboolean use_small_icons,
				 void *data)
{
	EShortcutsView *view;

	view = E_SHORTCUTS_VIEW (data);

	if (use_small_icons)
		e_shortcut_bar_set_view_type (E_SHORTCUT_BAR (view), group_num, E_ICON_BAR_SMALL_ICONS);
	else
		e_shortcut_bar_set_view_type (E_SHORTCUT_BAR (view), group_num, E_ICON_BAR_LARGE_ICONS);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EShortcutsViewPrivate *priv;
	EShortcutsView *shortcuts_view;

	shortcuts_view = E_SHORTCUTS_VIEW (object);

	priv = shortcuts_view->priv;

	if (priv->shortcuts != NULL) {
		g_object_unref (priv->shortcuts);
		priv->shortcuts = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EShortcutsView *shortcuts_view;

	shortcuts_view = E_SHORTCUTS_VIEW (object);

	g_free (shortcuts_view->priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* EShortcutBar methods.  */

static void
item_selected (EShortcutBar *shortcut_bar,
	       GdkEvent *event,
	       int group_num,
	       int item_num)
{
	EShortcuts *shortcuts;
	EShortcutsView *shortcuts_view;
	const EShortcutItem *shortcut_item;

	shortcuts_view = E_SHORTCUTS_VIEW (shortcut_bar);
	shortcuts = shortcuts_view->priv->shortcuts;

	if (event->button.button == 3) {
		if (item_num < 0)
			pop_up_right_click_menu_for_group (shortcuts_view, &event->button,
							   group_num);
		else
			pop_up_right_click_menu_for_shortcut (shortcuts_view, &event->button,
							      group_num, item_num);
		return;
	} else if (event->button.button != 1) {
		return;
	}

	if (item_num < 0)
		return;

	shortcut_item = e_shortcuts_get_shortcut (shortcuts, group_num, item_num);
	if (shortcut_item == NULL)
		return;

	g_signal_emit (shortcuts_view, signals[ACTIVATE_SHORTCUT], 0,
		       shortcuts, shortcut_item->uri, FALSE);
}

static void
get_shortcut_info (EShortcutsView *shortcuts_view,
		   const char *item_uri,
		   int *unread_count_return,
		   const char **type_return,
		   const char **custom_icon_name_return)
{
	EShortcutsViewPrivate *priv;
	EStorageSet *storage_set;
	EFolder *folder;
	EShell *shell;
	char *path;

	priv = shortcuts_view->priv;

	shell = e_shortcuts_get_shell (priv->shortcuts);

	if (! e_shell_parse_uri (shell, item_uri, &path, NULL)) {
		*unread_count_return = 0;
		*type_return = NULL;
		*custom_icon_name_return = NULL;
		return;
	}

	storage_set = e_shell_get_storage_set (shell);

	folder = e_storage_set_get_folder (storage_set, path);
	if (folder != NULL) {
		*unread_count_return     = e_folder_get_unread_count (folder);
		*type_return             = e_folder_get_type_string (folder);
		*custom_icon_name_return = e_folder_get_custom_icon_name (folder);
	} else {
		*unread_count_return     = 0;
		*type_return             = NULL;
		*custom_icon_name_return = NULL;
	}

	g_free (path);
}

static void
impl_shortcut_dropped (EShortcutBar *shortcut_bar,
		       int group_num,
		       int position,
		       const char *item_url,
		       const char *item_name)
{
	EShortcutsView *shortcuts_view;
	EShortcutsViewPrivate *priv;
	int unread_count;
	const char *type;
	const char *custom_icon_name;
	char *tmp;
	char *tp;
	char *name_without_unread;

	shortcuts_view = E_SHORTCUTS_VIEW (shortcut_bar);
	priv = shortcuts_view->priv;

	get_shortcut_info (shortcuts_view, item_url, &unread_count, &type, &custom_icon_name);

	/* Looks funny, but keeps it from adding the unread count
           repeatedly when dragging folders around */
	tmp = g_strdup_printf (" (%d)", unread_count);
	if ((tp = strstr (item_name, tmp)) != NULL)
		name_without_unread = g_strndup (item_name, strlen (item_name) - strlen (tp));
	else
		name_without_unread = g_strdup (item_name);

	e_shortcuts_add_shortcut (priv->shortcuts,
				  group_num, position,
				  item_url,
				  name_without_unread,
				  unread_count,
				  type,
				  custom_icon_name);

	g_free (tmp);
	g_free (name_without_unread);
}

static void
impl_shortcut_dragged (EShortcutBar *shortcut_bar,
		       gint group_num,
		       gint item_num)
{
	EShortcutsView *shortcuts_view;
	EShortcutsViewPrivate *priv;

	shortcuts_view = E_SHORTCUTS_VIEW (shortcut_bar);
	priv = shortcuts_view->priv;

	e_shortcuts_remove_shortcut (priv->shortcuts, group_num, item_num);
}

static gboolean
impl_shortcut_drag_motion (EShortcutBar *shortcut_bar,
			   GtkWidget *widget,
			   GdkDragContext *context,
			   guint time,
			   gint group_num,
			   gint item_num)
{
	EShortcutsView *view;
	EShortcutsViewPrivate *priv;
	const EShortcutItem *shortcut;

	view = E_SHORTCUTS_VIEW (shortcut_bar);
	priv = view->priv;

	shortcut = e_shortcuts_get_shortcut (priv->shortcuts, group_num, item_num);
	if (shortcut == NULL)
		return FALSE;
	if (strncmp (shortcut->uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) != 0)
		return FALSE;

	if (! e_folder_dnd_bridge_motion (widget, context, time,
					  e_shell_get_storage_set (e_shortcuts_get_shell (priv->shortcuts)),
					  shortcut->uri + E_SHELL_URI_PREFIX_LEN))
		gdk_drag_status (context, 0, time);

	return TRUE;
}

static gboolean
impl_shortcut_drag_data_received (EShortcutBar *shortcut_bar,
				  GtkWidget *widget,
				  GdkDragContext *context,
				  GtkSelectionData *selection_data,
				  guint time,
				  gint group_num,
				  gint item_num)
{
	EShortcutsView *view;
	EShortcutsViewPrivate *priv;
	const EShortcutItem *shortcut;

	view = E_SHORTCUTS_VIEW (shortcut_bar);
	priv = view->priv;

	shortcut = e_shortcuts_get_shortcut (priv->shortcuts, group_num, item_num);
	if (shortcut == NULL)
		return FALSE;
	if (strncmp (shortcut->uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) != 0)
		return FALSE;

	e_folder_dnd_bridge_data_received (widget, context, selection_data, time,
					   e_shell_get_storage_set (e_shortcuts_get_shell (priv->shortcuts)),
					   shortcut->uri + E_SHELL_URI_PREFIX_LEN);
	return TRUE;
}


static void
class_init (EShortcutsViewClass *klass)
{
	GObjectClass *object_class;
	EShortcutBarClass *shortcut_bar_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	shortcut_bar_class = E_SHORTCUT_BAR_CLASS (klass);
	shortcut_bar_class->item_selected               = item_selected;
	shortcut_bar_class->shortcut_dropped            = impl_shortcut_dropped;
	shortcut_bar_class->shortcut_dragged            = impl_shortcut_dragged;
	shortcut_bar_class->shortcut_drag_motion        = impl_shortcut_drag_motion;
	shortcut_bar_class->shortcut_drag_data_received = impl_shortcut_drag_data_received;

	parent_class = g_type_class_ref(e_shortcut_bar_get_type ());

	signals[ACTIVATE_SHORTCUT] =
		g_signal_new ("activate_shortcut",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EShortcutsViewClass, activate_shortcut),
			      NULL, NULL,
			      e_shell_marshal_NONE__POINTER_STRING_BOOL,
			      G_TYPE_NONE, 3,
			      G_TYPE_POINTER,
			      G_TYPE_STRING,
			      G_TYPE_BOOLEAN);

	signals[HIDE_REQUESTED] =
		g_signal_new ("hide_requested",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EShortcutsViewClass,
					       hide_requested),
			      NULL, NULL,
			      e_shell_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

static void
init (EShortcutsView *shortcuts_view)
{
	EShortcutsViewPrivate *priv;

	priv = g_new (EShortcutsViewPrivate, 1);
	priv->shortcuts = NULL;

	shortcuts_view->priv = priv;
}


void
e_shortcuts_view_construct (EShortcutsView *shortcuts_view,
			    EShortcuts *shortcuts)
{
	EShortcutsViewPrivate *priv;
	int i, num_groups;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = shortcuts_view->priv;

	priv->shortcuts = shortcuts;
	g_object_ref (priv->shortcuts);

	e_shortcut_bar_set_model (E_SHORTCUT_BAR (shortcuts_view),
				  E_SHORTCUT_MODEL (e_shortcuts_view_model_new (shortcuts)));

	g_signal_connect_object (shortcuts, "group_change_icon_size",
				 G_CALLBACK (group_change_icon_size_callback), shortcuts_view, 0);

	num_groups = e_shortcuts_get_num_groups (shortcuts);
	for (i = 0; i < num_groups; i ++) {
		if (e_shortcuts_get_group_uses_small_icons (shortcuts, i))
			e_shortcut_bar_set_view_type (E_SHORTCUT_BAR (shortcuts_view), i, E_ICON_BAR_SMALL_ICONS);
		else
			e_shortcut_bar_set_view_type (E_SHORTCUT_BAR (shortcuts_view), i, E_ICON_BAR_LARGE_ICONS);
	}
}

GtkWidget *
e_shortcuts_view_new (EShortcuts *shortcuts)
{
	GtkWidget *new;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	new = g_object_new (e_shortcuts_view_get_type (), NULL);
	e_shortcuts_view_construct (E_SHORTCUTS_VIEW (new), shortcuts);

	return new;
}


E_MAKE_TYPE (e_shortcuts_view, "EShortcutsView", EShortcutsView, class_init, init, PARENT_TYPE)
