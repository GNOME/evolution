/*
 * E-shell-view-menu.c: Controls the shell view's menus.
 *
 * This file provides API entry points for changing and updating
 * the menus to reflect the status of Evolution.
 *
 * Authors:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include <gnome.h>
#include "e-shell-view.h"
#include "e-shell-view-menu.h"

static void
esv_cmd_new_folder (GtkWidget *widget, EShellView *esv)
{
	e_shell_view_new_folder (esv);
}

static void
esv_cmd_new_shortcut (GtkWidget *widget, EShellView *esv)
{
	e_shell_view_new_shortcut (esv);
}

static void
esv_cmd_new_mail_message (GtkWidget *widget, EShellView *esv)
{
	e_shell_new_mail_message (esv->eshell);
}

static void
esv_cmd_new_meeting_request (GtkWidget *widget, EShellView *esv)
{
	e_shell_new_meeting_request (esv->eshell);
}

static void
esv_cmd_new_contact (GtkWidget *widget, EShellView *esv)
{
	e_shell_new_contact (esv->eshell);
}

static void
esv_cmd_new_task (GtkWidget *widget, EShellView *esv)
{
	e_shell_new_task (esv->eshell);
}

static void
esv_cmd_new_task_request (GtkWidget *widget, EShellView *esv)
{
	e_shell_new_task_request (esv->eshell);
}

static void
esv_cmd_new_journal_entry (GtkWidget *widget, EShellView *esv)
{
	e_shell_new_journal_entry (esv->eshell);
}

static void
esv_cmd_new_note (GtkWidget *widget, EShellView *esv)
{
	e_shell_new_note (esv->eshell);
}

static void
esv_cmd_open_selected_items (GtkWidget *widget, EShellView *esv)
{
	printf ("Unimplemented open selected items\n");
}

static void
esv_cmd_save_as (GtkWidget *widget, EShellView *esv)
{
}

static void
quit_cmd (GtkWidget *widget, EShellView *esv)
{
	e_shell_quit (esv->eshell);
}

static void
esv_cmd_close_open_items (GtkWidget *widget, EShellView *esv)
{
	printf ("Unimplemented function");
}

static void
esv_cmd_toggle_shortcut_bar (GtkWidget *widget, EShellView *esv)
{
	e_shell_view_toggle_shortcut_bar (esv);
}

static void
esv_cmd_toggle_treeview (GtkWidget *widget, EShellView *esv)
{
	e_shell_view_toggle_treeview (esv);
}


/*
 * Fixme
 *
 * This menu is actually pretty dynamic, it changes de values of various entries
 * depending on the current data being displayed
 *
 * This is currently only a placeholder.  We need to figure what to do about this.
 */
static GnomeUIInfo esv_menu_file_new [] = {

	{ GNOME_APP_UI_ITEM, N_("_Folder"),
	  NULL, esv_cmd_new_folder, NULL,
	  NULL, 0, 0, 'e', GDK_CONTROL_MASK | GDK_SHIFT_MASK },

	{ GNOME_APP_UI_ITEM, N_("Evolution _Bar Shortcut"),
	  NULL, esv_cmd_new_shortcut, NULL,
	  NULL, 0, 0, 'e', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	
	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("_Mail message"),
	  N_("Composes a new mail message"), esv_cmd_new_mail_message, NULL,
	  NULL, 0, 0, 'n', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Appointment"),
	  N_("Composes a new mail message"), esv_cmd_new_mail_message, NULL,
	  NULL, 0, 0, 'a', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("Meeting Re_quest"), NULL,
	  esv_cmd_new_meeting_request, NULL,
	  NULL, 0, 0, 'q', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Contact"), NULL,
	  esv_cmd_new_contact, NULL,
	  NULL, 0, 0, 'c', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Task"), NULL,
	  esv_cmd_new_task, NULL,
	  NULL, 0, 0, 'k', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("Task _Request"), NULL,
	  esv_cmd_new_task_request, NULL,
	  NULL, 0, 0, 'u', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Journal Entry"), NULL,
	  esv_cmd_new_journal_entry, NULL,
	  NULL, 0, 0, 'j', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Note"), NULL,
	  esv_cmd_new_note, NULL,
	  NULL, 0, 0, 'o', GDK_CONTROL_MASK | GDK_SHIFT_MASK },

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_END
};

