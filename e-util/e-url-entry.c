/*
 * e-url-entry.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-url-entry.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-misc-utils.h"

#define E_URL_ENTRY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_URL_ENTRY, EUrlEntryPrivate))

struct _EUrlEntryPrivate {
	GtkWidget *entry;
	GtkWidget *button;
};

G_DEFINE_TYPE (
	EUrlEntry,
	e_url_entry,
	GTK_TYPE_HBOX)

static gboolean
url_entry_text_to_sensitive (GBinding *binding,
                             const GValue *source_value,
                             GValue *target_value,
                             gpointer user_data)
{
	const gchar *text;
	gboolean sensitive;

	text = g_value_get_string (source_value);
	sensitive = (text != NULL && *text != '\0');
	g_value_set_boolean (target_value, sensitive);

	return TRUE;
}

static void
url_entry_button_clicked_cb (GtkButton *button,
                             EUrlEntry *url_entry)
{
	const gchar *text;
	gpointer toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (url_entry));
	toplevel = gtk_widget_is_toplevel (toplevel) ? toplevel : NULL;

	text = gtk_entry_get_text (GTK_ENTRY (url_entry->priv->entry));

	e_show_uri (toplevel, text);
}

static gboolean
url_entry_mnemonic_activate (GtkWidget *widget,
                             gboolean group_cycling)
{
	GtkWidget *entry;

	entry = e_url_entry_get_entry (E_URL_ENTRY (widget));

	return gtk_widget_mnemonic_activate (entry, group_cycling);
}

static void
e_url_entry_class_init (EUrlEntryClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EUrlEntryPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->mnemonic_activate = url_entry_mnemonic_activate;
}

static void
e_url_entry_init (EUrlEntry *url_entry)
{
	GtkWidget *widget;

	url_entry->priv = E_URL_ENTRY_GET_PRIVATE (url_entry);

	widget = gtk_entry_new ();
	gtk_entry_set_placeholder_text (
		GTK_ENTRY (widget), _("Enter a URL here"));
	gtk_box_pack_start (GTK_BOX (url_entry), widget, TRUE, TRUE, 0);
	url_entry->priv->entry = widget;  /* do not reference */
	gtk_widget_show (widget);

	widget = gtk_button_new ();
	gtk_container_add (
		GTK_CONTAINER (widget),
		gtk_image_new_from_stock (
			GTK_STOCK_JUMP_TO,
			GTK_ICON_SIZE_BUTTON));
	gtk_widget_set_tooltip_text (
		widget, _("Click here to open the URL"));
	gtk_box_pack_start (GTK_BOX (url_entry), widget, FALSE, FALSE, 0);
	url_entry->priv->button = widget;  /* do not reference */
	gtk_widget_show_all (widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (url_entry_button_clicked_cb), url_entry);

	g_object_bind_property_full (
		url_entry->priv->entry, "text",
		url_entry->priv->button, "sensitive",
		G_BINDING_SYNC_CREATE,
		url_entry_text_to_sensitive,
		(GBindingTransformFunc) NULL,
		NULL, (GDestroyNotify) NULL);
}

GtkWidget *
e_url_entry_new (void)
{
	return g_object_new (E_TYPE_URL_ENTRY, NULL);
}

GtkWidget *
e_url_entry_get_entry (EUrlEntry *url_entry)
{
	g_return_val_if_fail (E_IS_URL_ENTRY (url_entry), NULL);

	return url_entry->priv->entry;
}

