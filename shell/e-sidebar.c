/*
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
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-sidebar.h"

#include "e-util/e-util.h"

#include <gconf/gconf-client.h>

typedef struct {
	GtkWidget *button_widget;
	GtkWidget *label;
	GtkWidget *icon;
	GtkWidget *hbox;
	gchar *default_icon_name;
	gint id;
} Button;

struct _ESidebarPrivate {
	ESidebarMode mode;
	ESidebarMode toolbar_mode;

	gboolean show;

	GtkWidget *selection_widget;
	GSList *buttons;

	guint style_changed_id;

	gboolean in_toggle;
};

enum {
	BUTTON_SELECTED,
	BUTTON_PRESSED,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0 };

G_DEFINE_TYPE (ESidebar, e_sidebar, GTK_TYPE_CONTAINER)

#define INTERNAL_MODE(sidebar)  (sidebar->priv->mode == E_SIDEBAR_MODE_TOOLBAR ? sidebar->priv->toolbar_mode : sidebar->priv->mode)
#define H_PADDING 6
#define V_PADDING 6

/* Utility functions.  */

static Button *
button_new (GtkWidget *button_widget,
	    GtkWidget *label,
	    GtkWidget *icon,
	    GtkWidget *hbox,
	    gint        id)
{
	Button *button = g_new (Button, 1);
	const gchar *icon_name;

	button->button_widget = button_widget;
	button->label = label;
        button->icon = icon;
	button->hbox = hbox;
	button->id = id;

	gtk_image_get_icon_name (GTK_IMAGE (icon), &icon_name, NULL);
	button->default_icon_name = g_strdup (icon_name);

	g_object_ref (button_widget);
	g_object_ref (label);
        g_object_ref (icon);
	g_object_ref (hbox);

	return button;
}

static void
button_free (Button *button)
{
	g_object_unref (button->button_widget);
	g_object_unref (button->label);
	g_object_unref (button->icon);
	g_object_unref (button->hbox);
	g_free (button->default_icon_name);
	g_free (button);
}

static void
update_buttons (ESidebar *sidebar, gint new_selected_id)
{
	GSList *p;

	sidebar->priv->in_toggle = TRUE;

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		if (button->id == new_selected_id)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button->button_widget), TRUE);
		else
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button->button_widget), FALSE);
	}

	sidebar->priv->in_toggle = FALSE;
}

/* Callbacks.  */

static void
button_toggled_callback (GtkToggleButton *toggle_button,
			 ESidebar *sidebar)
{
	gint id = 0;
	gboolean is_active = FALSE;
	GSList *p;

	if (sidebar->priv->in_toggle)
		return;

	sidebar->priv->in_toggle = TRUE;

	if (gtk_toggle_button_get_active (toggle_button))
		is_active = TRUE;

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		if (button->button_widget != GTK_WIDGET (toggle_button)) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button->button_widget), FALSE);
		} else {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button->button_widget), TRUE);
			id = button->id;
		}
	}

	sidebar->priv->in_toggle = FALSE;

	if (is_active)
		g_signal_emit (sidebar, signals[BUTTON_SELECTED], 0, id);
}

static gboolean
button_pressed_callback (GtkToggleButton *toggle_button,
			 GdkEventButton  *event,
			 ESidebar        *sidebar)
{
	gboolean return_val = FALSE;
	GSList *p;

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		if (button->button_widget == GTK_WIDGET (toggle_button))
			g_signal_emit (sidebar, signals [BUTTON_PRESSED],
				       0, event, button->id, &return_val);
	}

	return return_val;
}

static gboolean
button_query_tooltip (GtkWidget  *widget,
		      gint        x,
		      gint        y,
		      gboolean    keyboard_mode,
		      GtkTooltip *tooltip,
		      ESidebar   *sidebar)
{
	/* Show the tooltip only if the label is hidden */
	if (INTERNAL_MODE (sidebar) == E_SIDEBAR_MODE_ICON) {
		gchar *tip;

		tip = g_object_get_data (G_OBJECT (widget),
					 "ESidebar:button-tooltip");
		if (tip) {
			gtk_tooltip_set_text (tooltip, tip);

			return TRUE;
		}
	}

	return FALSE;
}

/* Layout. */

