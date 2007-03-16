/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Copyright (C) 2002-2004 Novell, Inc.
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

/* ExchangeDelegatesUser: A single delegate */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exchange-delegates-user.h"

#include <e2k-global-catalog.h>
#include <e2k-marshal.h>
#include <e2k-sid.h>
#include <e2k-utils.h>

#include <e-util/e-dialog-utils.h>
#include <e-util/e-dialog-widgets.h>
#include <e-util/e-gtk-utils.h>
#include <glade/glade.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktogglebutton.h>

#undef GTK_DISABLE_DEPRECATED
#include <gtk/gtkoptionmenu.h>

#include <string.h>

#define EXCHANGE_DELEGATES_USER_SEPARATOR -2
#define EXCHANGE_DELEGATES_USER_CUSTOM    -3
/* Can't use E2K_PERMISSIONS_ROLE_CUSTOM, because it's -1, which
 * means "end of list" to e_dialog_option_menu_get/set
 */

static const int exchange_perm_map[] = {
	E2K_PERMISSIONS_ROLE_NONE,
	E2K_PERMISSIONS_ROLE_REVIEWER,
	E2K_PERMISSIONS_ROLE_AUTHOR,
	E2K_PERMISSIONS_ROLE_EDITOR,

	EXCHANGE_DELEGATES_USER_SEPARATOR,
	EXCHANGE_DELEGATES_USER_CUSTOM,

	-1
};

const char *exchange_delegates_user_folder_names[] = {
	"calendar", "tasks", "inbox", "contacts"
};
static const char *widget_names[] = {
	"calendar_perms", "task_perms", "inbox_perms", "contact_perms",
};


enum {
	EDITED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

static void
finalize (GObject *object)
{
	ExchangeDelegatesUser *user = EXCHANGE_DELEGATES_USER (object);

	if (user->display_name)
		g_free (user->display_name);
	if (user->dn)
		g_free (user->dn);
	if (user->entryid)
		g_byte_array_free (user->entryid, TRUE);
	if (user->sid)
		g_object_unref (user->sid);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
class_init (GObjectClass *object_class)
{
	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->finalize = finalize;

	/* signals */
	signals[EDITED] =
		g_signal_new ("edited",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ExchangeDelegatesUserClass, edited),
			      NULL, NULL,
			      e2k_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

E2K_MAKE_TYPE (exchange_delegates_user, ExchangeDelegatesUser, class_init, NULL, PARENT_TYPE)

static inline gboolean
is_delegate_role (E2kPermissionsRole role)
{
	return (role == E2K_PERMISSIONS_ROLE_NONE ||
		role == E2K_PERMISSIONS_ROLE_REVIEWER ||
		role == E2K_PERMISSIONS_ROLE_AUTHOR ||
		role == E2K_PERMISSIONS_ROLE_EDITOR);
}

static void
set_perms (GtkWidget *omenu, E2kPermissionsRole role)
{
	if (!is_delegate_role (role)) {
		GtkWidget *menu, *item;

		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (omenu));

		item = gtk_menu_item_new ();
		gtk_widget_set_sensitive (item, FALSE);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		item = gtk_menu_item_new_with_label (_("Custom"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		gtk_widget_show_all (menu);

		role = EXCHANGE_DELEGATES_USER_CUSTOM;
	}

	e_dialog_option_menu_set (omenu, role, exchange_perm_map);
}

static void
parent_window_destroyed (gpointer dialog, GObject *where_parent_window_was)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
}

/**
 * exchange_delegates_user_edit:
 * @user: a delegate
 * @parent_window: parent window for the editor dialog
 *
 * Brings up a dialog to edit @user's permissions as a delegate.
 * An %edited signal will be emitted if anything changed.
 *
 * Return value: %TRUE for "OK", %FALSE for "Cancel".
 **/
gboolean
exchange_delegates_user_edit (ExchangeDelegatesUser *user,
			      GtkWidget *parent_window)
{
	GladeXML *xml;
	GtkWidget *dialog, *table, *label, *menu, *check;
	char *title;
	int button, i;
	E2kPermissionsRole role;
	gboolean modified;

	g_return_val_if_fail (EXCHANGE_IS_DELEGATES_USER (user), FALSE);
	g_return_val_if_fail (E2K_IS_SID (user->sid), FALSE);

	/* Grab the Glade widgets */
	xml = glade_xml_new (
		CONNECTOR_GLADEDIR "/exchange-delegates.glade",
		"delegate_permissions", PACKAGE);
	g_return_val_if_fail (xml, FALSE);

	title = g_strdup (_("Delegate Permissions"));

	dialog = glade_xml_get_widget (xml, "delegate_permissions");
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	e_dialog_set_transient_for (GTK_WINDOW (dialog), parent_window);
	g_free (title);

	table = glade_xml_get_widget (xml, "toplevel_table");
	gtk_widget_reparent (table, GTK_DIALOG (dialog)->vbox);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 6);

	title = g_strdup_printf (_("Permissions for %s"), user->display_name);
	label = glade_xml_get_widget (xml, "delegate_label");
	gtk_label_set_text (GTK_LABEL (label), title);
	g_free (title);

	/* Set up the permissions */
	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		menu = glade_xml_get_widget (xml, widget_names[i]);
		set_perms (menu, user->role[i]);
	}
	check = glade_xml_get_widget (xml, "see_private_checkbox");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
				      user->see_private);

