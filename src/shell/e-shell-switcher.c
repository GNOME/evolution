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

#include "evolution-config.h"

#include "e-shell-switcher.h"

#include <glib/gi18n.h>
#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

#include "e-shell-window-private.h"

struct _EShellSwitcher {
	GtkBin parent;

	gint button_width;
	gint menu_button_width;

	gint n_buttons;
	gint n_visible_buttons;

	GSList *view_buttons;

	GtkWidget *box;
	GtkWidget *menu_button;
	GtkWidget *popover;
	GtkWidget *popover_box;
};

G_DEFINE_TYPE (EShellSwitcher, e_shell_switcher, GTK_TYPE_BIN);

static void e_shell_switcher_update_button_width (EShellSwitcher *self);
static void e_shell_switcher_show_buttons (EShellSwitcher *self);
static void e_shell_switcher_dispose (GObject *object);
static void e_shell_switcher_get_preferred_width (GtkWidget *widget, gint *minimum, gint *natural);
static void e_shell_switcher_size_allocate (GtkWidget *widget, GtkAllocation *alloc);

static void
e_shell_switcher_update_button_width (EShellSwitcher *self)
{
	g_return_if_fail (E_IS_SHELL_SWITCHER (self));

	gint new_button_width = 0;
	for (GSList *iter = self->view_buttons; iter; iter = iter->next) {
		gint button_width;

		/* It is important to call `gtk_widget_show' to ensure that the button
		 * has been realized before we attempt to get the button width.
		 */
		gtk_widget_show (iter->data);
		gtk_widget_get_preferred_width (iter->data, &button_width, NULL);

		new_button_width = MAX (button_width, new_button_width);
	}

	self->button_width = new_button_width;

	for (GSList *iter = self->view_buttons; iter; iter = iter->next)
		gtk_widget_set_size_request (iter->data, self->button_width, -1);
}

static void
remove_all_container_children (GtkContainer *container)
{
	GList *children = gtk_container_get_children (container);
	for (GList *iter = children; iter; iter = iter->next)
		gtk_container_remove (container, iter->data);
	g_list_free (children);
}

static void
e_shell_switcher_show_buttons (EShellSwitcher *self)
{
	g_return_if_fail (E_IS_SHELL_SWITCHER (self));

	remove_all_container_children (GTK_CONTAINER (self->box));
	remove_all_container_children (GTK_CONTAINER (self->popover_box));

	gboolean active_visible = FALSE;
	GSList *iter = self->view_buttons;

	/* The first (n - 1) buttons are always shown, so show them. */
	for (gint i = 0; i < (self->n_visible_buttons - 1); i++, iter = iter->next) {
		GtkWidget *button = iter->data;
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
			active_visible = TRUE;
		gtk_box_pack_start (GTK_BOX (self->box), button, FALSE, FALSE, 0);
	}

	/* Only one button is active.  If that button is already visible, then we
	 * pack the next button to fill the visible slot and pack the rest to fill
	 * the popover box.
	 *
	 * If the active button is not yet visible, then we iterate over the remaining
	 * buttons and pack the active button to fill the visible slot and pack the
	 * rest to fill the popover box.
	 */
	if (active_visible) {
		GtkWidget *button = iter->data;
		iter = iter->next;

		gtk_box_pack_start (GTK_BOX (self->box), button, FALSE, FALSE, 0);

		for (gint i = self->n_visible_buttons; i < self->n_buttons; i++, iter = iter->next) {
			GtkWidget *button = iter->data;
			gtk_box_pack_start (GTK_BOX (self->popover_box), button, FALSE, FALSE, 0);
		}
	} else {
		for (gint i = self->n_visible_buttons - 1; i < self->n_buttons; i++, iter = iter->next) {
			GtkWidget *button = iter->data;
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
				gtk_box_pack_start (GTK_BOX (self->box), button, FALSE, FALSE, 0);
			else
				gtk_box_pack_start (GTK_BOX (self->popover_box), button, FALSE, FALSE, 0);
		}
	}

	if (self->n_visible_buttons < self->n_buttons)
		gtk_box_pack_start (GTK_BOX (self->box), self->menu_button, FALSE, FALSE, 0);

	gtk_widget_show_all (self->box);
	gtk_widget_show_all (self->popover_box);
}