static gint
layout_buttons (ESidebar *sidebar)
{
	GtkAllocation *allocation = & GTK_WIDGET (sidebar)->allocation;
	ESidebarMode mode;
	gboolean icons_only;
	gint num_btns = g_slist_length (sidebar->priv->buttons), btns_per_row;
	GSList **rows, *p;
	Button *button;
	gint row_number;
	gint max_btn_width = 0, max_btn_height = 0;
	gint row_last;
	gint x, y;
	gint i;

	y = allocation->y + allocation->height - V_PADDING - 1;

	if (num_btns == 0)
		return y;

	mode = INTERNAL_MODE (sidebar);
	icons_only = (mode == E_SIDEBAR_MODE_ICON);

	/* Figure out the max width and height */
	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		GtkRequisition requisition;

		button = p->data;
		gtk_widget_size_request (GTK_WIDGET (button->button_widget), &requisition);

		max_btn_height = MAX (max_btn_height, requisition.height);
		max_btn_width = MAX (max_btn_width, requisition.width);
	}

	/* Figure out how many rows and columns we'll use. */
	btns_per_row = allocation->width / (max_btn_width + H_PADDING);
	if (!icons_only) {
		/* If using text buttons, we want to try to have a
		 * completely filled-in grid, but if we can't, we want
		 * the odd row to have just a single button.
		 */
		while (num_btns % btns_per_row > 1)
			btns_per_row--;
	}

	/* Assign buttons to rows */
	rows = g_new0 (GSList *, num_btns / btns_per_row + 1);

	if (!icons_only && num_btns % btns_per_row != 0) {
		button = sidebar->priv->buttons->data;
		rows [0] = g_slist_append (rows [0], button->button_widget);

		p = sidebar->priv->buttons->next;
		row_number = p ? 1 : 0;
	} else {
		p = sidebar->priv->buttons;
		row_number = 0;
	}

	for (; p != NULL; p = p->next) {
		button = p->data;

		if (g_slist_length (rows [row_number]) == btns_per_row)
			row_number ++;

		rows [row_number] = g_slist_append (rows [row_number], button->button_widget);
	}

	row_last = row_number;

	/* Layout the buttons. */
	for (i = row_last; i >= 0; i --) {
		gint len, extra_width;

		y -= max_btn_height;
		x = H_PADDING + allocation->x;
		len = g_slist_length (rows[i]);
		if (mode == E_SIDEBAR_MODE_TEXT || mode == E_SIDEBAR_MODE_BOTH)
			extra_width = (allocation->width - (len * max_btn_width ) - (len * H_PADDING)) / len;
		else
			extra_width = 0;
		for (p = rows [i]; p != NULL; p = p->next) {
			GtkAllocation child_allocation;

			child_allocation.x = x;
			child_allocation.y = y;
			child_allocation.width = max_btn_width + extra_width;
			child_allocation.height = max_btn_height;

			gtk_widget_size_allocate (GTK_WIDGET (p->data), &child_allocation);

			x += child_allocation.width + H_PADDING;
		}

		y -= V_PADDING;
	}

	for (i = 0; i <= row_last; i ++)
		g_slist_free (rows [i]);
	g_free (rows);

	return y;
}

static void
do_layout (ESidebar *sidebar)
{
	GtkAllocation *allocation = & GTK_WIDGET (sidebar)->allocation;
	GtkAllocation child_allocation;
	gint y;

	if (sidebar->priv->show)
		y = layout_buttons (sidebar);
	else
		y = allocation->y + allocation->height;

	/* Place the selection widget.  */
	child_allocation.x = allocation->x;
	child_allocation.y = allocation->y;
	child_allocation.width = allocation->width;
	child_allocation.height = y - allocation->y;

	gtk_widget_size_allocate (sidebar->priv->selection_widget, & child_allocation);
}

/* GtkContainer methods.  */

static void
impl_forall (GtkContainer *container,
	     gboolean include_internals,
	     GtkCallback callback,
	     gpointer callback_data)
{
	ESidebar *sidebar = E_SIDEBAR (container);
	GSList *p;

	if (sidebar->priv->selection_widget != NULL)
		(* callback) (sidebar->priv->selection_widget, callback_data);

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		GtkWidget *widget = ((Button *) p->data)->button_widget;
		(* callback) (widget, callback_data);
	}
}

