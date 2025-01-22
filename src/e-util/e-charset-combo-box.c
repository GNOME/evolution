/*
 * e-charset-combo-box.c
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

#include <glib/gi18n.h>

#include "e-charset.h"

#include "e-charset-combo-box.h"

struct _ECharsetComboBox {
	GtkComboBox parent;

	/* Used when the user clicks Cancel in the character set
	 * dialog. Reverts to the previous combo box setting. */
	gchar *previous_id;

	/* When setting the character set programmatically, this
	 * prevents the custom character set dialog from running. */
	guint block_dialog : 1;
};

enum {
	PROP_0,
	PROP_CHARSET
};

G_DEFINE_TYPE (ECharsetComboBox, e_charset_combo_box, GTK_TYPE_COMBO_BOX)

static void
charset_combo_box_entry_changed_cb (GtkEntry *entry,
                                    GtkDialog *dialog)
{
	const gchar *text;
	gboolean sensitive;

	text = gtk_entry_get_text (entry);
	sensitive = (text != NULL && *text != '\0');
	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, sensitive);
}

static void
charset_combo_box_run_dialog (ECharsetComboBox *combo_box)
{
	GtkDialog *dialog;
	GtkEntry *entry;
	GtkWidget *container;
	GtkWidget *widget;
	gpointer parent;
	const gchar *charset;

	/* FIXME Using a dialog for this is lame.  Selecting "Other..."
	 *       should unlock an entry directly in the Preferences tab.
	 *       Unfortunately space in Preferences is at a premium right
	 *       now, but we should revisit this when the space issue is
	 *       finally resolved. */

	parent = gtk_widget_get_toplevel (GTK_WIDGET (combo_box));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	charset = combo_box->previous_id;

	widget = gtk_dialog_new_with_buttons (
		_("Character Encoding"), parent,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK, NULL);

	dialog = GTK_DIALOG (widget);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);

	widget = gtk_dialog_get_action_area (dialog);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);

	widget = gtk_dialog_get_content_area (dialog);
	gtk_box_set_spacing (GTK_BOX (widget), 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);

	container = widget;

	widget = gtk_label_new (_("Enter the character set to use"));
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_entry_new ();
	gtk_widget_set_margin_start (widget, 12);
	entry = GTK_ENTRY (widget);
	gtk_entry_set_activates_default (entry, TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect (
		entry, "changed",
		G_CALLBACK (charset_combo_box_entry_changed_cb), dialog);

	/* Set the default text -after- connecting the signal handler.
	 * This will initialize the "OK" button to the proper state. */
	gtk_entry_set_text (entry, charset);

	if (gtk_dialog_run (dialog) == GTK_RESPONSE_OK) {
		charset = gtk_entry_get_text (entry);
		g_return_if_fail (charset != NULL && *charset != '\0');

		/* for the cases where the user confirmed the choice without changing it,
		   it will force to set the correct id (from Other...) on the combo box */
		g_clear_pointer (&combo_box->previous_id, g_free);

		e_charset_combo_box_set_charset (combo_box, charset);
	} else {
		/* Revert to the previously selected character set. */
		combo_box->block_dialog = TRUE;
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), combo_box->previous_id);
		combo_box->block_dialog = FALSE;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
charset_combo_box_changed (GtkComboBox *combo_box)
{
	ECharsetComboBox *self = E_CHARSET_COMBO_BOX (combo_box);
	const gchar *charset;

	/* Chain up to parent's changed() method. */
	if (GTK_COMBO_BOX_CLASS (e_charset_combo_box_parent_class)->changed)
		GTK_COMBO_BOX_CLASS (e_charset_combo_box_parent_class)->changed (combo_box);

	if (self->block_dialog)
		return;

	charset = e_charset_combo_box_get_charset (self);

	/* the "Other..." value is an empty string */
	if (charset && !*charset) {
		charset_combo_box_run_dialog (self);
	} else {
		g_clear_pointer (&self->previous_id, g_free);
		self->previous_id = g_strdup (charset);

		g_object_notify (G_OBJECT (self), "charset");
	}
}

