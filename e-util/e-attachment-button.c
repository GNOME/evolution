/*
 * e-attachment-button.c
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

/* Much of the popup menu logic here was ripped from GtkMenuToolButton. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-attachment-button.h"
#include "e-misc-utils.h"

#define E_ATTACHMENT_BUTTON_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_BUTTON, EAttachmentButtonPrivate))

struct _EAttachmentButtonPrivate {

	EAttachmentView *view;
	EAttachment *attachment;
	gulong reference_handler_id;

	GBinding *can_show_binding;
	GBinding *shown_binding;
	GBinding *zoom_to_window_binding;

	GtkWidget *expand_button;
	GtkWidget *toggle_button;
	GtkWidget *cell_view;
	GtkWidget *popup_menu;

	guint expandable : 1;
	guint expanded : 1;
	guint zoom_to_window : 1;
};

enum {
	PROP_0,
	PROP_ATTACHMENT,
	PROP_EXPANDABLE,
	PROP_EXPANDED,
	PROP_VIEW,
	PROP_ZOOM_TO_WINDOW
};

G_DEFINE_TYPE (
	EAttachmentButton,
	e_attachment_button,
	GTK_TYPE_HBOX)

static void
attachment_button_menu_deactivate_cb (EAttachmentButton *button)
{
	EAttachmentView *view;
	GtkActionGroup *action_group;
	GtkToggleButton *toggle_button;

	view = e_attachment_button_get_view (button);
	action_group = e_attachment_view_get_action_group (view, "inline");
	toggle_button = GTK_TOGGLE_BUTTON (button->priv->toggle_button);

	gtk_toggle_button_set_active (toggle_button, FALSE);

	gtk_action_group_set_visible (action_group, FALSE);
}

static void
attachment_button_menu_position (GtkMenu *menu,
                                 gint *x,
                                 gint *y,
                                 gboolean *push_in,
                                 EAttachmentButton *button)
{
	GtkRequisition menu_requisition;
	GtkTextDirection direction;
	GtkAllocation allocation;
	GdkRectangle monitor;
	GdkScreen *screen;
	GdkWindow *window;
	GtkWidget *widget;
	GtkWidget *toggle_button;
	gint monitor_num;

	widget = GTK_WIDGET (button);
	toggle_button = button->priv->toggle_button;
	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &menu_requisition, NULL);

	window = gtk_widget_get_parent_window (widget);
	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	if (monitor_num < 0)
		monitor_num = 0;
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	gtk_widget_get_allocation (widget, &allocation);

	gdk_window_get_origin (window, x, y);
	*x += allocation.x;
	*y += allocation.y;

	direction = gtk_widget_get_direction (widget);
	if (direction == GTK_TEXT_DIR_LTR)
		*x += MAX (allocation.width - menu_requisition.width, 0);
	else if (menu_requisition.width > allocation.width)
		*x -= menu_requisition.width - allocation.width;

	gtk_widget_get_allocation (toggle_button, &allocation);

	if ((*y + allocation.height +
		menu_requisition.height) <= monitor.y + monitor.height)
		*y += allocation.height;
	else if ((*y - menu_requisition.height) >= monitor.y)
		*y -= menu_requisition.height;
	else if (monitor.y + monitor.height -
		(*y + allocation.height) > *y)
		*y += allocation.height;
	else
		*y -= menu_requisition.height;

	*push_in = FALSE;
}

static void
attachment_button_select_path (EAttachmentButton *button)
{
	EAttachmentView *view;
	EAttachment *attachment;
	GtkTreeRowReference *reference;
	GtkTreePath *path;

	attachment = e_attachment_button_get_attachment (button);
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	reference = e_attachment_get_reference (attachment);
	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	view = e_attachment_button_get_view (button);
	path = gtk_tree_row_reference_get_path (reference);

	e_attachment_view_unselect_all (view);
	e_attachment_view_select_path (view, path);

	gtk_tree_path_free (path);
}

static void
attachment_button_show_popup_menu (EAttachmentButton *button,
                                   GdkEventButton *event)
{
	EAttachmentView *view;
	GtkActionGroup *action_group;
	GtkToggleButton *toggle_button;

	view = e_attachment_button_get_view (button);
	action_group = e_attachment_view_get_action_group (view, "inline");
	toggle_button = GTK_TOGGLE_BUTTON (button->priv->toggle_button);

	attachment_button_select_path (button);
	gtk_toggle_button_set_active (toggle_button, TRUE);

	e_attachment_view_show_popup_menu (
		view, event, (GtkMenuPositionFunc)
		attachment_button_menu_position, button);

	gtk_action_group_set_visible (action_group, TRUE);
}

static void
attachment_button_update_cell_view (EAttachmentButton *button)
{
	GtkCellView *cell_view;
	EAttachment *attachment;
	GtkTreeRowReference *reference;
	GtkTreeModel *model = NULL;
	GtkTreePath *path = NULL;

	cell_view = GTK_CELL_VIEW (button->priv->cell_view);

	attachment = e_attachment_button_get_attachment (button);
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
attachment_button_update_pixbufs (EAttachmentButton *button)
{
	GtkIconTheme *icon_theme;
	GtkCellLayout *cell_layout;
	GtkCellRenderer *renderer;
	GdkPixbuf *pixbuf_expander_open;
	GdkPixbuf *pixbuf_expander_closed;
	GList *list;

	icon_theme = gtk_icon_theme_get_default ();

	/* Grab the first cell renderer. */
	cell_layout = GTK_CELL_LAYOUT (button->priv->cell_view);
	list = gtk_cell_layout_get_cells (cell_layout);
	renderer = GTK_CELL_RENDERER (list->data);
	g_list_free (list);

	pixbuf_expander_open = gtk_icon_theme_load_icon (
		icon_theme, "go-down",
		GTK_ICON_SIZE_BUTTON, 0, NULL);

	pixbuf_expander_closed = gtk_icon_theme_load_icon (
		icon_theme, "go-next",
		GTK_ICON_SIZE_BUTTON, 0, NULL);

	g_object_set (
		renderer,
		"pixbuf-expander-open", pixbuf_expander_open,
		"pixbuf-expander-closed", pixbuf_expander_closed,
		NULL);

	g_object_unref (pixbuf_expander_open);
	g_object_unref (pixbuf_expander_closed);
}

