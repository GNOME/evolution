/*
 * e-emoticon-tool-button.c
 *
 * Copyright (C) 2008 Novell, Inc.
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "evolution-config.h"

#include "e-emoticon-tool-button.h"

/* XXX The "button" aspects of this widget are based heavily on the
 *     GtkComboBox tree-view implementation.  Consider splitting it
 *     into a reusable "button-with-an-empty-window" widget. */

#include <string.h>
#include <glib/gi18n-lib.h>

#include "e-emoticon-chooser.h"

/* XXX Should calculate this dynamically. */
#define NUM_ROWS	7
#define NUM_COLS	3

enum {
	PROP_0,
	PROP_CURRENT_EMOTICON,
	PROP_POPUP_SHOWN
};

enum {
	POPUP,
	POPDOWN,
	LAST_SIGNAL
};

struct _EEmoticonToolButtonPrivate {
	GtkWidget *active_button;  /* not referenced */
	GtkWidget *table;
	GtkWidget *popover;

	guint popup_shown	: 1;
	guint popup_in_progress	: 1;
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_emoticon_tool_button_interface_init
					(EEmoticonChooserInterface *interface);

G_DEFINE_TYPE_WITH_CODE (EEmoticonToolButton, e_emoticon_tool_button, GTK_TYPE_TOGGLE_TOOL_BUTTON,
	G_ADD_PRIVATE (EEmoticonToolButton)
	G_IMPLEMENT_INTERFACE (E_TYPE_EMOTICON_CHOOSER, e_emoticon_tool_button_interface_init))

/* XXX Copied from _gtk_toolbar_elide_underscores() */
static gchar *
emoticon_tool_button_elide_underscores (const gchar *original)
{
	gchar *q, *result;
	const gchar *p, *end;
	gsize len;
	gboolean last_underscore;

	if (!original)
		return NULL;

	len = strlen (original);
	q = result = g_malloc (len + 1);
	last_underscore = FALSE;

	end = original + len;
	for (p = original; p < end; p++) {
		if (!last_underscore && *p == '_')
			last_underscore = TRUE;
		else {
			last_underscore = FALSE;
			if (original + 2 <= p && p + 1 <= end &&
				p[-2] == '(' && p[-1] == '_' &&
				p[0] != '_' && p[1] == ')') {
				q--;
				*q = '\0';
				p++;
			} else
				*q++ = *p;
		}
	}

	if (last_underscore)
		*q++ = '_';

	*q = '\0';

	return result;
}

static void
emoticon_tool_button_emoticon_clicked_cb (EEmoticonToolButton *button,
                                          GtkWidget *emoticon_button)
{
	button->priv->active_button = emoticon_button;
	e_emoticon_tool_button_popdown (button);
}

static gboolean
emoticon_tool_button_emoticon_release_event_cb (EEmoticonToolButton *button,
                                                GdkEventButton *event,
                                                GtkButton *emoticon_button)
{
	GtkStateFlags state;

	state = gtk_widget_get_state_flags (GTK_WIDGET (button));

	if (state != GTK_STATE_FLAG_NORMAL)
		gtk_button_clicked (emoticon_button);

	return FALSE;
}

static gboolean
emoticon_tool_button_button_release_event_cb (EEmoticonToolButton *button,
                                              GdkEventButton *event)
{
	GtkToggleToolButton *tool_button;
	GtkWidget *event_widget;
	gboolean popup_in_progress;

	tool_button = GTK_TOGGLE_TOOL_BUTTON (button);
	event_widget = gtk_get_event_widget ((GdkEvent *) event);

	popup_in_progress = button->priv->popup_in_progress;
	button->priv->popup_in_progress = FALSE;

	if (event_widget != GTK_WIDGET (button))
		goto popdown;

	if (popup_in_progress)
		return FALSE;

	if (gtk_toggle_tool_button_get_active (tool_button))
		goto popdown;

	return FALSE;

popdown:
	e_emoticon_tool_button_popdown (button);

	return TRUE;
}

static void
emoticon_tool_button_child_show_cb (EEmoticonToolButton *button)
{
	button->priv->popup_shown = TRUE;
	g_object_notify (G_OBJECT (button), "popup-shown");
}

static void
emoticon_tool_button_child_hide_cb (EEmoticonToolButton *button)
{
	button->priv->popup_shown = FALSE;
	g_object_notify (G_OBJECT (button), "popup-shown");
}

static gboolean
emoticon_tool_button_child_key_press_event_cb (EEmoticonToolButton *button,
                                               GdkEventKey *event)
{
	GtkWidget *popover = button->priv->popover;

	return gtk_bindings_activate_event (G_OBJECT (popover), event) ||
		gtk_bindings_activate_event (G_OBJECT (button), event);
}

static void
emoticon_tool_button_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_EMOTICON:
			e_emoticon_chooser_set_current_emoticon (
				E_EMOTICON_CHOOSER (object),
				g_value_get_boxed (value));
			return;

