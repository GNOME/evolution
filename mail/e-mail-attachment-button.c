/*
 * e-mail-attachment-button.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-attachment-button.h"

#include "e-util/e-binding.h"

#define E_MAIL_ATTACHMENT_BUTTON_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_ATTACHMENT_BUTTON, EMailAttachmentButtonPrivate))

struct _EMailAttachmentButtonPrivate {

	EAttachmentView *view;
	EAttachment *attachment;
	gulong reference_handler_id;

	GtkWidget *inline_button;
	GtkWidget *action_button;
	GtkWidget *cell_view;

	guint expandable : 1;
	guint expanded   : 1;
};

enum {
	PROP_0,
	PROP_ATTACHMENT,
	PROP_EXPANDABLE,
	PROP_EXPANDED,
	PROP_VIEW
};

enum {
	CLICKED,
	LAST_SIGNAL
};

static gpointer parent_class;
static gulong signals[LAST_SIGNAL];

static void
mail_attachment_button_action_clicked_cb (EMailAttachmentButton *button)
{
}

static void
mail_attachment_button_update_cell_view (EMailAttachmentButton *button)
{
	GtkCellView *cell_view;
	EAttachment *attachment;
	GtkTreeRowReference *reference;
	GtkTreeModel *model = NULL;
	GtkTreePath *path = NULL;

	cell_view = GTK_CELL_VIEW (button->priv->cell_view);

	attachment = e_mail_attachment_button_get_attachment (button);
	if (attachment == NULL)
		goto exit;

	reference = e_attachment_get_reference (attachment);
	if (reference == NULL)
		goto exit;

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);

exit:
	gtk_cell_view_set_model (cell_view, model);
	gtk_cell_view_set_displayed_row (cell_view, path);

	if (path != NULL)
		gtk_tree_path_free (path);
}

static void
mail_attachment_button_update_pixbufs (EMailAttachmentButton *button)
{
	GtkCellView *cell_view;
	GtkCellRenderer *renderer;
	GtkIconTheme *icon_theme;
	GdkPixbuf *pixbuf_expander_open;
	GdkPixbuf *pixbuf_expander_closed;
	GList *list;

	icon_theme = gtk_icon_theme_get_default ();

	/* Grab the first cell renderer. */
	cell_view = GTK_CELL_VIEW (button->priv->cell_view);
	list = gtk_cell_view_get_cell_renderers (cell_view);
	renderer = GTK_CELL_RENDERER (list->data);
	g_list_free (list);

	pixbuf_expander_open = gtk_widget_render_icon (
		GTK_WIDGET (button), GTK_STOCK_GO_DOWN,
		GTK_ICON_SIZE_BUTTON, NULL);

	pixbuf_expander_closed = gtk_widget_render_icon (
		GTK_WIDGET (button), GTK_STOCK_GO_FORWARD,
		GTK_ICON_SIZE_BUTTON, NULL);

	g_object_set (
		renderer,
		"pixbuf-expander-open", pixbuf_expander_open,
		"pixbuf-expander-closed", pixbuf_expander_closed,
		NULL);

	g_object_unref (pixbuf_expander_open);
	g_object_unref (pixbuf_expander_closed);
}

static void
mail_attachment_button_set_view (EMailAttachmentButton *button,
                                 EAttachmentView *view)
{
	g_return_if_fail (button->priv->view == NULL);

	button->priv->view = g_object_ref (view);
}