static void
attachment_button_expand_clicked_cb (EAttachmentButton *button)
{
	gboolean expanded;

	expanded = e_attachment_button_get_expanded (button);
	e_attachment_button_set_expanded (button, !expanded);
}

static void
attachment_button_expand_drag_begin_cb (EAttachmentButton *button,
                                        GdkDragContext *context)
{
	EAttachmentView *view;

	view = e_attachment_button_get_view (button);

	attachment_button_select_path (button);
	e_attachment_view_drag_begin (view, context);
}

static void
attachment_button_expand_drag_data_get_cb (EAttachmentButton *button,
                                           GdkDragContext *context,
                                           GtkSelectionData *selection,
                                           guint info,
                                           guint time)
{
	EAttachmentView *view;
	EAttachment *attachment;
	gchar *mime_type = NULL;

	attachment = e_attachment_button_get_attachment (button);

	if (attachment != NULL)
		mime_type = e_attachment_dup_mime_type (attachment);

	if (mime_type != NULL) {
		gboolean processed = FALSE;
		GdkAtom atom;
		gchar *atom_name;

		atom = gtk_selection_data_get_target (selection);
		atom_name = gdk_atom_name (atom);

		if (g_strcmp0 (atom_name, mime_type) == 0) {
			CamelMimePart *mime_part;

			mime_part = e_attachment_ref_mime_part (attachment);

			if (mime_part != NULL) {
				CamelDataWrapper *wrapper;
				CamelStream *stream;
				GByteArray *buffer;

				buffer = g_byte_array_new ();
				stream = camel_stream_mem_new ();
				camel_stream_mem_set_byte_array (
					CAMEL_STREAM_MEM (stream),
					buffer);
				wrapper = camel_medium_get_content (
					CAMEL_MEDIUM (mime_part));
				camel_data_wrapper_decode_to_stream_sync (
					wrapper, stream, NULL, NULL);
				g_object_unref (stream);

				gtk_selection_data_set (
					selection, atom, 8,
					buffer->data, buffer->len);
				processed = TRUE;

				g_byte_array_free (buffer, TRUE);

				g_object_unref (mime_part);
			}
		}

		g_free (atom_name);
		g_free (mime_type);

		if (processed)
			return;
	}

	view = e_attachment_button_get_view (button);

	e_attachment_view_drag_data_get (
		view, context, selection, info, time);
}

