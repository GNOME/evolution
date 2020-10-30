/*
 * Copyright (C) 2020 Red Hat (www.redhat.com)
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
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-ellipsized-combo-box-text.h"

struct _EEllipsizedComboBoxTextPrivate {
	gint max_natural_width;
};

G_DEFINE_TYPE_WITH_PRIVATE (EEllipsizedComboBoxText, e_ellipsized_combo_box_text, GTK_TYPE_COMBO_BOX_TEXT)

static void
ellipsized_combo_box_text_get_preferred_width (GtkWidget *widget,
					       gint *minimum_width,
					       gint *natural_width)
{
	EEllipsizedComboBoxText *combo_box = E_ELLIPSIZED_COMBO_BOX_TEXT (widget);

	GTK_WIDGET_CLASS (e_ellipsized_combo_box_text_parent_class)->get_preferred_width (widget, minimum_width, natural_width);

	if (*natural_width > combo_box->priv->max_natural_width + (25 * gtk_widget_get_scale_factor (widget)))
		*natural_width = combo_box->priv->max_natural_width;
}

static void
ellipsized_combo_box_text_constructed (GObject *object)
{
	GList *cells, *link;

	G_OBJECT_CLASS (e_ellipsized_combo_box_text_parent_class)->constructed (object);

	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (object));
	for (link = cells; link; link = g_list_next (link)) {
		if (GTK_IS_CELL_RENDERER_TEXT (link->data)) {
			g_object_set (link->data,
				"ellipsize", PANGO_ELLIPSIZE_END,
				NULL);
		}
	}

	g_list_free (cells);
}

static void
e_ellipsized_combo_box_text_class_init (EEllipsizedComboBoxTextClass *klass)
{
	GtkWidgetClass *widget_class;
	GObjectClass *object_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->get_preferred_width = ellipsized_combo_box_text_get_preferred_width;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = ellipsized_combo_box_text_constructed;
}

static void
e_ellipsized_combo_box_text_init (EEllipsizedComboBoxText *combo_box)
{
	combo_box->priv = e_ellipsized_combo_box_text_get_instance_private (combo_box);
	combo_box->priv->max_natural_width = 225;
}

GtkWidget *
e_ellipsized_combo_box_text_new (gboolean has_entry)
{
	return g_object_new (e_ellipsized_combo_box_text_get_type (),
		"has-entry", has_entry,
		NULL);
}

gint
e_ellipsized_combo_box_text_get_max_natural_width (EEllipsizedComboBoxText *combo_box)
{
	g_return_val_if_fail (E_IS_ELLIPSIZED_COMBO_BOX_TEXT (combo_box), -1);

	return combo_box->priv->max_natural_width;
}

void
e_ellipsized_combo_box_text_set_max_natural_width (EEllipsizedComboBoxText *combo_box,
						   gint max_natural_width)
{
	g_return_if_fail (E_IS_ELLIPSIZED_COMBO_BOX_TEXT (combo_box));

	if (combo_box->priv->max_natural_width != max_natural_width) {
		GtkWidget *widget;

		combo_box->priv->max_natural_width = max_natural_width;

		widget = GTK_WIDGET (combo_box);

		if (gtk_widget_get_realized (widget))
			gtk_widget_queue_resize (widget);
	}
}
