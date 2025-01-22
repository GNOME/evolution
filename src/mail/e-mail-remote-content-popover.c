/*
 * Copyright (C) 2019 Red Hat (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of version 2.1. of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"
#include "e-mail-reader.h"

#include "e-mail-remote-content-popover.h"

#define REMOTE_CONTENT_KEY_POPOVER	"remote-content-key-popover"
#define REMOTE_CONTENT_KEY_IS_MAIL	"remote-content-key-is-mail"
#define REMOTE_CONTENT_KEY_VALUE	"remote-content-key-value"

static void
destroy_remote_content_popover (EMailReader *reader)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));

	g_object_set_data (G_OBJECT (reader), REMOTE_CONTENT_KEY_POPOVER, NULL);
}

static void
load_remote_content_clicked_cb (GtkButton *button,
				EMailReader *reader)
{
	EMailDisplay *mail_display;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	destroy_remote_content_popover (reader);

	mail_display = e_mail_reader_get_mail_display (reader);

	/* This causes reload, thus also alert removal */
	e_mail_display_load_images (mail_display);
}

static GList *
get_from_mail_addresses (EMailDisplay *mail_display)
{
	EMailPartList *part_list;
	CamelMimeMessage *message;
	CamelInternetAddress *from;
	GList *mails = NULL;

	g_return_val_if_fail (E_IS_MAIL_DISPLAY (mail_display), NULL);

	part_list = e_mail_display_get_part_list (mail_display);
	if (!part_list)
		return NULL;

	message = e_mail_part_list_get_message (part_list);
	if (!message)
		return NULL;

	from = camel_mime_message_get_from (message);
	if (from) {
		GHashTable *domains;
		GHashTableIter iter;
		gpointer key, value;
		gint ii, len;

		domains = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);

		len = camel_address_length (CAMEL_ADDRESS (from));
		for (ii = 0; ii < len; ii++) {
			const gchar *mail = NULL;

			if (!camel_internet_address_get	(from, ii, NULL, &mail))
				break;

			if (mail && *mail) {
				const gchar *at;

				mails = g_list_prepend (mails, g_strdup (mail));

				at = strchr (mail, '@');
				if (at && at != mail && at[1])
					g_hash_table_insert (domains, (gpointer) at, NULL);
			}
		}

		g_hash_table_iter_init (&iter, domains);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			const gchar *domain = key;

			mails = g_list_prepend (mails, g_strdup (domain));
		}

		g_hash_table_destroy (domains);
	}

	return g_list_reverse (mails);
}

static void
remote_content_menu_deactivate_cb (GtkMenuShell *popup_menu,
				   GtkToggleButton *toggle_button)
{
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

	gtk_toggle_button_set_active (toggle_button, FALSE);
	gtk_menu_detach (GTK_MENU (popup_menu));
}

static void
remote_content_menu_activate_cb (GObject *item,
				 EMailReader *reader)
{
	EMailDisplay *mail_display;
	EMailRemoteContent *remote_content;
	gboolean is_mail;
	const gchar *value;

	g_return_if_fail (GTK_IS_MENU_ITEM (item));
	g_return_if_fail (E_IS_MAIL_READER (reader));

	is_mail = GPOINTER_TO_INT (g_object_get_data (item, REMOTE_CONTENT_KEY_IS_MAIL)) == 1;
	value = g_object_get_data (item, REMOTE_CONTENT_KEY_VALUE);

	destroy_remote_content_popover (reader);

	g_return_if_fail (value && *value);

	mail_display = e_mail_reader_get_mail_display (reader);
	if (!mail_display)
		return;

	remote_content = e_mail_display_ref_remote_content (mail_display);
	if (!remote_content)
		return;

	if (is_mail)
		e_mail_remote_content_add_mail (remote_content, value);
	else
		e_mail_remote_content_add_site (remote_content, value);

	g_clear_object (&remote_content);

	e_mail_display_reload (mail_display);
}

static void
remote_content_disable_activate_cb (GObject *item,
				    EMailReader *reader)
{
	EMailDisplay *mail_display;
	GSettings *settings;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_settings_set_boolean (settings, "notify-remote-content", FALSE);
	g_clear_object (&settings);

	destroy_remote_content_popover (reader);

	mail_display = e_mail_reader_get_mail_display (reader);
	if (mail_display)
		e_mail_display_reload (mail_display);
}

static void
add_remote_content_menu_item (EMailReader *reader,
			      GtkWidget *popup_menu,
			      const gchar *label,
			      gboolean is_mail,
			      const gchar *value)
{
	GtkWidget *item;
	GObject *object;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (GTK_IS_MENU (popup_menu));
	g_return_if_fail (label != NULL);
	g_return_if_fail (value != NULL);

	item = gtk_menu_item_new_with_label (label);
	object = G_OBJECT (item);

	g_object_set_data (object, REMOTE_CONTENT_KEY_IS_MAIL, is_mail ? GINT_TO_POINTER (1) : NULL);
	g_object_set_data_full (object, REMOTE_CONTENT_KEY_VALUE, g_strdup (value), g_free);

	g_signal_connect (item, "activate", G_CALLBACK (remote_content_menu_activate_cb), reader);

	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
}