static void
attachment_button_expand_drag_end_cb (EAttachmentButton *button,
                                      GdkDragContext *context)
{
	EAttachmentView *view;

	view = e_attachment_button_get_view (button);

	e_attachment_view_drag_end (view, context);
}

static gboolean
attachment_button_toggle_button_press_event_cb (EAttachmentButton *button,
                                                GdkEventButton *event)
{
	if (event->button == 1) {
		attachment_button_show_popup_menu (button, event);
		return TRUE;
	}

	return FALSE;
}

static void
attachment_button_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ATTACHMENT:
			e_attachment_button_set_attachment (
				E_ATTACHMENT_BUTTON (object),
				g_value_get_object (value));
			return;

		case PROP_EXPANDABLE:
			e_attachment_button_set_expandable (
				E_ATTACHMENT_BUTTON (object),
				g_value_get_boolean (value));
			return;

		case PROP_EXPANDED:
			e_attachment_button_set_expanded (
				E_ATTACHMENT_BUTTON (object),
				g_value_get_boolean (value));
			return;

		case PROP_VIEW:
			e_attachment_button_set_view (
				E_ATTACHMENT_BUTTON (object),
				g_value_get_object (value));
			return;

		case PROP_ZOOM_TO_WINDOW:
			e_attachment_button_set_zoom_to_window (
				E_ATTACHMENT_BUTTON (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_button_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ATTACHMENT:
			g_value_set_object (
				value,
				e_attachment_button_get_attachment (
				E_ATTACHMENT_BUTTON (object)));
			return;

		case PROP_EXPANDABLE:
			g_value_set_boolean (
				value,
				e_attachment_button_get_expandable (
				E_ATTACHMENT_BUTTON (object)));
			return;

		case PROP_EXPANDED:
			g_value_set_boolean (
				value,
				e_attachment_button_get_expanded (
				E_ATTACHMENT_BUTTON (object)));
			return;

		case PROP_VIEW:
			g_value_set_object (
				value,
				e_attachment_button_get_view (
				E_ATTACHMENT_BUTTON (object)));
			return;

		case PROP_ZOOM_TO_WINDOW:
			g_value_set_boolean (
				value,
				e_attachment_button_get_zoom_to_window (
				E_ATTACHMENT_BUTTON (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_button_dispose (GObject *object)
{
	EAttachmentButtonPrivate *priv;

	priv = E_ATTACHMENT_BUTTON_GET_PRIVATE (object);

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

	if (priv->expand_button != NULL) {
		g_object_unref (priv->expand_button);
		priv->expand_button = NULL;
	}

	if (priv->toggle_button != NULL) {
		g_object_unref (priv->toggle_button);
		priv->toggle_button = NULL;
	}

	if (priv->cell_view != NULL) {
		g_object_unref (priv->cell_view);
		priv->cell_view = NULL;
	}

	if (priv->popup_menu != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->popup_menu, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->popup_menu);
		priv->popup_menu = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_button_parent_class)->dispose (object);
}

static void
attachment_button_style_updated (GtkWidget *widget)
{
	EAttachmentButton *button;

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_attachment_button_parent_class)->style_updated (widget);

	button = E_ATTACHMENT_BUTTON (widget);
	attachment_button_update_pixbufs (button);
}

static void
e_attachment_button_class_init (EAttachmentButtonClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EAttachmentButtonPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_button_set_property;
	object_class->get_property = attachment_button_get_property;
	object_class->dispose = attachment_button_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->style_updated = attachment_button_style_updated;

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
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ZOOM_TO_WINDOW,
		g_param_spec_boolean (
			"zoom-to-window",
			"Zoom to window",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
e_attachment_button_init (EAttachmentButton *button)
{
	GtkCellRenderer *renderer;
	GtkCellLayout *cell_layout;
	GtkTargetEntry *targets;
	GtkTargetList *list;
	GtkWidget *container;
	GtkWidget *widget;
	GtkStyleContext *context;
	gint n_targets;

	button->priv = E_ATTACHMENT_BUTTON_GET_PRIVATE (button);

	/* Configure Widgets */

	container = GTK_WIDGET (button);
	context = gtk_widget_get_style_context (container);
	gtk_style_context_add_class (context, "linked");

	widget = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	button->priv->expand_button = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		button, "expandable",
		widget, "sensitive",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	widget = gtk_toggle_button_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	button->priv->toggle_button = g_object_ref (widget);
	gtk_widget_show (widget);

	container = button->priv->expand_button;

	widget = gtk_cell_view_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	button->priv->cell_view = g_object_ref (widget);
	gtk_widget_show (widget);

	container = button->priv->toggle_button;

	widget = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	/* Configure Renderers */

	cell_layout = GTK_CELL_LAYOUT (button->priv->cell_view);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "is-expander", TRUE, NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, FALSE);

	e_binding_bind_property (
		button, "expanded",
		renderer, "is-expanded",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_BUTTON, NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, FALSE);

	gtk_cell_layout_add_attribute (
		cell_layout, renderer, "gicon",
		E_ATTACHMENT_STORE_COLUMN_ICON);

	/* Configure Drag and Drop */

	list = gtk_target_list_new (NULL, 0);
	gtk_target_list_add_uri_targets (list, 0);
	targets = gtk_target_table_new_from_list (list, &n_targets);

	gtk_drag_source_set (
		button->priv->expand_button, GDK_BUTTON1_MASK,
		targets, n_targets, GDK_ACTION_COPY);

	gtk_drag_source_set (
		button->priv->toggle_button, GDK_BUTTON1_MASK,
		targets, n_targets, GDK_ACTION_COPY);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);

	/* Configure Signal Handlers */

	g_signal_connect_swapped (
		button->priv->expand_button, "clicked",
		G_CALLBACK (attachment_button_expand_clicked_cb), button);

	g_signal_connect_swapped (
		button->priv->expand_button, "drag-begin",
		G_CALLBACK (attachment_button_expand_drag_begin_cb),
		button);

	g_signal_connect_swapped (
		button->priv->expand_button, "drag-data-get",
		G_CALLBACK (attachment_button_expand_drag_data_get_cb),
		button);

	g_signal_connect_swapped (
		button->priv->expand_button, "drag-end",
		G_CALLBACK (attachment_button_expand_drag_end_cb),
		button);

	g_signal_connect_swapped (
		button->priv->toggle_button, "button-press-event",
		G_CALLBACK (attachment_button_toggle_button_press_event_cb),
		button);

	g_signal_connect_swapped (
		button->priv->toggle_button, "drag-begin",
		G_CALLBACK (attachment_button_expand_drag_begin_cb),
		button);

	g_signal_connect_swapped (
		button->priv->toggle_button, "drag-data-get",
		G_CALLBACK (attachment_button_expand_drag_data_get_cb),
		button);

	g_signal_connect_swapped (
		button->priv->toggle_button, "drag-end",
		G_CALLBACK (attachment_button_expand_drag_end_cb),
		button);
}

GtkWidget *
e_attachment_button_new (void)
{
	return g_object_new (
		E_TYPE_ATTACHMENT_BUTTON, NULL);
}

EAttachmentView *
e_attachment_button_get_view (EAttachmentButton *button)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BUTTON (button), NULL);

	return button->priv->view;
}

void
e_attachment_button_set_view (EAttachmentButton *button,
                              EAttachmentView *view)
{
	GtkWidget *popup_menu;

	g_return_if_fail (button->priv->view == NULL);

	g_object_ref (view);
	if (button->priv->view)
		g_object_unref (button->priv->view);
	button->priv->view = view;

	popup_menu = e_attachment_view_get_popup_menu (view);

	g_signal_connect_swapped (
		popup_menu, "deactivate",
		G_CALLBACK (attachment_button_menu_deactivate_cb), button);

	/* Keep a reference to the popup menu so we can
	 * disconnect the signal handler in dispose(). */
	if (button->priv->popup_menu)
		g_object_unref (button->priv->popup_menu);
	button->priv->popup_menu = g_object_ref (popup_menu);
}

EAttachment *
e_attachment_button_get_attachment (EAttachmentButton *button)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BUTTON (button), NULL);

	return button->priv->attachment;
}

