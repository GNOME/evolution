/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>

#include "e-alphabet-box.h"

struct _EAlphabetBoxPrivate {
	GtkSizeGroup *sizegroup;
	GtkCssProvider *css_provider;

	GtkWidget *layout;
	GtkWidget *flowbox;
	GtkWidget *scrollbar;
	EBookIndices *indices;

	guint idle_update_id;
};

enum {
	SIGNAL_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EAlphabetBox, e_alphabet_box, GTK_TYPE_BIN)

static void
e_alphabet_box_update (EAlphabetBox *self)
{
	PangoAttrList *attrs = NULL;
	GtkFlowBoxChild *child;
	GtkFlowBox *box = GTK_FLOW_BOX (self->priv->flowbox);
	GtkWidget *label;
	guint ii;

	if (!self->priv->indices) {
		while (child = gtk_flow_box_get_child_at_index (box, 0), child) {
			gtk_widget_destroy (GTK_WIDGET (child));
		}

		return;
	}

	for (ii = 0; self->priv->indices[ii].chr; ii++) {
		child = gtk_flow_box_get_child_at_index (box, ii);
		if (child) {
			label = gtk_bin_get_child (GTK_BIN (child));
			gtk_label_set_label (GTK_LABEL (label), self->priv->indices[ii].chr);
		} else {
			if (!attrs) {
				attrs = pango_attr_list_new ();
				pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
				pango_attr_list_insert (attrs, pango_attr_scale_new (0.8));
			}

			label = gtk_label_new (self->priv->indices[ii].chr);
			g_object_set (label,
				"halign", GTK_ALIGN_CENTER,
				"valign", GTK_ALIGN_CENTER,
				"visible", TRUE,
				"attributes", attrs,
				"margin-start", 2,
				NULL);

			gtk_flow_box_insert (box, label, -1);
			gtk_size_group_add_widget (self->priv->sizegroup, label);

			child = gtk_flow_box_get_child_at_index (box, ii);
			gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (child)),
				GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

			e_binding_bind_property (label, "visible", child, "visible", G_BINDING_SYNC_CREATE);
		}

		gtk_widget_set_sensitive (label, self->priv->indices[ii].index != G_MAXUINT);
		gtk_widget_set_visible (label, self->priv->indices[ii].index != G_MAXUINT);
	}

	g_clear_pointer (&attrs, pango_attr_list_unref);

	while (child = gtk_flow_box_get_child_at_index (box, ii), child) {
		gtk_widget_destroy (GTK_WIDGET (child));
	}
}

static void
e_alphabet_box_show_all (GtkWidget *widget)
{
	/* No child to show, internal widgets are handled manually */
	gtk_widget_set_visible (widget, TRUE);
}

static gboolean
e_alphabet_box_update_idle_cb (gpointer user_data)
{
	EAlphabetBox *self = user_data;
	GtkAdjustment *adj;
	gint page_size = gtk_widget_get_allocated_height (self->priv->layout);
	gint need_height = gtk_widget_get_allocated_height (self->priv->flowbox);
	gint need_width = gtk_widget_get_allocated_width (self->priv->flowbox);
	gdouble value;

	self->priv->idle_update_id = 0;

	gtk_widget_set_visible (GTK_WIDGET (self->priv->scrollbar), page_size < need_height);
	if (page_size > need_height)
		need_height = page_size;

	gtk_widget_set_size_request (self->priv->layout, need_width, -1);

	adj = gtk_range_get_adjustment (GTK_RANGE (self->priv->scrollbar));
	g_object_set (adj,
		"lower", 0.0,
		"upper", (gdouble) need_height,
		"page-size", (gdouble) page_size,
		"step-increment", 30.0,
		"page-increment", (gdouble) page_size,
		NULL);

	value = gtk_adjustment_get_value (adj);
	if (value > 1e-9 && value + page_size > need_height)
		gtk_adjustment_set_value (adj, CLAMP (need_height - page_size, 0, need_height - page_size));

	return FALSE;
}

static void
e_alphabet_box_size_allocate_cb (GtkWidget *widget,
				 GtkAllocation *allocation,
				 gpointer user_data)
{
	EAlphabetBox *self = user_data;

	if (!self->priv->idle_update_id)
		self->priv->idle_update_id = g_idle_add (e_alphabet_box_update_idle_cb, self);
}

static void
e_alphabet_box_scrollbar_notify_value_cb (GObject *adjustment,
					  GParamSpec *param,
					  gpointer user_data)
{
	EAlphabetBox *self = user_data;
	gdouble value = gtk_adjustment_get_value (GTK_ADJUSTMENT (adjustment));

	gtk_layout_move (GTK_LAYOUT (self->priv->layout), self->priv->flowbox, 0, -value);
}

