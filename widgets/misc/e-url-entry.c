/*
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
 *
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "e-url-entry.h"
#include "e-util/e-util.h"

struct _EUrlEntryPrivate {
	GtkWidget *entry;
	GtkWidget *button;
};

static void finalize (GObject *object);

static void button_clicked_cb (GtkWidget *widget, gpointer data);
static void entry_changed_cb (GtkEditable *editable, gpointer data);

static gboolean mnemonic_activate (GtkWidget *widget, gboolean group_cycling);

G_DEFINE_TYPE (
	EUrlEntry,
	e_url_entry,
	GTK_TYPE_HBOX)

static void
e_url_entry_class_init (EUrlEntryClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->mnemonic_activate = mnemonic_activate;
}

static void
e_url_entry_init (EUrlEntry *url_entry)
{
	EUrlEntryPrivate *priv;
	GtkWidget *pixmap;

	priv = g_new0 (EUrlEntryPrivate, 1);
	url_entry->priv = priv;

	priv->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (url_entry), priv->entry, TRUE, TRUE, 0);
	priv->button = gtk_button_new ();
	gtk_widget_set_sensitive (priv->button, FALSE);
	gtk_box_pack_start (GTK_BOX (url_entry), priv->button, FALSE, FALSE, 0);
	atk_object_set_name (
		gtk_widget_get_accessible (priv->button),
		_("Click here to go to URL"));
	pixmap = gtk_image_new_from_icon_name ("go-jump", GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (priv->button), pixmap);
	gtk_widget_show (pixmap);

	gtk_widget_show (priv->button);
	gtk_widget_show (priv->entry);

	g_signal_connect (priv->button, "clicked",
			  G_CALLBACK (button_clicked_cb), url_entry);
	g_signal_connect (priv->entry, "changed",
			  G_CALLBACK (entry_changed_cb), url_entry);
}

static void
finalize (GObject *object)
{
	EUrlEntry *url_entry;

	url_entry = E_URL_ENTRY (object);
	if (url_entry->priv) {
		g_free (url_entry->priv);
		url_entry->priv = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_url_entry_parent_class)->finalize (object);
}

/* GtkWidget::mnemonic_activate() handler for the EUrlEntry */
static gboolean
mnemonic_activate (GtkWidget *widget, gboolean group_cycling)
{
	EUrlEntry *url_entry;
	EUrlEntryPrivate *priv;

	url_entry = E_URL_ENTRY (widget);
	priv = url_entry->priv;

	return gtk_widget_mnemonic_activate (priv->entry, group_cycling);
}



GtkWidget *
e_url_entry_new (void)
{
	return g_object_new (E_TYPE_URL_ENTRY, NULL);
}

GtkWidget *
e_url_entry_get_entry (EUrlEntry *url_entry)
{
	EUrlEntryPrivate *priv;

	g_return_val_if_fail (url_entry != NULL, NULL);
	g_return_val_if_fail (E_IS_URL_ENTRY (url_entry), NULL);

	priv = url_entry->priv;

	return priv->entry;
}

static void
button_clicked_cb (GtkWidget *widget, gpointer data)
{
	EUrlEntry *url_entry;
	EUrlEntryPrivate *priv;
	const gchar *uri;

	url_entry = E_URL_ENTRY (data);
	priv = url_entry->priv;

	uri = gtk_entry_get_text (GTK_ENTRY (priv->entry));

	/* FIXME Pass a parent window. */
	e_show_uri (NULL, uri);
}

static void
entry_changed_cb (GtkEditable *editable, gpointer data)
{
	EUrlEntry *url_entry;
	EUrlEntryPrivate *priv;
	const gchar *url;

	url_entry = E_URL_ENTRY (data);
	priv = url_entry->priv;

	url = gtk_entry_get_text (GTK_ENTRY (priv->entry));
	gtk_widget_set_sensitive (priv->button, url != NULL && *url != '\0');
}
