/*
 * Evolution calendar - Delegate selector dialog
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libical/ical.h>
#include <libebook/libebook.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "e-delegate-dialog.h"

#define E_DELEGATE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_DELEGATE_DIALOG, EDelegateDialogPrivate))

struct _EDelegateDialogPrivate {
	gchar *name;
	gchar *address;

	GtkBuilder *builder;

	/* Widgets from the UI file */
	GtkWidget *app;
	GtkWidget *hbox;
	GtkWidget *addressbook;

	ENameSelector *name_selector;
	GtkWidget *entry;
};

static const gchar *section_name = "Delegate To";

static gboolean get_widgets			(EDelegateDialog *edd);
static void addressbook_clicked_cb              (GtkWidget *widget, gpointer data);
static void addressbook_response_cb             (GtkWidget *widget, gint response, gpointer data);

G_DEFINE_TYPE (EDelegateDialog, e_delegate_dialog, G_TYPE_OBJECT)

static void
e_delegate_dialog_finalize (GObject *object)
{
	EDelegateDialogPrivate *priv;
	GtkWidget *dialog;

	priv = E_DELEGATE_DIALOG_GET_PRIVATE (object);

	e_name_selector_cancel_loading (priv->name_selector);
	g_object_unref (priv->name_selector);

	/* Destroy the actual dialog. */
	dialog = e_delegate_dialog_get_toplevel (E_DELEGATE_DIALOG (object));
	gtk_widget_destroy (dialog);

	g_free (priv->address);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_delegate_dialog_parent_class)->finalize (object);
}

static void
e_delegate_dialog_class_init (EDelegateDialogClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EDelegateDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_delegate_dialog_finalize;
}

static void
e_delegate_dialog_init (EDelegateDialog *edd)
{
	edd->priv = E_DELEGATE_DIALOG_GET_PRIVATE (edd);
}

EDelegateDialog *
e_delegate_dialog_construct (EDelegateDialog *edd,
                             EClientCache *client_cache,
                             const gchar *name,
                             const gchar *address)
{
	EDelegateDialogPrivate *priv;
	EDestinationStore *destination_store;
	EDestination *dest;
	ENameSelectorModel *name_selector_model;
	ENameSelectorDialog *name_selector_dialog;

	g_return_val_if_fail (E_IS_DELEGATE_DIALOG (edd), NULL);
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	priv = edd->priv;

	/* Load the content widgets */

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "e-delegate-dialog.ui");

	if (!get_widgets (edd)) {
		g_message ("e_delegate_dialog_construct(): Could not find all widgets in the XML file!");
		goto error;
	}

	priv->name_selector = e_name_selector_new (client_cache);
	e_name_selector_load_books (priv->name_selector);
	name_selector_model = e_name_selector_peek_model (priv->name_selector);
	e_name_selector_model_add_section (name_selector_model, section_name, section_name, NULL);

	priv->entry = GTK_WIDGET (e_name_selector_peek_section_entry (priv->name_selector, section_name));
	gtk_widget_show (priv->entry);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->entry, TRUE, TRUE, 6);

	dest = e_destination_new ();

	if (name != NULL && *name)
		e_destination_set_name (dest, name);
	if (address != NULL && *address)
		e_destination_set_email (dest, address);

	e_name_selector_model_peek_section (name_selector_model, section_name, NULL, &destination_store);
	e_destination_store_append_destination (destination_store, dest);
	g_object_unref (dest);

	g_signal_connect (
		priv->addressbook, "clicked",
		G_CALLBACK (addressbook_clicked_cb), edd);

	name_selector_dialog = e_name_selector_peek_dialog (priv->name_selector);
	g_signal_connect (
		name_selector_dialog, "response",
		G_CALLBACK (addressbook_response_cb), edd);

	return edd;

 error:

	g_object_unref (edd);
	return NULL;
}

static gboolean
get_widgets (EDelegateDialog *edd)
{
	EDelegateDialogPrivate *priv;

	priv = edd->priv;

	priv->app = e_builder_get_widget (priv->builder, "delegate-dialog");
	priv->hbox = e_builder_get_widget (priv->builder, "delegate-hbox");
	priv->addressbook = e_builder_get_widget (priv->builder, "addressbook");

	return (priv->app
		&& priv->hbox
		&& priv->addressbook);
}

static void
addressbook_clicked_cb (GtkWidget *widget,
                        gpointer data)
{
	EDelegateDialog *edd = data;

	e_name_selector_show_dialog (
		edd->priv->name_selector,
		e_delegate_dialog_get_toplevel (edd));
}

static void
addressbook_response_cb (GtkWidget *widget,
                         gint response,
                         gpointer data)
{
	EDelegateDialog *edd = data;
	EDelegateDialogPrivate *priv;
	ENameSelectorDialog *name_selector_dialog;

	priv = edd->priv;

	name_selector_dialog = e_name_selector_peek_dialog (priv->name_selector);
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

/**
 * e_delegate_dialog_new:
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
EDelegateDialog *
e_delegate_dialog_new (EClientCache *client_cache,
                       const gchar *name,
                       const gchar *address)
{
	EDelegateDialog *edd;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	edd = g_object_new (E_TYPE_DELEGATE_DIALOG, NULL);

	return e_delegate_dialog_construct (
		E_DELEGATE_DIALOG (edd), client_cache, name, address);
}

gchar *
e_delegate_dialog_get_delegate (EDelegateDialog *edd)
{
	EDelegateDialogPrivate *priv;
	ENameSelectorModel *name_selector_model;
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;

	g_return_val_if_fail (E_IS_DELEGATE_DIALOG (edd), NULL);

	priv = edd->priv;

	name_selector_model = e_name_selector_peek_model (priv->name_selector);
	e_name_selector_model_peek_section (name_selector_model, section_name, NULL, &destination_store);
	destinations = e_destination_store_list_destinations (destination_store);
	if (!destinations)
		return NULL;

	destination = destinations->data;

	if (destination) {
		g_free (priv->address);
		priv->address = g_strdup (e_destination_get_email (destination));
	}

	g_list_free (destinations);
	return g_strdup (priv->address);
}

gchar *
e_delegate_dialog_get_delegate_name (EDelegateDialog *edd)
{
	EDelegateDialogPrivate *priv;
	ENameSelectorModel *name_selector_model;
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;

	g_return_val_if_fail (E_IS_DELEGATE_DIALOG (edd), NULL);

	priv = edd->priv;

	name_selector_model = e_name_selector_peek_model (priv->name_selector);
	e_name_selector_model_peek_section (name_selector_model, section_name, NULL, &destination_store);
	destinations = e_destination_store_list_destinations (destination_store);
	if (!destinations)
		return NULL;

	destination = destinations->data;

	if (destination) {
		g_free (priv->name);
		priv->name = g_strdup (e_destination_get_name (destination));
	}

	g_list_free (destinations);
	return g_strdup (priv->name);
}

GtkWidget *
e_delegate_dialog_get_toplevel (EDelegateDialog *edd)
{
	g_return_val_if_fail (E_IS_DELEGATE_DIALOG (edd), NULL);

	return edd->priv->app;
}