static void
impl_remove (GtkContainer *container,
	     GtkWidget *widget)
{
	ESidebar *sidebar = E_SIDEBAR (container);
	GSList *p;

	if (widget == sidebar->priv->selection_widget) {
		e_sidebar_set_selection_widget (sidebar, NULL);
		return;
	}

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		GtkWidget *button_widget = ((Button *) p->data)->button_widget;

		if (button_widget == widget) {
			gtk_widget_unparent (button_widget);
			sidebar->priv->buttons = g_slist_remove_link (sidebar->priv->buttons, p);
			gtk_widget_queue_resize (GTK_WIDGET (sidebar));
			break;
		}
	}
}

/* GtkWidget methods.  */

static void
impl_size_request (GtkWidget *widget,
		   GtkRequisition *requisition)
{
	ESidebar *sidebar = E_SIDEBAR (widget);
	GSList *p;

	if (sidebar->priv->selection_widget == NULL) {
		requisition->width = 2 * H_PADDING;
		requisition->height = 2 * V_PADDING;
	} else {
		gtk_widget_size_request (sidebar->priv->selection_widget, requisition);
	}

	if (!sidebar->priv->show)
		return;

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;
		GtkRequisition button_requisition;

		gtk_widget_size_request (button->button_widget, &button_requisition);

		requisition->width = MAX (requisition->width, button_requisition.width + 2 * H_PADDING);
		requisition->height += button_requisition.height + V_PADDING;
	}
}

static void
impl_size_allocate (GtkWidget *widget,
		    GtkAllocation *allocation)
{
	widget->allocation = *allocation;

	do_layout (E_SIDEBAR (widget));
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	ESidebarPrivate *priv = E_SIDEBAR (object)->priv;
	GConfClient *gconf_client = gconf_client_get_default ();

	g_slist_foreach (priv->buttons, (GFunc) button_free, NULL);
	g_slist_free (priv->buttons);
	priv->buttons = NULL;

	if (priv->style_changed_id) {
		gconf_client_notify_remove (gconf_client, priv->style_changed_id);
		priv->style_changed_id = 0;
	}

	g_object_unref (gconf_client);

	(* G_OBJECT_CLASS (e_sidebar_parent_class)->dispose) (object);
}

static gboolean
boolean_handled_accumulator (GSignalInvocationHint *ihint,
			     GValue                *return_accu,
			     const GValue          *handler_return,
			     gpointer               dummy)
{
	gboolean handled;

	handled = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accu, handled);

	return !handled;
}

static void
impl_finalize (GObject *object)
{
	ESidebarPrivate *priv = E_SIDEBAR (object)->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (e_sidebar_parent_class)->finalize) (object);
}

/* Initialization.  */

static void
e_sidebar_class_init (ESidebarClass *klass)
{
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	container_class->forall = impl_forall;
	container_class->remove = impl_remove;

	widget_class->size_request = impl_size_request;
	widget_class->size_allocate = impl_size_allocate;

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	signals[BUTTON_SELECTED]
		= g_signal_new ("button_selected",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (ESidebarClass, button_selected),
				NULL, NULL,
				g_cclosure_marshal_VOID__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);
	signals[BUTTON_PRESSED]
		= g_signal_new ("button_pressed",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (ESidebarClass, button_pressed),
				boolean_handled_accumulator, NULL,
				e_marshal_NONE__POINTER_INT,
				G_TYPE_BOOLEAN, 2,
				G_TYPE_POINTER, G_TYPE_INT);
}

static void
e_sidebar_init (ESidebar *sidebar)
{
	ESidebarPrivate *priv;

	GTK_WIDGET_SET_FLAGS (sidebar, GTK_NO_WINDOW);

	priv = g_new0 (ESidebarPrivate, 1);
	sidebar->priv = priv;

	priv->mode = E_SIDEBAR_MODE_TEXT;
}

GtkWidget *
e_sidebar_new (void)
{
	ESidebar *sidebar = g_object_new (e_sidebar_get_type (), NULL);

	return GTK_WIDGET (sidebar);
}

void
e_sidebar_set_selection_widget (ESidebar *sidebar, GtkWidget *widget)
{
	if (sidebar->priv->selection_widget != NULL)
		gtk_widget_unparent (sidebar->priv->selection_widget);

	sidebar->priv->selection_widget = widget;

	if (widget != NULL)
		gtk_widget_set_parent (widget, GTK_WIDGET (sidebar));

	gtk_widget_queue_resize (GTK_WIDGET (sidebar));
}