static void
e_shell_switcher_dispose (GObject *object)
{
	EShellSwitcher *self = E_SHELL_SWITCHER (object);

	g_clear_pointer (&self->menu_button, gtk_widget_unparent);
	g_clear_pointer (&self->popover, gtk_widget_unparent);

	G_OBJECT_CLASS (e_shell_switcher_parent_class)->dispose (object);
}

static void
e_shell_switcher_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
	EShellSwitcher *self = E_SHELL_SWITCHER (widget);

	if (self->n_buttons > 1)
		*minimum = self->button_width + self->menu_button_width;
	else
		*minimum = self->button_width;
	*natural = self->button_width * self->n_buttons;
}

static void
e_shell_switcher_size_allocate (GtkWidget     *widget,
                              GtkAllocation *alloc)
{
	EShellSwitcher *self = E_SHELL_SWITCHER (widget);

	if (self->button_width == 0)
		goto chain_up;

	gint n_visible_buttons = alloc->width / self->button_width;
	n_visible_buttons = MIN (self->n_buttons, n_visible_buttons);

	if (n_visible_buttons < self->n_buttons)
		n_visible_buttons = (alloc->width - self->menu_button_width) / self->button_width;

	if (n_visible_buttons != self->n_visible_buttons) {
		self->n_visible_buttons = n_visible_buttons;
		e_shell_switcher_show_buttons (self);
	}

chain_up:
	GTK_WIDGET_CLASS (e_shell_switcher_parent_class)->size_allocate (widget, alloc);
}
static void
e_shell_switcher_class_init (EShellSwitcherClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	object_class->dispose = e_shell_switcher_dispose;

	widget_class->get_preferred_width = e_shell_switcher_get_preferred_width;
	widget_class->size_allocate = e_shell_switcher_size_allocate;
}

static void
e_shell_switcher_init (EShellSwitcher *self)
{
	GtkStyleContext *context;

	self->button_width = 0;
	self->menu_button_width = 0;

	self->n_buttons = 0;
	self->n_visible_buttons = 0;

	self->view_buttons = NULL;

	self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_halign (self->box, GTK_ALIGN_CENTER);
	context = gtk_widget_get_style_context (self->box);
	gtk_style_context_add_class (context, "linked");

	self->menu_button = gtk_menu_button_new ();
	gtk_widget_set_halign (self->menu_button, GTK_ALIGN_START);

	/* It is important to call `gtk_widget_show' to ensure that the button
	 * has been realized before we attempt to get the button width.
	 */
	gtk_widget_show (self->menu_button);
	gtk_widget_get_preferred_width (self->menu_button, &self->menu_button_width, NULL);

	/* When we clear self->box in e_shell_switcher_show_buttons (), all of the
	 * buttons in the container will be unreferenced and thus finalized.
	 * Therefore, we need to hold an extra reference to all of the buttons in
	 * self->box.
	 */
	g_object_ref (self->menu_button);

	self->popover = gtk_popover_new (self->menu_button);
	gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (self->menu_button), TRUE);
	gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->menu_button), self->popover);

	self->popover_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	context = gtk_widget_get_style_context (self->popover_box);
	gtk_style_context_add_class (context, "linked");

	gtk_container_add (GTK_CONTAINER (self->popover), self->popover_box);
	gtk_container_add (GTK_CONTAINER (self), self->box);
	gtk_widget_show_all (GTK_WIDGET (self));
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

/**
 * e_shell_switcher_switch_to_view:
 * @self: an #EShellSwitcher
 * @action: a #GtkAction
 *
 * Toggles the button associated with the action.
 **/
