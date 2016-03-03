/*
 * e-shell-switcher.c
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

/**
 * SECTION: e-shell-switcher
 * @short_description: buttons for switching views
 * @include: shell/e-shell-switcher.h
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-switcher.h"

#include <glib/gi18n.h>
#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

#include "e-shell-window-private.h"

#define E_SHELL_SWITCHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_SWITCHER, EShellSwitcherPrivate))

#define E_SHELL_SWITCHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_SWITCHER, EShellSwitcherPrivate))

#define H_PADDING 6
#define V_PADDING 6

struct _EShellSwitcherPrivate {
	GList *proxies;
	gboolean style_set;
	GtkToolbarStyle style;
	GtkSettings *settings;
	gulong settings_handler_id;
	gboolean toolbar_visible;
	gint hpaned_handle_size;
};

enum {
	PROP_0,
	PROP_TOOLBAR_STYLE,
	PROP_TOOLBAR_VISIBLE
};

enum {
	STYLE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void shell_switcher_tool_shell_iface_init (GtkToolShellIface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EShellSwitcher,
	e_shell_switcher,
	GTK_TYPE_BIN,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (
		GTK_TYPE_TOOL_SHELL,
		shell_switcher_tool_shell_iface_init))

static gint
shell_switcher_layout_actions (EShellSwitcher *switcher)
{
	GtkAllocation allocation;
	gint num_btns = g_list_length (switcher->priv->proxies), btns_per_row;
	GList **rows, *p;
	gboolean icons_only;
	gint row_number;
	gint max_width = 0;
	gint max_height = 0;
	gint row_last;
	gint x, y;
	gint i;

	gtk_widget_get_allocation (GTK_WIDGET (switcher), &allocation);

	y = allocation.y + allocation.height;

	if (num_btns == 0)
		return allocation.height;

	icons_only = (switcher->priv->style == GTK_TOOLBAR_ICONS);

	/* Figure out the max width and height. */
	for (p = switcher->priv->proxies; p != NULL; p = p->next) {
		GtkWidget *widget = p->data;
		GtkRequisition requisition;

		gtk_widget_get_preferred_size (widget, &requisition, NULL);
		max_height = MAX (max_height, requisition.height);
		max_width = MAX (max_width, requisition.width);
	}

	/* Figure out how many rows and columns we'll use. */
	btns_per_row = MAX (1, (allocation.width - 1) / (max_width + H_PADDING + H_PADDING - switcher->priv->hpaned_handle_size));
	if (!icons_only) {
		/* If using text buttons, we want to try to have a
		 * completely filled-in grid, but if we can't, we want
		 * the odd row to have just a single button. */
		while (btns_per_row > 0 && num_btns % btns_per_row > 1)
			btns_per_row--;
	}

	/* Assign buttons to rows. */
	rows = g_new0 (GList *, num_btns / btns_per_row + 1);

	if (!icons_only && num_btns % btns_per_row != 0) {
		rows[0] = g_list_append (rows[0], switcher->priv->proxies->data);

		p = switcher->priv->proxies->next;
		row_number = p ? 1 : 0;
	} else {
		p = switcher->priv->proxies;
		row_number = 0;
	}

	for (; p != NULL; p = p->next) {
		GtkWidget *widget = p->data;

		if (g_list_length (rows[row_number]) == btns_per_row)
			row_number++;

		rows[row_number] = g_list_append (rows[row_number], widget);
	}

	row_last = row_number;

	/* Layout the buttons. */
	for (i = row_last; i >= 0; i--) {
		gint len, extra_width, left_width;

		left_width = allocation.width - 1;
		x = H_PADDING + allocation.x;
		y -= max_height + V_PADDING;
		len = g_list_length (rows[i]);
		for (p = rows[i]; p != NULL; p = p->next, len--) {
			GtkAllocation child_allocation;

			if (!icons_only)
				extra_width =
					(left_width - (len * max_width) -
					(len * H_PADDING + H_PADDING - switcher->priv->hpaned_handle_size)) / len;
			else
				extra_width = 0;

			child_allocation.x = x;
			child_allocation.y = y;
			child_allocation.width = max_width + extra_width;
			child_allocation.height = max_height;

			gtk_widget_size_allocate (
				GTK_WIDGET (p->data), &child_allocation);

			x += child_allocation.width + H_PADDING;
			left_width = left_width - child_allocation.width - H_PADDING;
		}
	}

	for (i = 0; i <= row_last; i++)
		g_list_free (rows[i]);
	g_free (rows);

	return y - allocation.y - V_PADDING;
}