static gboolean
charset_combo_box_is_row_separator (GtkTreeModel *model,
				    GtkTreeIter *iter,
				    gpointer user_data)
{
	gchar *charset = NULL;
	gboolean separator;

	gtk_tree_model_get (model, iter, E_CHARSET_COLUMN_VALUE, &charset, -1);
	separator = (charset == NULL);
	g_free (charset);

	return separator;
}

static void
charset_combo_box_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHARSET:
			e_charset_combo_box_set_charset (
				E_CHARSET_COMBO_BOX (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
charset_combo_box_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHARSET:
			g_value_set_string (
				value, e_charset_combo_box_get_charset (
				E_CHARSET_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
charset_combo_box_dispose (GObject *object)
{
	ECharsetComboBox *self = E_CHARSET_COMBO_BOX (object);

	g_clear_pointer (&self->previous_id, g_free);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_charset_combo_box_parent_class)->dispose (object);
}

static void
e_charset_combo_box_class_init (ECharsetComboBoxClass *class)
{
	GObjectClass *object_class;
	GtkComboBoxClass *combo_box_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = charset_combo_box_set_property;
	object_class->get_property = charset_combo_box_get_property;
	object_class->dispose = charset_combo_box_dispose;

	combo_box_class = GTK_COMBO_BOX_CLASS (class);
	combo_box_class->changed = charset_combo_box_changed;

	g_object_class_install_property (
		object_class,
		PROP_CHARSET,
		g_param_spec_string (
			"charset",
			"Charset",
			"The selected character set",
			"",
			G_PARAM_READWRITE));
}

static void
e_charset_combo_box_init (ECharsetComboBox *self)
{
	GtkComboBox *combo_box = GTK_COMBO_BOX (self);
	GtkCellRenderer *renderer;
	GtkListStore *list_store;
	GtkTreeIter iter;

	list_store = e_charset_create_list_store ();

	/* separator, both label and value are NULL */
	gtk_list_store_append (list_store, &iter);

	/* other option */
	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter,
		E_CHARSET_COLUMN_LABEL, _("Other…"),
		E_CHARSET_COLUMN_VALUE, "",
		-1);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), renderer, "text", E_CHARSET_COLUMN_LABEL);

	gtk_combo_box_set_row_separator_func (combo_box, charset_combo_box_is_row_separator, NULL, NULL);

	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (list_store));
	gtk_combo_box_set_id_column (combo_box, E_CHARSET_COLUMN_VALUE);

	g_object_unref (list_store);
}

GtkWidget *
e_charset_combo_box_new (void)
{
	return g_object_new (E_TYPE_CHARSET_COMBO_BOX, NULL);
}

const gchar *
e_charset_combo_box_get_charset (ECharsetComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_CHARSET_COMBO_BOX (combo_box), NULL);

	return gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));
}

void
e_charset_combo_box_set_charset (ECharsetComboBox *combo_box,
                                 const gchar *charset)
{
	g_return_if_fail (E_IS_CHARSET_COMBO_BOX (combo_box));

	if (charset == NULL || *charset == '\0')
		charset = "UTF-8";

	if (g_strcmp0 (charset, combo_box->previous_id) == 0)
		return;

	combo_box->block_dialog = TRUE;

	g_clear_pointer (&combo_box->previous_id, g_free);
	combo_box->previous_id = g_strdup (charset);

	if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), charset)) {
		GtkListStore *list_store;
		GtkTreeIter iter;
		gchar *escaped_name;
		gchar **str_array;

		/* Escape underlines in the character set name so
		 * they're not treated as GtkLabel mnemonics. */
		str_array = g_strsplit (charset, "_", -1);
		escaped_name = g_strjoinv ("__", str_array);
		g_strfreev (str_array);

		list_store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box)));
		gtk_list_store_prepend (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
			E_CHARSET_COLUMN_LABEL, escaped_name,
			E_CHARSET_COLUMN_VALUE, charset,
			-1);

		g_free (escaped_name);

		gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), charset);
	}

	g_object_notify (G_OBJECT (combo_box), "charset");

	combo_box->block_dialog = FALSE;
}