void
e_attachment_button_set_attachment (EAttachmentButton *button,
                                    EAttachment *attachment)
{
	GtkTargetEntry *targets;
	GtkTargetList *list;
	gint n_targets;

	g_return_if_fail (E_IS_ATTACHMENT_BUTTON (button));

	if (attachment != NULL) {
		g_return_if_fail (E_IS_ATTACHMENT (attachment));
		g_object_ref (attachment);
	}

	if (button->priv->attachment != NULL) {
		g_clear_object (&button->priv->can_show_binding);
		g_clear_object (&button->priv->shown_binding);
		g_clear_object (&button->priv->zoom_to_window_binding);
		g_signal_handler_disconnect (
			button->priv->attachment,
			button->priv->reference_handler_id);
		g_object_unref (button->priv->attachment);
	}

	button->priv->attachment = attachment;

	if (attachment != NULL) {
		GBinding *binding;
		gulong handler_id;

		binding = e_binding_bind_property (
			attachment, "can-show",
			button, "expandable",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
		button->priv->can_show_binding = binding;

		binding = e_binding_bind_property (
			attachment, "shown",
			button, "expanded",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
		button->priv->shown_binding = binding;

		handler_id = e_signal_connect_notify_swapped (
			attachment, "notify::reference",
			G_CALLBACK (attachment_button_update_cell_view),
			button);
		button->priv->reference_handler_id = handler_id;

		binding = e_binding_bind_property (
			attachment, "zoom-to-window",
			button, "zoom-to-window",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
		button->priv->zoom_to_window_binding = binding;

		attachment_button_update_cell_view (button);
		attachment_button_update_pixbufs (button);
	}

	/* update drag sources */
	list = gtk_target_list_new (NULL, 0);
	gtk_target_list_add_uri_targets (list, 0);

	if (attachment != NULL) {
		gchar *simple_type;

		simple_type = e_attachment_dup_mime_type (attachment);
		if (simple_type != NULL) {
			GtkTargetEntry attach_entry[] = { { NULL, 0, 2 } };

			attach_entry[0].target = simple_type;

			gtk_target_list_add_table (
				list, attach_entry,
				G_N_ELEMENTS (attach_entry));

			g_free (simple_type);
		}
	}

	targets = gtk_target_table_new_from_list (list, &n_targets);

	gtk_drag_source_set (
		button->priv->expand_button, GDK_BUTTON1_MASK,
		targets, n_targets, GDK_ACTION_COPY);

	gtk_drag_source_set (
		button->priv->toggle_button, GDK_BUTTON1_MASK,
		targets, n_targets, GDK_ACTION_COPY);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);

	g_object_notify (G_OBJECT (button), "attachment");
}

gboolean
e_attachment_button_get_expandable (EAttachmentButton *button)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BUTTON (button), FALSE);

	return button->priv->expandable;
}