static void
show_remote_content_popup (EMailReader *reader,
			   GdkEventButton *event,
			   GtkToggleButton *toggle_button)
{
	EMailDisplay *mail_display;
	GList *mails, *sites, *link;
	GtkWidget *popup_menu = NULL;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	mail_display = e_mail_reader_get_mail_display (reader);
	mails = get_from_mail_addresses (mail_display);
	sites = e_mail_display_get_skipped_remote_content_sites (mail_display);

	for (link = mails; link; link = g_list_next (link)) {
		const gchar *mail = link->data;
		gchar *label;

		if (!mail || !*mail)
			continue;

		if (!popup_menu)
			popup_menu = gtk_menu_new ();

		if (*mail == '@')
			label = g_strdup_printf (_("Allow remote content for anyone from %s"), mail);
		else
			label = g_strdup_printf (_("Allow remote content for %s"), mail);

		add_remote_content_menu_item (reader, popup_menu, label, TRUE, mail);

		g_free (label);
	}

	for (link = sites; link; link = g_list_next (link)) {
		const gchar *site = link->data;
		gchar *label;

		if (!site || !*site)
			continue;

		if (!popup_menu)
			popup_menu = gtk_menu_new ();

		label = g_strdup_printf (_("Allow remote content from %s"), site);

		add_remote_content_menu_item (reader, popup_menu, label, FALSE, site);

		g_free (label);
	}

	g_list_free_full (mails, g_free);
	g_list_free_full (sites, g_free);

	if (popup_menu) {
		GtkWidget *box = gtk_widget_get_parent (GTK_WIDGET (toggle_button));
		GtkWidget *item;

		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);

		item = gtk_menu_item_new_with_label (_("Do not show this message again"));
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
		g_signal_connect (item, "activate", G_CALLBACK (remote_content_disable_activate_cb), reader);

		gtk_toggle_button_set_active (toggle_button, TRUE);

		g_signal_connect (
			popup_menu, "deactivate",
			G_CALLBACK (remote_content_menu_deactivate_cb), toggle_button);

		gtk_widget_show_all (popup_menu);

		gtk_menu_attach_to_widget (GTK_MENU (popup_menu), box, NULL);

		g_object_set (popup_menu,
		              "anchor-hints", (GDK_ANCHOR_FLIP_Y |
		                               GDK_ANCHOR_SLIDE |
		                               GDK_ANCHOR_RESIZE),
		              NULL);

		gtk_menu_popup_at_widget (GTK_MENU (popup_menu),
		                          box,
		                          GDK_GRAVITY_SOUTH_WEST,
		                          GDK_GRAVITY_NORTH_WEST,
		                          (const GdkEvent *) event);
	}
}

static gboolean
options_remote_content_button_press_cb (GtkToggleButton *toggle_button,
					GdkEventButton *event,
					EMailReader *reader)
{
	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	if (event && event->button == 1) {
		show_remote_content_popup (reader, event, toggle_button);
		return TRUE;
	}

	return FALSE;
}

static GtkWidget *
create_remote_content_alert_button (EMailReader *reader)
{
	GtkWidget *box, *button, *arrow;

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	gtk_style_context_add_class (gtk_widget_get_style_context (box), "linked");

	button = gtk_button_new_with_label (_("Load remote content"));
	gtk_container_add (GTK_CONTAINER (box), button);

	g_signal_connect (button, "clicked",
		G_CALLBACK (load_remote_content_clicked_cb), reader);

	button = gtk_toggle_button_new ();
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	g_signal_connect (button, "button-press-event",
		G_CALLBACK (options_remote_content_button_press_cb), reader);

	arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (button), arrow);

	gtk_widget_show_all (box);

	return box;
}

void
e_mail_remote_content_popover_run (EMailReader *reader,
				   GtkWidget *relative_to,
				   const GtkAllocation *position)
{
	GtkPopover *popover;
	GtkWidget *hbox, *vbox, *widget;
	PangoAttrList *bold;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (GTK_IS_WIDGET (relative_to));
	g_return_if_fail (position != NULL);

	popover = GTK_POPOVER (gtk_popover_new (relative_to));

	gtk_popover_set_position (popover, GTK_POS_BOTTOM);
	gtk_popover_set_pointing_to (popover, position);
	gtk_popover_set_modal (popover, TRUE);

	gtk_container_set_border_width (GTK_CONTAINER (popover), 12);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_add (GTK_CONTAINER (popover), hbox);

	widget = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DND);
	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_START,
		"vexpand", FALSE,
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	g_object_set (G_OBJECT (vbox),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	widget = gtk_label_new (_("Remote content download had been blocked for this message."));
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"attributes", bold,
		"wrap", TRUE,
		"width-chars", 20,
		"max-width-chars", 80,
		"xalign", 0.0,
		NULL);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	pango_attr_list_unref (bold);

	widget = gtk_label_new (_("You can download remote content manually, or set to remember to download remote content for this sender or used sites."));
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"wrap", TRUE,
		"width-chars", 20,
		"max-width-chars", 80,
		"xalign", 0.0,
		NULL);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	widget = create_remote_content_alert_button (reader);
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_END,
		"hexpand", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	g_object_set_data_full (G_OBJECT (reader), REMOTE_CONTENT_KEY_POPOVER, popover, (GDestroyNotify) gtk_widget_destroy);

	g_signal_connect_object (popover, "closed",
		G_CALLBACK (destroy_remote_content_popover), reader, G_CONNECT_SWAPPED);

	gtk_popover_popup (popover);
}