static void
mail_attachment_button_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ATTACHMENT:
			e_mail_attachment_button_set_attachment (
				E_MAIL_ATTACHMENT_BUTTON (object),
				g_value_get_object (value));
			return;

		case PROP_EXPANDABLE:
			e_mail_attachment_button_set_expandable (
				E_MAIL_ATTACHMENT_BUTTON (object),
				g_value_get_boolean (value));
			return;

		case PROP_EXPANDED:
			e_mail_attachment_button_set_expanded (
				E_MAIL_ATTACHMENT_BUTTON (object),
				g_value_get_boolean (value));
			return;

		case PROP_VIEW:
			mail_attachment_button_set_view (
				E_MAIL_ATTACHMENT_BUTTON (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_attachment_button_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ATTACHMENT:
			g_value_set_object (
				value,
				e_mail_attachment_button_get_attachment (
				E_MAIL_ATTACHMENT_BUTTON (object)));
			return;

		case PROP_EXPANDABLE:
			g_value_set_boolean (
				value,
				e_mail_attachment_button_get_expandable (
				E_MAIL_ATTACHMENT_BUTTON (object)));
			return;

		case PROP_EXPANDED:
			g_value_set_boolean (
				value,
				e_mail_attachment_button_get_expanded (
				E_MAIL_ATTACHMENT_BUTTON (object)));
			return;

		case PROP_VIEW:
			g_value_set_object (
				value,
				e_mail_attachment_button_get_view (
				E_MAIL_ATTACHMENT_BUTTON (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_attachment_button_dispose (GObject *object)
{
	EMailAttachmentButtonPrivate *priv;

	priv = E_MAIL_ATTACHMENT_BUTTON_GET_PRIVATE (object);

	if (priv->view != NULL) {
		g_object_unref (priv->view);
		priv->view = NULL;
	}

	if (priv->attachment != NULL) {
		g_signal_handler_disconnect (
			priv->attachment,
			priv->reference_handler_id);
		g_object_unref (priv->attachment);
		priv->attachment = NULL;
	}

	if (priv->inline_button != NULL) {
		g_object_unref (priv->inline_button);
		priv->inline_button = NULL;
	}

	if (priv->action_button != NULL) {
		g_object_unref (priv->action_button);
		priv->action_button = NULL;
	}

	if (priv->cell_view != NULL) {
		g_object_unref (priv->cell_view);
		priv->cell_view = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_attachment_button_style_set (GtkWidget *widget,
                                  GtkStyle *previous_style)
{
	EMailAttachmentButton *button;

	/* Chain up to parent's style_set() method. */
	GTK_WIDGET_CLASS (parent_class)->style_set (widget, previous_style);

	button = E_MAIL_ATTACHMENT_BUTTON (widget);
	mail_attachment_button_update_pixbufs (button);
}

static void
mail_attachment_button_class_init (EMailAttachmentButtonClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailAttachmentButtonPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_attachment_button_set_property;
	object_class->get_property = mail_attachment_button_get_property;
	object_class->dispose = mail_attachment_button_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->style_set = mail_attachment_button_style_set;

	g_object_class_install_property (
		object_class,
		PROP_ATTACHMENT,
		g_param_spec_object (
			"attachment",
			"Attachment",
			NULL,
			E_TYPE_ATTACHMENT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EXPANDABLE,
		g_param_spec_boolean (
			"expandable",
			"Expandable",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_EXPANDED,
		g_param_spec_boolean (
			"expanded",
			"Expanded",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_VIEW,
		g_param_spec_object (
			"view",
			"View",
			NULL,
			E_TYPE_ATTACHMENT_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[CLICKED] = g_signal_new (
		"clicked",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailAttachmentButtonClass, clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
mail_attachment_button_init (EMailAttachmentButton *button)
{
	GtkCellRenderer *renderer;
	GtkCellLayout *cell_layout;
	GtkWidget *container;
	GtkWidget *widget;

	button->priv = E_MAIL_ATTACHMENT_BUTTON_GET_PRIVATE (button);

	/* Configure Widgets */

	container = GTK_WIDGET (button);

	widget = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	button->priv->inline_button = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_new (
		G_OBJECT (button), "expandable",
		G_OBJECT (widget), "sensitive");

	widget = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	button->priv->action_button = g_object_ref (widget);
	gtk_widget_show (widget);

	container = button->priv->inline_button;

	widget = gtk_cell_view_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	button->priv->cell_view = g_object_ref (widget);
	gtk_widget_show (widget);

	container = button->priv->action_button;

	widget = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	/* Configure Renderers */

	cell_layout = GTK_CELL_LAYOUT (button->priv->cell_view);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "is-expander", TRUE, NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, FALSE);

	e_mutual_binding_new (
		G_OBJECT (button), "expanded",
		G_OBJECT (renderer), "is-expanded");

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_BUTTON, NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, FALSE);

	gtk_cell_layout_add_attribute (
		cell_layout, renderer, "gicon",
		E_ATTACHMENT_STORE_COLUMN_ICON);

	/* Configure Signal Handlers */

	g_signal_connect_swapped (
		button->priv->action_button, "clicked",
		G_CALLBACK (mail_attachment_button_action_clicked_cb), button);

	g_signal_connect_swapped (
		button->priv->inline_button, "clicked",
		G_CALLBACK (e_mail_attachment_button_clicked), button);
}

GType
e_mail_attachment_button_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailAttachmentButtonClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_attachment_button_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailAttachmentButton),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_attachment_button_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_HBOX, "EMailAttachmentButton", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_mail_attachment_button_new (EAttachmentView *view)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_VIEW (view), NULL);

	return g_object_new (
		E_TYPE_MAIL_ATTACHMENT_BUTTON,
		"view", view, NULL);
}

EAttachmentView *
e_mail_attachment_button_get_view (EMailAttachmentButton *button)
{
	g_return_val_if_fail (E_IS_MAIL_ATTACHMENT_BUTTON (button), NULL);

	return button->priv->view;
}

EAttachment *
e_mail_attachment_button_get_attachment (EMailAttachmentButton *button)
{
	g_return_val_if_fail (E_IS_MAIL_ATTACHMENT_BUTTON (button), NULL);

	return button->priv->attachment;
}

void
e_mail_attachment_button_set_attachment (EMailAttachmentButton *button,
                                         EAttachment *attachment)
{
	g_return_if_fail (E_IS_MAIL_ATTACHMENT_BUTTON (button));

	if (attachment != NULL) {
		g_return_if_fail (E_IS_ATTACHMENT (attachment));
		g_object_ref (attachment);
	}

	if (button->priv->attachment != NULL) {
		g_signal_handler_disconnect (
			button->priv->attachment,
			button->priv->reference_handler_id);
		g_object_unref (button->priv->attachment);
	}

	button->priv->attachment = attachment;

	if (attachment != NULL) {
		gulong handler_id;

		handler_id = g_signal_connect_swapped (
			attachment, "notify::reference",
			G_CALLBACK (mail_attachment_button_update_cell_view),
			button);
		mail_attachment_button_update_cell_view (button);
		mail_attachment_button_update_pixbufs (button);
		button->priv->reference_handler_id = handler_id;
	}

	g_object_notify (G_OBJECT (button), "attachment");
}

gboolean
e_mail_attachment_button_get_expandable (EMailAttachmentButton *button)
{
	g_return_val_if_fail (E_IS_MAIL_ATTACHMENT_BUTTON (button), FALSE);

	return button->priv->expandable;
}

void
e_mail_attachment_button_set_expandable (EMailAttachmentButton *button,
                                         gboolean expandable)
{
	g_return_if_fail (E_IS_MAIL_ATTACHMENT_BUTTON (button));

	button->priv->expandable = expandable;

	if (!expandable)
		e_mail_attachment_button_set_expanded (button, FALSE);

	g_object_notify (G_OBJECT (button), "expandable");
}

gboolean
e_mail_attachment_button_get_expanded (EMailAttachmentButton *button)
{
	g_return_val_if_fail (E_IS_MAIL_ATTACHMENT_BUTTON (button), FALSE);

	return button->priv->expanded;
}

void
e_mail_attachment_button_set_expanded (EMailAttachmentButton *button,
                                       gboolean expanded)
{
	g_return_if_fail (E_IS_MAIL_ATTACHMENT_BUTTON (button));

	button->priv->expanded = expanded;

	g_object_notify (G_OBJECT (button), "expanded");
}

void
e_mail_attachment_button_clicked (EMailAttachmentButton *button)
{
	g_return_if_fail (E_IS_MAIL_ATTACHMENT_BUTTON (button));

	g_signal_emit (button, signals[CLICKED], 0);
}