void
e_sidebar_add_button (ESidebar *sidebar,
		      const gchar *label,
		      const gchar *tooltips,
		      const gchar *icon_name,
		      gint id)
{
	GtkWidget *button_widget;
	GtkWidget *hbox;
	GtkWidget *icon_widget;
	GtkWidget *label_widget;

	button_widget = gtk_toggle_button_new ();
	if (sidebar->priv->show)
		gtk_widget_show (button_widget);
	g_signal_connect (button_widget, "toggled", G_CALLBACK (button_toggled_callback), sidebar);
	g_signal_connect (button_widget, "button_press_event",
			  G_CALLBACK (button_pressed_callback), sidebar);

	hbox = gtk_hbox_new (FALSE, 3);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
	gtk_widget_show (hbox);

	icon_widget = gtk_image_new_from_icon_name (
		icon_name, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (icon_widget);

	label_widget = gtk_label_new (label);
	gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);
	gtk_widget_show (label_widget);

	g_object_set_data_full (G_OBJECT (button_widget),
				"ESidebar:button-tooltip",
				g_strdup (tooltips),
				g_free);
	gtk_widget_set_has_tooltip (button_widget, TRUE);
	g_signal_connect (button_widget, "query-tooltip",
			  G_CALLBACK (button_query_tooltip), sidebar);

	switch (INTERNAL_MODE (sidebar)) {
	case E_SIDEBAR_MODE_TEXT:
		gtk_box_pack_start (GTK_BOX (hbox), label_widget, TRUE, TRUE, 0);
		break;
	case E_SIDEBAR_MODE_ICON:
		gtk_box_pack_start (GTK_BOX (hbox), icon_widget, TRUE, TRUE, 0);
		break;
	case E_SIDEBAR_MODE_BOTH:
	default:
		gtk_box_pack_start (GTK_BOX (hbox), icon_widget, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), label_widget, TRUE, TRUE, 0);
		break;
	}

	gtk_container_add (GTK_CONTAINER (button_widget), hbox);

	sidebar->priv->buttons = g_slist_append (sidebar->priv->buttons, button_new (button_widget, label_widget, icon_widget, hbox, id));
	gtk_widget_set_parent (button_widget, GTK_WIDGET (sidebar));

	gtk_widget_queue_resize (GTK_WIDGET (sidebar));
}

/**
 * e_sidebar_change_button_icon
 * @sidebar: an #ESidebar
 * @icon_name: button icon name, or %NULL
 * @button_id: component's button ID, for which change the icon.
 *
 * This will change icon in icon_widget of the button of known component.
 * You cannot change icon as in a stack, only one default icon will be stored.
 **/
void
e_sidebar_change_button_icon (ESidebar *sidebar, const gchar *icon_name, gint button_id)
{
	GSList *p;

	g_return_if_fail (sidebar != NULL);

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		if (button->id == button_id) {
			if (!button->icon)
				break;

			if (icon_name == NULL)
				icon_name = button->default_icon_name;

			gtk_image_set_from_icon_name (
				GTK_IMAGE (button->icon),
				icon_name, GTK_ICON_SIZE_BUTTON);

			break;
		}
	}
}

void
e_sidebar_select_button (ESidebar *sidebar, gint id)
{
	update_buttons (sidebar, id);

	g_signal_emit (sidebar, signals[BUTTON_SELECTED], 0, id);
}

ESidebarMode
e_sidebar_get_mode (ESidebar *sidebar)
{
	return sidebar->priv->mode;
}

static GConfEnumStringPair toolbar_styles[] = {
         { E_SIDEBAR_MODE_TEXT, "text" },
         { E_SIDEBAR_MODE_ICON, "icons" },
         { E_SIDEBAR_MODE_BOTH, "both" },
         { E_SIDEBAR_MODE_BOTH, "both-horiz" },
         { E_SIDEBAR_MODE_BOTH, "both_horiz" },
	{ -1, NULL }
};