void
e_shell_switcher_switch_to_view (EShellSwitcher *self,
                                 const gchar *view_name)
{
	g_return_if_fail (E_IS_SHELL_SWITCHER (self));

	for (GSList *iter = self->view_buttons; iter; iter = iter->next) {
		GtkAction *button_action = g_object_get_data (G_OBJECT (iter->data), "action");
		gchar *button_view_name = g_object_get_data (G_OBJECT (button_action), "view-name");
		if (g_strcmp0 (view_name, button_view_name) == 0) {
			gtk_action_activate (button_action);
			gtk_toggle_button_set_active (iter->data, TRUE);
		}
	}
}

static gboolean
switch_to_view (GtkWidget *button,
		GdkEvent  *event,
		GtkAction *action)
{
	g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

	if (!gtk_action_is_sensitive (action))
		return GDK_EVENT_PROPAGATE;

	guint event_button;
	GdkModifierType event_state;

	gdk_event_get_button (event, &event_button);
	gdk_event_get_state (event, &event_state);

	GdkModifierType new_window_mods = GDK_SHIFT_MASK | GDK_CONTROL_MASK;
	if (event_button == GDK_BUTTON_MIDDLE ||
	    (event_button == GDK_BUTTON_PRIMARY && (event_state & new_window_mods))) {
		gchar *view_name = g_object_get_data (G_OBJECT (action), "view-name");
		view_name = g_strconcat ("*", view_name, NULL);

		GtkWidget *window = gtk_widget_get_toplevel (button);
		g_return_val_if_fail (E_IS_SHELL_WINDOW (window), GDK_EVENT_PROPAGATE);

		EShell *shell = e_shell_window_get_shell (E_SHELL_WINDOW (window));
		e_shell_create_shell_window (shell, view_name);
	} else {
		gtk_action_activate (action);
	}

	return GDK_EVENT_PROPAGATE;
}

static void
update_buttons (GtkButton      *button,
		EShellSwitcher *switcher)
{
	e_shell_switcher_show_buttons (switcher);
}

/**
 * e_shell_switcher_add_action:
 * @self: an #EShellSwitcher
 * @action: a #GtkAction
 *
 * Adds a button to @self that proxies for @action.
 * Switcher buttons appear in the order they were added.
 *
 * #EShellWindow adds switcher actions in the order given by the
 * <structfield>sort_order</structfield> field in #EShellBackendClass.
 **/
void
e_shell_switcher_add_action (EShellSwitcher *self,
                             GtkAction      *action)
{
	g_return_if_fail (E_IS_SHELL_SWITCHER (self));
	g_return_if_fail (GTK_IS_ACTION (action));

	GSettings *settings = e_util_ref_settings ("org.gnome.evolution.shell");
	gchar **strv = g_settings_get_strv (settings, "buttons-hide");
	g_clear_object (&settings);

	gboolean skip = FALSE;
	for (gint i = 0; strv && strv[i] && !skip; i++) {
		gchar *name = g_strdup_printf (E_SHELL_SWITCHER_FORMAT, strv[i]);
		skip = g_strcmp0 (name, gtk_action_get_name (action)) == 0;
		g_free (name);
	}

	g_strfreev (strv);

	if (skip)
		return;

	const gchar *action_name = gtk_action_get_label (action);
	GtkWidget *button = gtk_radio_button_new_with_label (self->view_buttons, action_name);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);

	g_object_set_data (G_OBJECT (button), "action", action);
	g_signal_connect (button, "button-release-event", G_CALLBACK (switch_to_view), action);
	g_signal_connect (button, "toggled", G_CALLBACK (update_buttons), self);

	g_object_ref (action);
	g_object_ref (button);

	self->n_buttons++;
	self->n_visible_buttons++;
	self->view_buttons = g_slist_append (self->view_buttons, button);

	e_shell_switcher_update_button_width (self);
	e_shell_switcher_show_buttons (self);
}