static void
shell_switcher_toolbar_style_changed_cb (EShellSwitcher *switcher)
{
	if (!switcher->priv->style_set) {
		switcher->priv->style_set = TRUE;
		e_shell_switcher_unset_style (switcher);
	}
}

static void
shell_switcher_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TOOLBAR_STYLE:
			e_shell_switcher_set_style (
				E_SHELL_SWITCHER (object),
				g_value_get_enum (value));
			return;

		case PROP_TOOLBAR_VISIBLE:
			e_shell_switcher_set_visible (
				E_SHELL_SWITCHER (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_switcher_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TOOLBAR_STYLE:
			g_value_set_enum (
				value, e_shell_switcher_get_style (
				E_SHELL_SWITCHER (object)));
			return;

		case PROP_TOOLBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_switcher_get_visible (
				E_SHELL_SWITCHER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_switcher_dispose (GObject *object)
{
	EShellSwitcherPrivate *priv;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (object);

	while (priv->proxies != NULL) {
		GtkWidget *widget = priv->proxies->data;
		gtk_container_remove (GTK_CONTAINER (object), widget);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_shell_switcher_parent_class)->dispose (object);
}

static void
shell_switcher_get_preferred_width (GtkWidget *widget,
                                    gint *minimum,
                                    gint *natural)
{
	EShellSwitcherPrivate *priv;
	GtkWidget *child;
	GList *iter;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (widget);

	*minimum = *natural = 0;

	child = gtk_bin_get_child (GTK_BIN (widget));
	if (child != NULL)
		gtk_widget_get_preferred_width (child, minimum, natural);

	if (!priv->toolbar_visible)
		return;

	for (iter = priv->proxies; iter != NULL; iter = iter->next) {
		GtkWidget *widget_proxy = iter->data;
		gint child_min, child_nat;

		gtk_widget_get_preferred_width (
			widget_proxy, &child_min, &child_nat);

		child_min += H_PADDING;
		child_nat += H_PADDING;

		*minimum = MAX (*minimum, child_min);
		*natural = MAX (*natural, child_nat);
	}
}

static void
shell_switcher_get_preferred_height (GtkWidget *widget,
                                     gint *minimum,
                                     gint *natural)
{
	EShellSwitcherPrivate *priv;
	GtkWidget *child;
	GList *iter;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (widget);

	*minimum = *natural = 0;

	child = gtk_bin_get_child (GTK_BIN (widget));
	if (child != NULL)
		gtk_widget_get_preferred_height (child, minimum, natural);

	if (!priv->toolbar_visible)
		return;

	for (iter = priv->proxies; iter != NULL; iter = iter->next) {
		GtkWidget *widget = iter->data;
		gint child_min, child_nat;

		gtk_widget_get_preferred_height (
			widget, &child_min, &child_nat);

		child_min += V_PADDING;
		child_nat += V_PADDING;

		*minimum += child_min;
		*natural += child_nat;
	}
}

static void
shell_switcher_size_allocate (GtkWidget *widget,
                              GtkAllocation *allocation)
{
	EShellSwitcher *switcher;
	GtkAllocation child_allocation;
	GtkWidget *child;
	gint height;

	switcher = E_SHELL_SWITCHER (widget);

	gtk_widget_set_allocation (widget, allocation);

	if (switcher->priv->toolbar_visible)
		height = shell_switcher_layout_actions (switcher);
	else
		height = allocation->height;

	child_allocation.x = allocation->x;
	child_allocation.y = allocation->y;
	child_allocation.width = allocation->width;
	child_allocation.height = height;

	child = gtk_bin_get_child (GTK_BIN (widget));
	if (child != NULL)
		gtk_widget_size_allocate (child, &child_allocation);
}

static void
shell_switcher_screen_changed (GtkWidget *widget,
                               GdkScreen *previous_screen)
{
	EShellSwitcherPrivate *priv;
	GtkSettings *settings;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (widget);

	if (gtk_widget_has_screen (widget))
		settings = gtk_widget_get_settings (widget);
	else
		settings = NULL;

	if (settings == priv->settings)
		return;

	if (priv->settings != NULL) {
		g_signal_handler_disconnect (
			priv->settings, priv->settings_handler_id);
		g_object_unref (priv->settings);
	}

	if (settings != NULL) {
		priv->settings = g_object_ref (settings);
		priv->settings_handler_id = e_signal_connect_notify_swapped (
			settings, "notify::gtk-toolbar-style",
			G_CALLBACK (shell_switcher_toolbar_style_changed_cb),
			widget);
	} else
		priv->settings = NULL;

	shell_switcher_toolbar_style_changed_cb (E_SHELL_SWITCHER (widget));
}

static void
shell_switcher_remove (GtkContainer *container,
                       GtkWidget *widget)
{
	EShellSwitcherPrivate *priv;
	GList *link;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (container);

	/* Look in the internal widgets first. */

	link = g_list_find (priv->proxies, widget);
	if (link != NULL) {
		GtkWidget *widget = link->data;

		gtk_widget_unparent (widget);
		priv->proxies = g_list_delete_link (priv->proxies, link);
		gtk_widget_queue_resize (GTK_WIDGET (container));
		return;
	}

	/* Chain up to parent's remove() method. */
	GTK_CONTAINER_CLASS (e_shell_switcher_parent_class)->remove (
		container, widget);
}

static void
shell_switcher_forall (GtkContainer *container,
                       gboolean include_internals,
                       GtkCallback callback,
                       gpointer callback_data)
{
	EShellSwitcherPrivate *priv;

	priv = E_SHELL_SWITCHER_GET_PRIVATE (container);

	if (include_internals)
		g_list_foreach (
			priv->proxies, (GFunc) callback, callback_data);

	/* Chain up to parent's forall() method. */
	GTK_CONTAINER_CLASS (e_shell_switcher_parent_class)->forall (
		container, include_internals, callback, callback_data);
}

static void
shell_switcher_style_changed (EShellSwitcher *switcher,
                              GtkToolbarStyle style)
{
	if (switcher->priv->style == style)
		return;

	switcher->priv->style = style;

	g_list_foreach (
		switcher->priv->proxies,
		(GFunc) gtk_tool_item_toolbar_reconfigured, NULL);

	gtk_widget_queue_resize (GTK_WIDGET (switcher));
	g_object_notify (G_OBJECT (switcher), "toolbar-style");
}

static void
shell_switcher_read_handle_size (EShellSwitcher *switcher)
{
	GtkWidget *paned;

	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_style_get (paned, "handle-size", &switcher->priv->hpaned_handle_size, NULL);
	gtk_widget_destroy (paned);

	if (switcher->priv->hpaned_handle_size < 0)
		switcher->priv->hpaned_handle_size = 0;
	else if (switcher->priv->hpaned_handle_size > H_PADDING)
		switcher->priv->hpaned_handle_size = H_PADDING;
}

static void
shell_switcher_style_updated_cb (EShellSwitcher *switcher)
{
	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	shell_switcher_read_handle_size (switcher);
}

static GtkIconSize
shell_switcher_get_icon_size (GtkToolShell *shell)
{
	return GTK_ICON_SIZE_LARGE_TOOLBAR;
}

static GtkOrientation
shell_switcher_get_orientation (GtkToolShell *shell)
{
	return GTK_ORIENTATION_HORIZONTAL;
}

static GtkToolbarStyle
shell_switcher_get_style (GtkToolShell *shell)
{
	return e_shell_switcher_get_style (E_SHELL_SWITCHER (shell));
}

static GtkReliefStyle
shell_switcher_get_relief_style (GtkToolShell *shell)
{
	return GTK_RELIEF_NORMAL;
}

static gfloat
shell_switcher_get_text_alignment (GtkToolShell *shell)
{
	return 0.0;
}

static void
e_shell_switcher_class_init (EShellSwitcherClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	g_type_class_add_private (class, sizeof (EShellSwitcherPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_switcher_set_property;
	object_class->get_property = shell_switcher_get_property;
	object_class->dispose = shell_switcher_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = shell_switcher_get_preferred_width;
	widget_class->get_preferred_height = shell_switcher_get_preferred_height;
	widget_class->size_allocate = shell_switcher_size_allocate;
	widget_class->screen_changed = shell_switcher_screen_changed;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->remove = shell_switcher_remove;
	container_class->forall = shell_switcher_forall;

	class->style_changed = shell_switcher_style_changed;

	/**
	 * EShellSwitcher:toolbar-style
	 *
	 * The switcher's toolbar style.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_TOOLBAR_STYLE,
		g_param_spec_enum (
			"toolbar-style",
			"Toolbar Style",
			"The switcher's toolbar style",
			GTK_TYPE_TOOLBAR_STYLE,
			E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellSwitcher:toolbar-visible
	 *
	 * Whether the switcher is visible.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_TOOLBAR_VISIBLE,
		g_param_spec_boolean (
			"toolbar-visible",
			"Toolbar Visible",
			"Whether the switcher is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellSwitcher::style-changed
	 * @switcher: the #EShellSwitcher which emitted the signal
	 * @style: the new #GtkToolbarStyle of the switcher
	 *
	 * Emitted when the style of the switcher changes.
	 **/
	signals[STYLE_CHANGED] = g_signal_new (
		"style-changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellSwitcherClass, style_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__ENUM,
		G_TYPE_NONE, 1,
		GTK_TYPE_TOOLBAR_STYLE);
}

static void
e_shell_switcher_init (EShellSwitcher *switcher)
{
	switcher->priv = E_SHELL_SWITCHER_GET_PRIVATE (switcher);

	gtk_widget_set_has_window (GTK_WIDGET (switcher), FALSE);

	e_extensible_load_extensions (E_EXTENSIBLE (switcher));

	switcher->priv->hpaned_handle_size = 5;

	shell_switcher_read_handle_size (switcher);

	g_signal_connect (switcher, "style-updated", G_CALLBACK (shell_switcher_style_updated_cb), NULL);
}

static void
shell_switcher_tool_shell_iface_init (GtkToolShellIface *iface)
{
	iface->get_icon_size = shell_switcher_get_icon_size;
	iface->get_orientation = shell_switcher_get_orientation;
	iface->get_style = shell_switcher_get_style;
	iface->get_relief_style = shell_switcher_get_relief_style;
	iface->get_text_alignment = shell_switcher_get_text_alignment;
}

/**
 * e_shell_switcher_new:
 *
 * Creates a new #EShellSwitcher instance.
 *
 * Returns: a new #EShellSwitcher instance
 **/
GtkWidget *
e_shell_switcher_new (void)
{
	return g_object_new (E_TYPE_SHELL_SWITCHER, NULL);
}

/*
 * gtk+ doesn't give us what we want - a middle click,
 * option on toolbar items, so we have to get it by force.
 */
static GtkButton *
tool_item_get_button (GtkWidget *widget)
{
	GtkWidget *child;

	g_return_val_if_fail (GTK_IS_TOOL_ITEM (widget), NULL);

	child = gtk_bin_get_child (GTK_BIN (widget));
	if (child != NULL && GTK_IS_BUTTON (child))
		return GTK_BUTTON (child);
	else
		return NULL;
}

static gboolean
tool_item_button_cb (GtkWidget *internal_widget,
                     GdkEvent *button_event,
                     GtkAction *action)
{
	guint event_button = 0;

	g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

	gdk_event_get_button (button_event, &event_button);

	if (event_button == 2) {
		gtk_action_activate (action);
		return TRUE;
	}

	return FALSE;
}

/**
 * e_shell_switcher_add_action:
 * @switcher: an #EShellSwitcher
 * @switch_action: a #GtkAction
 * @new_window_action: a #GtkAction
 *
 * Adds a button to @switcher that proxies for @switcher_action.
 * Switcher buttons appear in the order they were added. A middle
 * click opens a new window of this type.
 *
 * #EShellWindow adds switcher actions in the order given by the
 * <structfield>sort_order</structfield> field in #EShellBackendClass.
 **/
void
e_shell_switcher_add_action (EShellSwitcher *switcher,
                             GtkAction *switch_action,
                             GtkAction *new_window_action)
{
	GtkWidget *widget;
	GtkButton *button;
	GSettings *settings;
	gchar **strv;
	gint ii;
	gboolean skip = FALSE;

	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));
	g_return_if_fail (GTK_IS_ACTION (switch_action));
	g_return_if_fail (GTK_IS_ACTION (new_window_action));

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	strv = g_settings_get_strv (settings, "buttons-hide");
	g_clear_object (&settings);

	for (ii = 0; strv && strv[ii] && !skip; ii++) {
		gchar *name;

		name = g_strdup_printf (E_SHELL_SWITCHER_FORMAT, strv[ii]);
		skip = g_strcmp0 (name, gtk_action_get_name (switch_action)) == 0;
		g_free (name);
	}

	g_strfreev (strv);

	if (skip)
		return;

	g_object_ref (switch_action);
	widget = gtk_action_create_tool_item (switch_action);
	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (widget), TRUE);
	gtk_widget_show (widget);

	button = tool_item_get_button (widget);
	if (button != NULL)
		g_signal_connect (
			button, "button-release-event",
			G_CALLBACK (tool_item_button_cb),
			new_window_action);

	switcher->priv->proxies = g_list_append (
		switcher->priv->proxies, widget);

	gtk_widget_set_parent (widget, GTK_WIDGET (switcher));
	gtk_widget_queue_resize (GTK_WIDGET (switcher));
}

/**
 * e_shell_switcher_get_style:
 * @switcher: an #EShellSwitcher
 *
 * Returns whether @switcher has text, icons or both.
 *
 * Returns: the current style of @shell
 **/
GtkToolbarStyle
e_shell_switcher_get_style (EShellSwitcher *switcher)
{
	g_return_val_if_fail (
		E_IS_SHELL_SWITCHER (switcher),
		E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE);

	return switcher->priv->style;
}

/**
 * e_shell_switcher_set_style:
 * @switcher: an #EShellSwitcher
 * @style: the new style for @switcher
 *
 * Alters the view of @switcher to display either icons only, text only,
 * or both.
 **/
void
e_shell_switcher_set_style (EShellSwitcher *switcher,
                            GtkToolbarStyle style)
{
	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	switcher->priv->style_set = TRUE;
	g_signal_emit (switcher, signals[STYLE_CHANGED], 0, style);
}

/**
 * e_shell_switcher_unset_style:
 * @switcher: an #EShellSwitcher
 *
 * Unsets a switcher style set with e_shell_switcher_set_style(), so
 * that user preferences will be used to determine the switcher style.
 **/
void
e_shell_switcher_unset_style (EShellSwitcher *switcher)
{
	GtkSettings *settings;
	GtkToolbarStyle style;

	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	if (!switcher->priv->style_set)
		return;

	settings = switcher->priv->settings;
	if (settings != NULL)
		g_object_get (settings, "gtk-toolbar-style", &style, NULL);
	else
		style = E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE;

	if (style == GTK_TOOLBAR_BOTH)
		style = GTK_TOOLBAR_BOTH_HORIZ;

	if (style != switcher->priv->style)
		g_signal_emit (switcher, signals[STYLE_CHANGED], 0, style);

	switcher->priv->style_set = FALSE;
}

/**
 * e_shell_switcher_get_visible:
 * @switcher: an #EShellSwitcher
 *
 * Returns %TRUE if the switcher buttons are visible.
 *
 * Note that switcher button visibility is different than
 * @switcher<!-- -->'s GTK_VISIBLE flag, since #EShellSwitcher
 * is actually a container widget for #EShellSidebar.
 *
 * Returns: %TRUE if the switcher buttons are visible
 **/
gboolean
e_shell_switcher_get_visible (EShellSwitcher *switcher)
{
	g_return_val_if_fail (E_IS_SHELL_SWITCHER (switcher), FALSE);

	return switcher->priv->toolbar_visible;
}

/**
 * e_shell_switcher_set_visible:
 * @switcher: an #EShellSwitcher
 * @visible: whether the switcher buttons should be visible
 *
 * Sets the switcher button visiblity to @visible.
 *
 * Note that switcher button visibility is different than
 * @switcher<!-- -->'s GTK_VISIBLE flag, since #EShellSwitcher
 * is actually a container widget for #EShellSidebar.
 **/
void
e_shell_switcher_set_visible (EShellSwitcher *switcher,
                              gboolean visible)
{
	GList *iter;

	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	if (switcher->priv->toolbar_visible == visible)
		return;

	switcher->priv->toolbar_visible = visible;

	for (iter = switcher->priv->proxies; iter != NULL; iter = iter->next)
		g_object_set (iter->data, "visible", visible, NULL);

	gtk_widget_queue_resize (GTK_WIDGET (switcher));

	g_object_notify (G_OBJECT (switcher), "toolbar-visible");
}