	/* Run the dialog, while watching its parent. */
	g_object_weak_ref (G_OBJECT (parent_window),
			   parent_window_destroyed, dialog);
	g_object_add_weak_pointer (G_OBJECT (parent_window),
				   (gpointer*)&parent_window);
	button = gtk_dialog_run (GTK_DIALOG (dialog));
	if (parent_window) {
		g_object_remove_weak_pointer (G_OBJECT (parent_window),
					      (gpointer *)&parent_window);
		g_object_weak_unref (G_OBJECT (parent_window),
				     parent_window_destroyed, dialog);
	}

	if (button != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	/* And update */
	modified = FALSE;
	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		menu = glade_xml_get_widget (xml, widget_names[i]);
		role = e_dialog_option_menu_get (menu, exchange_perm_map);

		if (is_delegate_role (user->role[i]) &&
		    user->role[i] != role) {
			user->role[i] = role;
			modified = TRUE;
		}
	}
	check = glade_xml_get_widget (xml, "see_private_checkbox");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)) !=
	    user->see_private) {
		user->see_private = !user->see_private;
		modified = TRUE;
	}

	g_object_unref (xml);
	gtk_widget_destroy (dialog);

	if (modified)
		g_signal_emit (user, signals[EDITED], 0);

	return TRUE;
}

/**
 * exchange_delegates_user_new:
 * @display_name: the delegate's (UTF8) display name
 *
 * Return value: a new delegate user with default permissions (but
 * with most of the internal data blank).
 **/
ExchangeDelegatesUser *
exchange_delegates_user_new (const char *display_name)
{
	ExchangeDelegatesUser *user;
	int i;

	user = g_object_new (EXCHANGE_TYPE_DELEGATES_USER, NULL);
	user->display_name = g_strdup (display_name);

	for (i = 0; i < EXCHANGE_DELEGATES_LAST; i++) {
		if (i == EXCHANGE_DELEGATES_CALENDAR ||
		    i == EXCHANGE_DELEGATES_TASKS)
			user->role[i] = E2K_PERMISSIONS_ROLE_EDITOR;
		else
			user->role[i] = E2K_PERMISSIONS_ROLE_NONE;
	}

	return user;
}

/**
 * exchange_delegates_user_new_from_gc:
 * @gc: the global catalog object
 * @email: email address of the new delegate
 * @creator_entryid: The value of the PR_CREATOR_ENTRYID property
 * on the LocalFreebusy file.
 *
 * Return value: a new delegate user with default permissions and
 * internal data filled in from the global catalog.
 **/
ExchangeDelegatesUser *
exchange_delegates_user_new_from_gc (E2kGlobalCatalog *gc,
				     const char *email,
				     GByteArray *creator_entryid)
{
	E2kGlobalCatalogStatus status;
	E2kGlobalCatalogEntry *entry;
	ExchangeDelegatesUser *user;
	guint8 *p;

	status = e2k_global_catalog_lookup (
		gc, NULL, /* FIXME: cancellable */
		E2K_GLOBAL_CATALOG_LOOKUP_BY_EMAIL, email,
		(E2K_GLOBAL_CATALOG_LOOKUP_SID |
		 E2K_GLOBAL_CATALOG_LOOKUP_LEGACY_EXCHANGE_DN),
		&entry);
	if (status != E2K_GLOBAL_CATALOG_OK)
		return NULL;

	user = exchange_delegates_user_new (e2k_sid_get_display_name (entry->sid));
	user->dn = g_strdup (entry->dn);
	user->sid = entry->sid;
	g_object_ref (user->sid);

	user->entryid = g_byte_array_new ();
	p = creator_entryid->data + creator_entryid->len - 2;
	while (p > creator_entryid->data && *p)
		p--;
	g_byte_array_append (user->entryid, creator_entryid->data,
			     p - creator_entryid->data + 1);
	g_byte_array_append (user->entryid, (guint8*)entry->legacy_exchange_dn,
			     strlen (entry->legacy_exchange_dn));
	g_byte_array_append (user->entryid, (guint8*)"", 1);

	return user;
}
