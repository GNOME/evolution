/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-composer-from-header.h"

enum {
	PROP_0,
	PROP_OVERRIDE_VISIBLE
};

G_DEFINE_TYPE (
	EComposerFromHeader,
	e_composer_from_header,
	E_TYPE_COMPOSER_HEADER)

static void
composer_from_header_changed_cb (EMailIdentityComboBox *combo_box,
                                 EComposerFromHeader *header)
{
	if (e_mail_identity_combo_box_get_refreshing (combo_box))
		return;

	g_signal_emit_by_name (header, "changed");
}

static void
composer_from_header_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_OVERRIDE_VISIBLE:
			e_composer_from_header_set_override_visible (
				E_COMPOSER_FROM_HEADER (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_from_header_get_property (GObject *object,
				   guint property_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_OVERRIDE_VISIBLE:
			g_value_set_boolean (
				value, e_composer_from_header_get_override_visible (
				E_COMPOSER_FROM_HEADER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_from_header_constructed (GObject *object)
{
	ESourceRegistry *registry;
	EComposerHeader *header;
	EComposerFromHeader *from_header;
	GtkWidget *widget;
	GtkWidget *grid;
	GtkWidget *label;
	GtkWidget *name;
	GtkWidget *address;

	header = E_COMPOSER_HEADER (object);
	from_header = E_COMPOSER_FROM_HEADER (object);
	registry = e_composer_header_get_registry (header);

	/* Input widget must be set before chaining up. */

	widget = e_mail_identity_combo_box_new (registry);
	e_mail_identity_combo_box_set_allow_aliases (E_MAIL_IDENTITY_COMBO_BOX (widget), TRUE);
	gtk_widget_show (widget);
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (composer_from_header_changed_cb), header);
	header->input_widget = g_object_ref_sink (widget);

	grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
	label = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	name = gtk_entry_new ();
	gtk_widget_set_hexpand (name, TRUE);
	gtk_grid_attach (GTK_GRID (grid), name, 1, 0, 1, 1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), name);
	gtk_widget_show (label);
	gtk_widget_show (name);

	label = gtk_label_new_with_mnemonic (_("_Address:"));
	gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);
	address = gtk_entry_new ();
	gtk_widget_set_hexpand (address, TRUE);
	gtk_grid_attach (GTK_GRID (grid), address, 3, 0, 1, 1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), address);
	gtk_widget_show (label);
	gtk_widget_show (address);

	if (from_header->override_visible)
		gtk_widget_show (grid);
	else
		gtk_widget_hide (grid);

	from_header->override_widget = g_object_ref_sink (grid);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_from_header_parent_class)->constructed (object);
}

static void
composer_from_header_dispose (GObject *object)
{
	EComposerFromHeader *from_header;

	from_header = E_COMPOSER_FROM_HEADER (object);

	g_clear_object (&from_header->override_widget);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_composer_from_header_parent_class)->dispose (object);
}