static void
set_mode_internal (ESidebar *sidebar, ESidebarMode mode )
{
	GSList *p;

	if (mode == INTERNAL_MODE (sidebar))
		return;

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		switch (mode) {
		case E_SIDEBAR_MODE_TEXT:
			gtk_container_remove (GTK_CONTAINER (button->hbox), button->icon);
			if (INTERNAL_MODE (sidebar) == E_SIDEBAR_MODE_ICON) {
				gtk_box_pack_start (GTK_BOX (button->hbox), button->label, TRUE, TRUE, 0);
				gtk_widget_show (button->label);
			}
			break;
		case E_SIDEBAR_MODE_ICON:
			gtk_container_remove(GTK_CONTAINER (button->hbox), button->label);
			if (INTERNAL_MODE (sidebar) == E_SIDEBAR_MODE_TEXT) {
				gtk_box_pack_start (GTK_BOX (button->hbox), button->icon, TRUE, TRUE, 0);
				gtk_widget_show (button->icon);
			} else
				gtk_container_child_set (GTK_CONTAINER (button->hbox), button->icon,
							 "expand", TRUE,
							 NULL);
			break;
		case E_SIDEBAR_MODE_BOTH:
			if (INTERNAL_MODE (sidebar) == E_SIDEBAR_MODE_TEXT) {
				gtk_container_remove (GTK_CONTAINER (button->hbox), button->label);
				gtk_box_pack_start (GTK_BOX (button->hbox), button->icon, FALSE, TRUE, 0);
				gtk_widget_show (button->icon);
			} else {
				gtk_container_child_set (GTK_CONTAINER (button->hbox), button->icon,
							 "expand", FALSE,
							 NULL);
			}

			gtk_box_pack_start (GTK_BOX (button->hbox), button->label, TRUE, TRUE, 0);
			gtk_widget_show (button->label);
			break;
		default:
			break;
		}
	}
}

static void
style_changed_notify (GConfClient *gconf, guint id, GConfEntry *entry, gpointer data)
{
	ESidebar *sidebar = data;
	gchar *val;
	gint mode;

	val = gconf_client_get_string (gconf, "/desktop/gnome/interface/toolbar_style", NULL);
	if (val == NULL || !gconf_string_to_enum (toolbar_styles, val, &mode))
		mode = E_SIDEBAR_MODE_BOTH;
	g_free(val);

	set_mode_internal (E_SIDEBAR (sidebar), mode);
	sidebar->priv->toolbar_mode = mode;

	gtk_widget_queue_resize (GTK_WIDGET (sidebar));
}

void
e_sidebar_set_mode (ESidebar *sidebar, ESidebarMode mode)
{
	GConfClient *gconf_client = gconf_client_get_default ();

	if (sidebar->priv->mode == mode)
		return;

	if (sidebar->priv->mode == E_SIDEBAR_MODE_TOOLBAR) {
		if (sidebar->priv->style_changed_id) {
			gconf_client_notify_remove (gconf_client, sidebar->priv->style_changed_id);
			sidebar->priv->style_changed_id = 0;
		}
	}

	if (mode != E_SIDEBAR_MODE_TOOLBAR) {
		set_mode_internal (sidebar, mode);

		gtk_widget_queue_resize (GTK_WIDGET (sidebar));
	} else {
		/* This is a little bit tricky, toolbar mode is more
		 * of a meta-mode where the actual mode is dictated by
		 * the gnome toolbar setting, so that is why we have
		 * the is_toolbar_mode bool - it tracks the toolbar
		 * mode while the mode member is the actual look and
		 * feel */
		sidebar->priv->style_changed_id = gconf_client_notify_add (gconf_client,
									   "/desktop/gnome/interface/toolbar_style",
									   style_changed_notify, sidebar, NULL, NULL);
		style_changed_notify (gconf_client, 0, NULL, sidebar);
	}

	g_object_unref (gconf_client);

	sidebar->priv->mode = mode;
}

void
e_sidebar_set_show_buttons  (ESidebar *sidebar, gboolean show)
{
	GSList *p;

	if (sidebar->priv->show == show)
		return;

	for (p = sidebar->priv->buttons; p != NULL; p = p->next) {
		Button *button = p->data;

		if (show)
			gtk_widget_show (button->button_widget);
		else
			gtk_widget_hide (button->button_widget);
	}

	sidebar->priv->show = show;

	gtk_widget_queue_resize (GTK_WIDGET (sidebar));
}

gboolean
e_sidebar_get_show_buttons  (ESidebar *sidebar)
{
	return sidebar->priv->show;
}
