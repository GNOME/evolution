/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <libintl.h>
#include <locale.h>
#include <libebook/libebook.h>
#include <libedataserver/libedataserver.h>

#include "e-util/e-util.h"

#include "e-vcard-editor.h"

static EBookClient *glob_client = NULL;
static GtkLabel *glob_book_label = NULL;
static GtkToggleButton *glob_save_to_book_check = NULL;
static GtkTextView *glob_vcard_text_view = NULL;
static GtkToggleButton *glob_is_new_check = NULL;

static void
after_save_cb (EVCardEditor *editor,
	       EBookClient *target_client,
	       EContact *contact,
	       gpointer user_data)
{
	ESourceRegistry *registry = user_data;
	gchar *text, *tmp;

	g_clear_object (&glob_client);
	glob_client = g_object_ref (target_client);

	text = e_util_get_source_full_name (registry, e_client_get_source (E_CLIENT (target_client)));
	tmp = g_strconcat ("Address book: ", text, NULL);
	gtk_label_set_text (glob_book_label, tmp);
	g_free (tmp);
	g_free (text);

	text = e_vcard_to_string (E_VCARD (contact));
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (glob_vcard_text_view), text, -1);
	g_free (text);

	gtk_toggle_button_set_active (glob_is_new_check, FALSE);
}

static gboolean
custom_save_cb (EVCardEditor *editor,
		EBookClient *target_client,
		EContact *contact,
		gchar **out_error_message,
		gpointer user_data)
{
	return TRUE;
}

static void
open_editor_clicked_cb (GtkWidget *button,
			gpointer user_data)
{
	ESourceRegistry *registry = user_data;
	EContact *contact = NULL;
	EVCardEditor *editor;
	GError *local_error = NULL;
	GtkWidget *window;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *vcard_str;
	EVCardEditorFlags flags = E_VCARD_EDITOR_FLAG_NONE;

	if (gtk_toggle_button_get_active (glob_is_new_check))
		flags |= E_VCARD_EDITOR_FLAG_IS_NEW;

	if (!glob_client) {
		EClient *client;
		ESource *source;

		source = e_source_registry_ref_default_address_book (registry);
		if (!source)
			source = e_source_registry_ref_builtin_address_book (registry);
		g_assert_nonnull (source);

		client = e_book_client_connect_sync (source, (guint32) -1, NULL, &local_error);
		g_assert_no_error (local_error);
		g_assert_nonnull (client);

		glob_client = E_BOOK_CLIENT (client);

		g_clear_object (&source);
	}

	buffer = gtk_text_view_get_buffer (glob_vcard_text_view);
	gtk_text_buffer_get_bounds (buffer, &start, &end);
	vcard_str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	if (vcard_str && *vcard_str)
		contact = e_contact_new_from_vcard (vcard_str);

	g_free (vcard_str);

	window = gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW);
	editor = e_vcard_editor_new (window ? GTK_WINDOW (window) : NULL, NULL /* no shell here */, glob_client, contact, flags);

	g_signal_connect (editor, "after-save", G_CALLBACK (after_save_cb), registry);

	if (gtk_toggle_button_get_active (glob_save_to_book_check)) {
		/* nothing to do, saving to book does the editor itself */
	} else {
		g_signal_connect (editor, "custom-save", G_CALLBACK (custom_save_cb), registry);
	}

	gtk_widget_set_visible (GTK_WIDGET (editor), TRUE);

	g_clear_object (&contact);
}

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static gboolean
on_idle_create_widget (gpointer user_data)
{
	GtkWidget *window, *widget, *scrolled;
	GtkBox *box;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 480, 640);

	g_signal_connect (window, "delete-event",
		G_CALLBACK (window_delete_event_cb), NULL);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_add (GTK_CONTAINER (window), widget);

	box = GTK_BOX (widget);

	widget = gtk_button_new_with_mnemonic ("_Open vCard Editor");
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (open_editor_clicked_cb), user_data);

	widget = gtk_label_new ("");
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	glob_book_label = GTK_LABEL (widget);

	widget = gtk_check_button_new_with_mnemonic ("_Save to Book (otherwise to data)");
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"active", FALSE,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	glob_save_to_book_check = GTK_TOGGLE_BUTTON (widget);

	widget = gtk_check_button_new_with_mnemonic ("Is _New Contact");
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"active", TRUE,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	glob_is_new_check = GTK_TOGGLE_BUTTON (widget);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (scrolled,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_NONE,
		"kinetic-scrolling", TRUE,
		"min-content-height", 120,
		"propagate-natural-height", TRUE,
		"propagate-natural-width", TRUE,
		NULL);
	gtk_box_pack_start (box, scrolled, TRUE, TRUE, 0);

	widget = gtk_text_view_new ();
	g_object_set (scrolled,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (scrolled), widget);

	glob_vcard_text_view = GTK_TEXT_VIEW (widget);

	gtk_widget_show_all (window);

	return G_SOURCE_REMOVE;
}

int
main (int argc,
      char **argv)
{
	ESourceRegistry *registry;
	GError *local_error = NULL;

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	e_util_init_main_thread (NULL);

	gtk_init (&argc, &argv);

	registry = e_source_registry_new_sync (NULL, &local_error);

	if (local_error != NULL) {
		g_error (
			"Failed to load ESource registry: %s",
			local_error->message);
		g_return_val_if_reached (-1);
	}

	g_idle_add (on_idle_create_widget, registry);

	gtk_main ();

	g_object_unref (registry);
	g_clear_object (&glob_client);
	e_misc_util_free_global_memory ();

	return 0;
}