static void
e_composer_from_header_class_init (EComposerFromHeaderClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = composer_from_header_set_property;
	object_class->get_property = composer_from_header_get_property;
	object_class->constructed = composer_from_header_constructed;
	object_class->dispose = composer_from_header_dispose;

	g_object_class_install_property (
		object_class,
		PROP_OVERRIDE_VISIBLE,
		g_param_spec_boolean (
			"override-visible",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_composer_from_header_init (EComposerFromHeader *from_header)
{
}

EComposerHeader *
e_composer_from_header_new (ESourceRegistry *registry,
                            const gchar *label)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_FROM_HEADER,
		"label", label, "button", FALSE,
		"registry", registry, NULL);
}

static GtkComboBox *
e_composer_from_header_get_identities_widget (EComposerFromHeader *header)

{
	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	return GTK_COMBO_BOX (E_COMPOSER_HEADER (header)->input_widget);
}

gchar *
e_composer_from_header_dup_active_id (EComposerFromHeader *header,
				      gchar **alias_name,
				      gchar **alias_address)
{
	GtkComboBox *combo_box;
	gchar *identity_uid = NULL;

	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	combo_box = e_composer_from_header_get_identities_widget (header);

	if (!e_mail_identity_combo_box_get_active_uid (E_MAIL_IDENTITY_COMBO_BOX (combo_box), &identity_uid, alias_name, alias_address))
		return NULL;

	return identity_uid;
}

void
e_composer_from_header_set_active_id (EComposerFromHeader *header,
				      const gchar *active_id,
				      const gchar *alias_name,
				      const gchar *alias_address)
{
	GtkComboBox *combo_box;

	g_return_if_fail (E_IS_COMPOSER_FROM_HEADER (header));

	if (!active_id)
		return;

	combo_box = e_composer_from_header_get_identities_widget (header);

	if (!e_mail_identity_combo_box_set_active_uid (E_MAIL_IDENTITY_COMBO_BOX (combo_box),
		active_id, alias_name, alias_address) && active_id && *active_id) {
		ESourceRegistry *registry;
		GtkTreeModel *model;
		GtkTreeIter iter;
		gint id_column;

		registry = e_composer_header_get_registry (E_COMPOSER_HEADER (header));
		id_column = gtk_combo_box_get_id_column (combo_box);
		model = gtk_combo_box_get_model (combo_box);

		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gchar *identity_uid = NULL;

				gtk_tree_model_get (model, &iter, id_column, &identity_uid, -1);

				if (identity_uid) {
					ESource *source;

					source = e_source_registry_ref_source (registry, identity_uid);
					if (source) {
						if (g_strcmp0 (e_source_get_parent (source), active_id) == 0) {
							g_object_unref (source);
							gtk_combo_box_set_active_id (combo_box, identity_uid);
							g_free (identity_uid);
							break;
						}
						g_object_unref (source);
					}

					g_free (identity_uid);
				}
			} while (gtk_tree_model_iter_next (model, &iter));
		}
	}
}

GtkEntry *
e_composer_from_header_get_name_entry (EComposerFromHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	return GTK_ENTRY (gtk_grid_get_child_at (GTK_GRID (header->override_widget), 1, 0));
}

const gchar *
e_composer_from_header_get_name (EComposerFromHeader *header)
{
	const gchar *text;

	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	text = gtk_entry_get_text (e_composer_from_header_get_name_entry (header));
	if (text && !*text)
		text = NULL;

	return text;
}

void
e_composer_from_header_set_name (EComposerFromHeader *header,
                                 const gchar *name)
{
	GtkEntry *widget;

	g_return_if_fail (E_IS_COMPOSER_FROM_HEADER (header));

	if (!name)
		name = "";

	widget = e_composer_from_header_get_name_entry (header);

	gtk_entry_set_text (widget, name);
}

GtkEntry *
e_composer_from_header_get_address_entry (EComposerFromHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	return GTK_ENTRY (gtk_grid_get_child_at (GTK_GRID (header->override_widget), 3, 0));
}

const gchar *
e_composer_from_header_get_address (EComposerFromHeader *header)
{
	const gchar *text;

	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	text = gtk_entry_get_text (e_composer_from_header_get_address_entry (header));
	if (text && !*text)
		text = NULL;

	return text;
}

void
e_composer_from_header_set_address (EComposerFromHeader *header,
                                    const gchar *address)
{
	GtkEntry *widget;

	g_return_if_fail (E_IS_COMPOSER_FROM_HEADER (header));

	if (!address)
		address = "";

	widget = e_composer_from_header_get_address_entry (header);

	gtk_entry_set_text (widget, address);
}

gboolean
e_composer_from_header_get_override_visible (EComposerFromHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), FALSE);

	return header->override_visible;
}

void
e_composer_from_header_set_override_visible (EComposerFromHeader *header,
					     gboolean visible)
{
	g_return_if_fail (E_IS_COMPOSER_FROM_HEADER (header));

	if (header->override_visible == visible)
		return;

	header->override_visible = visible;

	/* Show/hide the override widgets accordingly. */
	if (header->override_widget) {
		if (visible)
			gtk_widget_show (header->override_widget);
		else
			gtk_widget_hide (header->override_widget);
	}

	g_object_notify (G_OBJECT (header), "override-visible");
}