static GnomeUIInfo esv_menu_file_open [] = {
	{ GNOME_APP_UI_ITEM, N_("_Selected Items"), NULL,
	  esv_cmd_open_selected_items, NULL,
	  NULL, 0, 0, 'o', GDK_CONTROL_MASK },
	
	GNOMEUIINFO_END
};

static GnomeUIInfo esv_menu_folder [] = {
	{ GNOME_APP_UI_ITEM, N_("_New Folder"), NULL,
	  esv_cmd_new_folder, NULL,
	  NULL, 0, 0, 'e', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	
	GNOMEUIINFO_END
};

static GnomeUIInfo esv_menu_file [] = {
	GNOMEUIINFO_SUBTREE_STOCK (N_("_New"), esv_menu_file_new, GNOME_STOCK_MENU_NEW),
	GNOMEUIINFO_SUBTREE_STOCK (N_("_Open"), esv_menu_file_open, GNOME_STOCK_MENU_NEW),
	GNOMEUIINFO_ITEM_NONE (N_("Clos_e All Items"), N_("Closes all the open items"), esv_cmd_close_open_items),
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_MENU_SAVE_AS_ITEM (esv_cmd_save_as, NULL),
	
	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_SUBTREE (N_("_Folder"), esv_menu_folder),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_MENU_EXIT_ITEM(quit_cmd, NULL),

	GNOMEUIINFO_END
};

static GnomeUIInfo esv_menu_edit [] = {
	GNOMEUIINFO_END
};

static GnomeUIInfo esv_menu_view [] = {
	{ GNOME_APP_UI_ITEM, N_("_Toggle Shortcut Bar"),
	  N_("Toggles the shortcut bar"), esv_cmd_toggle_shortcut_bar, NULL,
	  NULL, 0, 0, 'n', GDK_CONTROL_MASK | GDK_SHIFT_MASK },
	{ GNOME_APP_UI_ITEM, N_("_Toggle Treeview"),
	  N_("Toggles the tree view"), esv_cmd_toggle_treeview, NULL,
	  NULL, 0, 0, 'n', GDK_CONTROL_MASK | GDK_SHIFT_MASK },		
	GNOMEUIINFO_END
};

static GnomeUIInfo esv_menu_tools [] = {
	GNOMEUIINFO_END
};

static GnomeUIInfo esv_menu_actions [] = {
	GNOMEUIINFO_END
};

static GnomeUIInfo esv_menu [] = {
	GNOMEUIINFO_MENU_FILE_TREE (esv_menu_file),
	GNOMEUIINFO_MENU_EDIT_TREE (esv_menu_edit),
	GNOMEUIINFO_MENU_VIEW_TREE (esv_menu_view),

	/* FIXME: add Favorites here */

	{ GNOME_APP_UI_SUBTREE, N_("_Tools"), NULL, esv_menu_tools },
	{ GNOME_APP_UI_SUBTREE, N_("_Actions"), NULL, esv_menu_actions },
#warning Should provide a help menu here;  Bonobo needs it
	GNOMEUIINFO_END
};

/*
 * Sets up the menus for the EShellView.
 *
 * Creates the Bonobo UI Handler, and then loads the menus from our
 * GnomeUIInfo definitions
 */
void
e_shell_view_setup_menus (EShellView *eshell_view)
{
	BonoboUIHandlerMenuItem *list;

	eshell_view->uih = bonobo_ui_handler_new ();
	bonobo_ui_handler_set_app (eshell_view->uih, GNOME_APP (eshell_view));
	bonobo_ui_handler_create_menubar (eshell_view->uih);

	list = bonobo_ui_handler_menu_parse_uiinfo_list_with_data (esv_menu, eshell_view);
	bonobo_ui_handler_menu_add_list (eshell_view->uih, "/", list);
	bonobo_ui_handler_menu_free_list (list);
}
