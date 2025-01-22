/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>

#include "e-alphabet-box.h"

struct _EAlphabetBox {
	GtkListBox parent_instance;

	EBookIndices *indices;
	GtkSizeGroup *sizegroup;
	GtkCssProvider *css_provider;
};

enum {
	SIGNAL_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (EAlphabetBox, e_alphabet_box, GTK_TYPE_LIST_BOX)

static void
e_alphabet_box_update (EAlphabetBox *self)
{
	GtkListBoxRow *child;
	GtkWidget *label;
	guint ii;

	if (!self->indices) {
		while (child = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self), 0), child) {
			gtk_widget_destroy (GTK_WIDGET (child));
		}

		return;
	}

	for (ii = 0; self->indices[ii].chr; ii++) {
		gchar *indice_markup = g_markup_printf_escaped ("<small><b>%s</b></small>", self->indices[ii].chr);
		child = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self), ii);
		if (child) {
			label = gtk_bin_get_child (GTK_BIN (child));
			gtk_label_set_markup (GTK_LABEL (label), indice_markup);
		} else {
			GtkStyleContext *style_context;
			label = gtk_label_new (indice_markup);
			g_object_set (label,
				"halign", GTK_ALIGN_CENTER,
				"valign", GTK_ALIGN_CENTER,
				"visible", TRUE,
				"use-markup", TRUE,
				"margin-start", 8,
				"margin-end", 6,
				"margin-top", 4,
				"margin-bottom", 4,
				NULL);

			gtk_list_box_insert (GTK_LIST_BOX (self), label, -1);
			gtk_size_group_add_widget (self->sizegroup, label);

			child = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self), ii);
			gtk_widget_set_margin_bottom (GTK_WIDGET (child), 1);
			style_context = gtk_widget_get_style_context (GTK_WIDGET (child));
			gtk_style_context_add_class (style_context, "frame");
			gtk_style_context_add_provider (style_context,
				GTK_STYLE_PROVIDER (self->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		}

		g_free (indice_markup);
		gtk_widget_set_visible (GTK_WIDGET (child), self->indices[ii].index != G_MAXUINT);
	}

	while (child = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self), ii), child) {
		gtk_widget_destroy (GTK_WIDGET (child));
	}
}

static void
e_alphabet_box_row_activated_cb (GtkListBox *box,
				 GtkListBoxRow *row,
				 gpointer user_data)
{
	EAlphabetBox *self = user_data;

	if (row && self->indices != NULL) {
		guint ii, index = gtk_list_box_row_get_index (row);

		for (ii = 0; ii < index && self->indices[ii].chr != NULL; ii++) {
			/* just verify the index is not out of bounds */
		}

		if (ii == index && self->indices[index].index != G_MAXUINT)
			g_signal_emit (self, signals[SIGNAL_CLICKED], 0, self->indices[index].index, NULL);
	}
}

static void
e_alphabet_box_constructed (GObject *object)
{
	EAlphabetBox *self = E_ALPHABET_BOX (object);
	GError *local_error = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_alphabet_box_parent_class)->constructed (object);

	self->sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

	self->css_provider = gtk_css_provider_new ();

	if (!gtk_css_provider_load_from_data (self->css_provider,
		"EAlphabetBox row {"
		"   border-radius:0px;"
		"   border-top-left-radius:8px;"
		"   border-bottom-left-radius:8px;"
		"}"
		"EAlphabetBox row:focus {"
		"   -gtk-outline-radius:0px;"
		"   -gtk-outline-top-left-radius:6px;"
		"   -gtk-outline-bottom-left-radius:6px;"
		"}",
		-1, &local_error)) {
		g_warning ("%s: Failed to parse CSS: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");
		g_clear_error (&local_error);
	}

	gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self)),
		GTK_STYLE_PROVIDER (self->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_signal_connect (self, "row-activated",
		G_CALLBACK (e_alphabet_box_row_activated_cb), self);
}

static void
e_alphabet_box_dispose (GObject *object)
{
	EAlphabetBox *self = E_ALPHABET_BOX (object);

	g_clear_object (&self->css_provider);
	g_clear_object (&self->sizegroup);

	G_OBJECT_CLASS (e_alphabet_box_parent_class)->dispose (object);
}

static void
e_alphabet_box_finalize (GObject *object)
{
	EAlphabetBox *self = E_ALPHABET_BOX (object);

	g_clear_pointer (&self->indices, e_book_indices_free);

	G_OBJECT_CLASS (e_alphabet_box_parent_class)->finalize (object);
}

static void
e_alphabet_box_class_init (EAlphabetBoxClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	gtk_widget_class_set_css_name (widget_class, "EAlphabetBox");

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_alphabet_box_constructed;
	object_class->dispose = e_alphabet_box_dispose;
	object_class->finalize = e_alphabet_box_finalize;

	signals[SIGNAL_CLICKED] = g_signal_new (
		"clicked",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);
}

static void
e_alphabet_box_init (EAlphabetBox *self)
{
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (self), GTK_SELECTION_NONE);
	gtk_widget_set_margin_top (GTK_WIDGET (self), 1);
}

/**
 * e_alphabet_box_new:
 *
 * Creates a new #EAlphabetBox
 *
 * Returns: (transfer full): a new #EAlphabetBox
 *
 * Since: 3.50
 **/
GtkWidget *
e_alphabet_box_new (void)
{
	return g_object_new (E_TYPE_ALPHABET_BOX, NULL);
}

/**
 * e_alphabet_box_take_indices:
 * @self: an #EAlphabetBox
 * @indices: (nullable) (transfer full): an #EBookIndices indices to use as an alphabet, or %NULL
 *
 * Sets the @indices as an alphabet to be shown in the @self.
 * The function assumes ownership of the @indices.
 *
 * Since: 3.50
 **/
void
e_alphabet_box_take_indices (EAlphabetBox *self,
			     EBookIndices *indices)
{

	g_return_if_fail (E_IS_ALPHABET_BOX (self));

	if (self->indices == indices)
		return;

	if (indices && self->indices) {
		guint ii;

		for (ii = 0; indices[ii].chr != NULL && self->indices[ii].chr != NULL; ii++) {
			if (g_strcmp0 (indices[ii].chr, self->indices[ii].chr) != 0 ||
			    indices[ii].index != self->indices[ii].index)
				break;
		}

		if (indices[ii].chr == NULL && self->indices[ii].chr == NULL) {
			e_book_indices_free (indices);
			return;
		}
	}

	e_book_indices_free (self->indices);
	self->indices = indices;

	e_alphabet_box_update (self);
}

/**
 * e_alphabet_box_get_indices:
 * @self: an #EAlphabetBox
 *
 * Returns the indices used as an alphabet currently shown in the @self.
 *
 * Returns: (nullable): an array of #EBookIndices currently used indices as the alphabet, or %NULL
 *
 * Since: 3.50
 **/
const EBookIndices *
e_alphabet_box_get_indices (EAlphabetBox *self)
{
	g_return_val_if_fail (E_IS_ALPHABET_BOX (self), NULL);

	return self->indices;
}