static void
e_alphabet_box_child_activated_cb (GtkFlowBox *flowbox,
				   GtkFlowBoxChild *child,
				   gpointer user_data)
{
	EAlphabetBox *self = user_data;

	if (child && self->priv->indices != NULL) {
		guint ii, index = gtk_flow_box_child_get_index (child);

		for (ii = 0; ii < index && self->priv->indices[ii].chr != NULL; ii++) {
			/* just verify the index is not out of bounds */
		}

		if (ii == index && self->priv->indices[index].index != G_MAXUINT)
			g_signal_emit (self, signals[SIGNAL_CLICKED], 0, self->priv->indices[index].index, NULL);
	}
}

static gboolean
e_alphabet_box_scroll_event_cb (GtkWidget *widget,
				GdkEvent *event,
				gpointer user_data)
{
	EAlphabetBox *self = E_ALPHABET_BOX (widget);

	if (!gtk_widget_get_visible (self->priv->scrollbar))
		return FALSE;

	return gtk_widget_event (self->priv->scrollbar, event);
}

static void
e_alphabet_box_constructed (GObject *object)
{
	EAlphabetBox *self = E_ALPHABET_BOX (object);
	GtkWidget *hbox;
	GError *local_error = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_alphabet_box_parent_class)->constructed (object);

	self->priv->sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

	self->priv->css_provider = gtk_css_provider_new ();

	if (!gtk_css_provider_load_from_data (self->priv->css_provider,
		"EAlphabetBox flowboxchild {"
		"   border-style:solid;"
		"   border-radius:0px;"
		"   border-top-left-radius:8px;"
		"   border-bottom-left-radius:8px;"
		"   border-color:@theme_selected_bg_color;"
		"   border-width:1px;"
		"}",
		-1, &local_error)) {
		g_warning ("%s: Failed to parse CSS: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");
		g_clear_error (&local_error);
	}

	gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self)),
		GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (self), hbox);
	gtk_widget_set_visible (hbox, TRUE);

	self->priv->layout = gtk_layout_new (NULL, NULL);
	g_object_set (self->priv->layout,
		"margin", 2,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (hbox), self->priv->layout, FALSE, FALSE, 0);

	self->priv->scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, NULL);
	gtk_widget_set_visible (self->priv->scrollbar, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), self->priv->scrollbar, FALSE, FALSE, 0);

	self->priv->flowbox = gtk_flow_box_new ();
	g_object_set (self->priv->flowbox,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_START,
		"visible", TRUE,
		"activate-on-single-click", TRUE,
		"column-spacing", 2,
		"homogeneous", TRUE,
		"max-children-per-line", 1,
		"row-spacing", 2,
		"selection-mode", GTK_SELECTION_NONE,
		NULL);

	gtk_layout_put (GTK_LAYOUT (self->priv->layout), self->priv->flowbox, 0, 0);

	g_signal_connect (self->priv->layout, "size-allocate",
		G_CALLBACK (e_alphabet_box_size_allocate_cb), self);
	g_signal_connect (self->priv->flowbox, "size-allocate",
		G_CALLBACK (e_alphabet_box_size_allocate_cb), self);
	g_signal_connect (gtk_range_get_adjustment (GTK_RANGE (self->priv->scrollbar)), "notify::value",
		G_CALLBACK (e_alphabet_box_scrollbar_notify_value_cb), self);
	g_signal_connect (self->priv->flowbox, "child-activated",
		G_CALLBACK (e_alphabet_box_child_activated_cb), self);
	g_signal_connect (self, "scroll-event",
		G_CALLBACK (e_alphabet_box_scroll_event_cb), NULL);
}

static void
e_alphabet_box_dispose (GObject *object)
{
	EAlphabetBox *self = E_ALPHABET_BOX (object);

	if (self->priv->idle_update_id) {
		g_source_remove (self->priv->idle_update_id);
		self->priv->idle_update_id = 0;
	}

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_alphabet_box_parent_class)->dispose (object);
}

static void
e_alphabet_box_finalize (GObject *object)
{
	EAlphabetBox *self = E_ALPHABET_BOX (object);

	g_clear_object (&self->priv->css_provider);
	g_clear_object (&self->priv->sizegroup);
	e_book_indices_free (self->priv->indices);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_alphabet_box_parent_class)->finalize (object);
}

static void
e_alphabet_box_class_init (EAlphabetBoxClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show_all = e_alphabet_box_show_all;

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
	self->priv = e_alphabet_box_get_instance_private (self);
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

	if (self->priv->indices == indices)
		return;

	if (indices && self->priv->indices) {
		guint ii;

		for (ii = 0; indices[ii].chr != NULL && self->priv->indices[ii].chr != NULL; ii++) {
			if (g_strcmp0 (indices[ii].chr, self->priv->indices[ii].chr) != 0 ||
			    indices[ii].index != self->priv->indices[ii].index)
				break;
		}

		if (indices[ii].chr == NULL && self->priv->indices[ii].chr == NULL) {
			e_book_indices_free (indices);
			return;
		}
	}

	e_book_indices_free (self->priv->indices);
	self->priv->indices = indices;

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

	return self->priv->indices;
}