		case PROP_POPUP_SHOWN:
			if (g_value_get_boolean (value))
				e_emoticon_tool_button_popup (
					E_EMOTICON_TOOL_BUTTON (object));
			else
				e_emoticon_tool_button_popdown (
					E_EMOTICON_TOOL_BUTTON (object));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
emoticon_tool_button_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	EEmoticonToolButton *self = E_EMOTICON_TOOL_BUTTON (object);

	switch (property_id) {
		case PROP_CURRENT_EMOTICON:
			g_value_set_boxed (
				value,
				e_emoticon_chooser_get_current_emoticon (
				E_EMOTICON_CHOOSER (object)));
			return;

		case PROP_POPUP_SHOWN:
			g_value_set_boolean (value, self->priv->popup_shown);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
emoticon_tool_button_dispose (GObject *object)
{
	EEmoticonToolButton *self = E_EMOTICON_TOOL_BUTTON (object);

	g_clear_pointer (&self->priv->popover, gtk_widget_destroy);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_emoticon_tool_button_parent_class)->dispose (object);
}

static gboolean
emoticon_tool_button_press_event (GtkWidget *widget,
                                  GdkEventButton *event)
{
	EEmoticonToolButton *button;
	GtkToggleToolButton *toggle_button;
	GtkWidget *event_widget;

	button = E_EMOTICON_TOOL_BUTTON (widget);

	event_widget = gtk_get_event_widget ((GdkEvent *) event);

	if (event_widget == button->priv->popover)
		return TRUE;

	if (event_widget != widget)
		return FALSE;

	toggle_button = GTK_TOGGLE_TOOL_BUTTON (widget);
	if (gtk_toggle_tool_button_get_active (toggle_button))
		return FALSE;

	e_emoticon_tool_button_popup (button);

	button->priv->popup_in_progress = TRUE;

	return TRUE;
}

static void
emoticon_tool_button_toggled (GtkToggleToolButton *button)
{
	if (gtk_toggle_tool_button_get_active (button))
		e_emoticon_tool_button_popup (
			E_EMOTICON_TOOL_BUTTON (button));
	else
		e_emoticon_tool_button_popdown (
			E_EMOTICON_TOOL_BUTTON (button));
}

static void
emoticon_tool_button_popup (EEmoticonToolButton *button)
{
	GtkToggleToolButton *tool_button;

	if (!gtk_widget_get_realized (GTK_WIDGET (button)))
		return;

	if (button->priv->popup_shown)
		return;

	/* Activate the tool button. */
	tool_button = GTK_TOGGLE_TOOL_BUTTON (button);
	gtk_toggle_tool_button_set_active (tool_button, TRUE);

	/* Show the pop-up. */
	gtk_widget_show (button->priv->popover);
	gtk_widget_grab_focus (button->priv->table);
}

static void
emoticon_tool_button_popdown (EEmoticonToolButton *button)
{
	GtkToggleToolButton *tool_button;

	if (!gtk_widget_get_realized (GTK_WIDGET (button)))
		return;

	if (!button->priv->popup_shown)
		return;

	gtk_widget_hide (button->priv->popover);

	/* Deactivate the tool button. */
	tool_button = GTK_TOGGLE_TOOL_BUTTON (button);
	gtk_toggle_tool_button_set_active (tool_button, FALSE);
}

static EEmoticon *
emoticon_tool_button_get_current_emoticon (EEmoticonChooser *chooser)
{
	EEmoticonToolButton *self = E_EMOTICON_TOOL_BUTTON (chooser);

	if (!self->priv->active_button)
		return NULL;

	return g_object_get_data (G_OBJECT (self->priv->active_button), "emoticon");
}

static void
emoticon_tool_button_set_current_emoticon (EEmoticonChooser *chooser,
                                           EEmoticon *emoticon)
{
	EEmoticonToolButton *self = E_EMOTICON_TOOL_BUTTON (chooser);
	GList *list, *iter;

	list = gtk_container_get_children (GTK_CONTAINER (self->priv->table));

	for (iter = list; iter != NULL; iter = iter->next) {
		GtkWidget *item = iter->data;
		EEmoticon *candidate;

		candidate = g_object_get_data (G_OBJECT (item), "emoticon");
		if (candidate == NULL)
			continue;

		if (e_emoticon_equal (emoticon, candidate)) {
			gtk_button_clicked (GTK_BUTTON (item));
			break;
		}
	}

	g_list_free (list);
}

static void
e_emoticon_tool_button_class_init (EEmoticonToolButtonClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkToggleToolButtonClass *toggle_tool_button_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = emoticon_tool_button_set_property;
	object_class->get_property = emoticon_tool_button_get_property;
	object_class->dispose = emoticon_tool_button_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = emoticon_tool_button_press_event;

	toggle_tool_button_class = GTK_TOGGLE_TOOL_BUTTON_CLASS (class);
	toggle_tool_button_class->toggled = emoticon_tool_button_toggled;

	class->popup = emoticon_tool_button_popup;
	class->popdown = emoticon_tool_button_popdown;

	g_object_class_override_property (
		object_class, PROP_CURRENT_EMOTICON, "current-emoticon");

	g_object_class_install_property (
		object_class,
		PROP_POPUP_SHOWN,
		g_param_spec_boolean (
			"popup-shown",
			"Popup Shown",
			"Whether the button's dropdown is shown",
			FALSE,
			G_PARAM_READWRITE));

	signals[POPUP] = g_signal_new (
		"popup",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EEmoticonToolButtonClass, popup),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[POPDOWN] = g_signal_new (
		"popdown",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EEmoticonToolButtonClass, popdown),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_Down, GDK_MOD1_MASK, "popup", 0);
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_KP_Down, GDK_MOD1_MASK, "popup", 0);

	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_Up, GDK_MOD1_MASK, "popdown", 0);
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_KP_Up, GDK_MOD1_MASK, "popdown", 0);
	gtk_binding_entry_add_signal (
		gtk_binding_set_by_class (class),
		GDK_KEY_Escape, 0, "popdown", 0);
}