void
e_attachment_button_set_expandable (EAttachmentButton *button,
                                    gboolean expandable)
{
	g_return_if_fail (E_IS_ATTACHMENT_BUTTON (button));

	if (button->priv->expandable == expandable)
		return;

	button->priv->expandable = expandable;

	if (!expandable)
		e_attachment_button_set_expanded (button, FALSE);

	g_object_notify (G_OBJECT (button), "expandable");
}

gboolean
e_attachment_button_get_expanded (EAttachmentButton *button)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BUTTON (button), FALSE);

	return button->priv->expanded;
}

void
e_attachment_button_set_expanded (EAttachmentButton *button,
                                  gboolean expanded)
{
	g_return_if_fail (E_IS_ATTACHMENT_BUTTON (button));

	if (button->priv->expanded == expanded)
		return;

	button->priv->expanded = expanded;

	g_object_notify (G_OBJECT (button), "expanded");
}

gboolean
e_attachment_button_get_zoom_to_window (EAttachmentButton *button)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BUTTON (button), FALSE);

	return button->priv->zoom_to_window;
}

void
e_attachment_button_set_zoom_to_window (EAttachmentButton *button,
					gboolean zoom_to_window)
{
	g_return_if_fail (E_IS_ATTACHMENT_BUTTON (button));

	if ((button->priv->zoom_to_window ? 1 : 0) == (zoom_to_window ? 1 : 0))
		return;

	button->priv->zoom_to_window = zoom_to_window;

	g_object_notify (G_OBJECT (button), "zoom-to-window");
}
