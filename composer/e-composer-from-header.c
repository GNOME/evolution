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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-composer-from-header.h"

G_DEFINE_TYPE (
	EComposerFromHeader,
	e_composer_from_header,
	E_TYPE_COMPOSER_HEADER)

static void
composer_from_header_changed_cb (EMailIdentityComboBox *combo_box,
                                 EComposerFromHeader *header)
{
	g_signal_emit_by_name (header, "changed");
}

static void
composer_from_header_constructed (GObject *object)
{
	ESourceRegistry *registry;
	EComposerHeader *header;
	GtkWidget *widget;

	header = E_COMPOSER_HEADER (object);
	registry = e_composer_header_get_registry (header);

	/* Input widget must be set before chaining up. */

	widget = e_mail_identity_combo_box_new (registry);
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (composer_from_header_changed_cb), header);
	header->input_widget = g_object_ref_sink (widget);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_from_header_parent_class)->constructed (object);
}

static void
e_composer_from_header_class_init (EComposerFromHeaderClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = composer_from_header_constructed;
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

const gchar *
e_composer_from_header_get_active_id (EComposerFromHeader *header)
{
	GtkComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	combo_box = GTK_COMBO_BOX (E_COMPOSER_HEADER (header)->input_widget);

	return gtk_combo_box_get_active_id (combo_box);
}

void
e_composer_from_header_set_active_id (EComposerFromHeader *header,
                                      const gchar *active_id)
{
	GtkComboBox *combo_box;

	g_return_if_fail (E_IS_COMPOSER_FROM_HEADER (header));

	if (!active_id)
		return;

	combo_box = GTK_COMBO_BOX (E_COMPOSER_HEADER (header)->input_widget);

	if (!gtk_combo_box_set_active_id (combo_box, active_id) && active_id && *active_id) {
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