static void
e_emoticon_tool_button_interface_init (EEmoticonChooserInterface *interface)
{
	interface->get_current_emoticon =
		emoticon_tool_button_get_current_emoticon;
	interface->set_current_emoticon =
		emoticon_tool_button_set_current_emoticon;
}

static void
e_emoticon_tool_button_init (EEmoticonToolButton *button)
{
	EEmoticonChooser *chooser;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *popover;
	GList *list, *iter;
	gint ii;

	button->priv = e_emoticon_tool_button_get_instance_private (button);

	/* Build the popover. */
	popover = gtk_popover_new (GTK_WIDGET (button));
	gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
	gtk_popover_set_modal (GTK_POPOVER (popover), TRUE);
	button->priv->popover = g_object_ref_sink (popover);

	g_signal_connect_swapped (
		popover, "show",
		G_CALLBACK (emoticon_tool_button_child_show_cb), button);
	g_signal_connect_swapped (
		popover, "hide",
		G_CALLBACK (emoticon_tool_button_child_hide_cb), button);
	g_signal_connect_swapped (
		popover, "button-release-event",
		G_CALLBACK (emoticon_tool_button_button_release_event_cb),
		button);
	g_signal_connect_swapped (
		popover, "key-press-event",
		G_CALLBACK (emoticon_tool_button_child_key_press_event_cb),
		button);

	/* Build the popover content. */
	container = popover;

	widget = gtk_table_new (NUM_ROWS, NUM_COLS, TRUE);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 0);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 0);
	gtk_container_add (GTK_CONTAINER (container), widget);
	button->priv->table = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	chooser = E_EMOTICON_CHOOSER (button);
	list = e_emoticon_chooser_get_items ();
	g_return_if_fail (g_list_length (list) <= NUM_ROWS * NUM_COLS);

	for (iter = list, ii = 0; iter != NULL; iter = iter->next, ii++) {
		EEmoticon *emoticon = iter->data;
		guint left = ii % NUM_COLS;
		guint top = ii / NUM_COLS;
		gchar *tooltip;

		tooltip = emoticon_tool_button_elide_underscores (
			gettext (emoticon->label));

		widget = gtk_button_new ();
		gtk_button_set_image (
			GTK_BUTTON (widget),
			gtk_image_new_from_icon_name (
			emoticon->icon_name, GTK_ICON_SIZE_BUTTON));
		gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
		gtk_widget_set_tooltip_text (widget, tooltip);
		gtk_widget_show (widget);

		g_object_set_data_full (
			G_OBJECT (widget), "emoticon",
			e_emoticon_copy (emoticon),
			(GDestroyNotify) e_emoticon_free);

		g_signal_connect_swapped (
			widget, "clicked",
			G_CALLBACK (emoticon_tool_button_emoticon_clicked_cb),
			button);

		g_signal_connect_swapped (
			widget, "clicked",
			G_CALLBACK (e_emoticon_chooser_item_activated),
			chooser);

		g_signal_connect_swapped (
			widget, "button-release-event",
			G_CALLBACK (emoticon_tool_button_emoticon_release_event_cb),
			button);

		gtk_table_attach (
			GTK_TABLE (container), widget,
			left, left + 1, top, top + 1, 0, 0, 0, 0);

		g_free (tooltip);
	}

	g_list_free (list);
}

GtkToolItem *
e_emoticon_tool_button_new (void)
{
	return g_object_new (E_TYPE_EMOTICON_TOOL_BUTTON, NULL);
}

void
e_emoticon_tool_button_popup (EEmoticonToolButton *button)
{
	g_return_if_fail (E_IS_EMOTICON_TOOL_BUTTON (button));

	g_signal_emit (button, signals[POPUP], 0);
}

void
e_emoticon_tool_button_popdown (EEmoticonToolButton *button)
{
	g_return_if_fail (E_IS_EMOTICON_TOOL_BUTTON (button));

	g_signal_emit (button, signals[POPDOWN], 0);
}
