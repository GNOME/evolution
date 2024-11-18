/*
 * e-alert-bar.c
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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-dialog-widgets.h"
#include "e-alert-dialog.h"
#include "e-alert-bar.h"

/* The GtkScrolledWindow has some minimum height of 86 pixels or so, but some
   messages can be smaller, thus subclass it here and override the minimum value. */

typedef struct _EScrolledWindow {
	GtkScrolledWindow parent;
} EScrolledWindow;

typedef struct _EScrolledWindowClass {
	GtkScrolledWindowClass parent_class;
} EScrolledWindowClass;

GType e_scrolled_window_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (EScrolledWindow, e_scrolled_window, GTK_TYPE_SCROLLED_WINDOW)

static void
e_scrolled_window_get_preferred_height_for_width (GtkWidget *widget,
						  gint width,
						  gint *minimum_size,
						  gint *natural_size)
{
	GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW (widget);
	GtkWidget *child;
	gint min_height, max_height;

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_scrolled_window_parent_class)->get_preferred_height_for_width (widget, width, minimum_size, natural_size);

	min_height = gtk_scrolled_window_get_min_content_height (scrolled_window);
	max_height = gtk_scrolled_window_get_max_content_height (scrolled_window);

	if (min_height > 0 && min_height < *minimum_size)
		*minimum_size = min_height + 2;

	if (max_height > 0 && max_height < *natural_size)
		*natural_size = max_height + 2;

	child = gtk_bin_get_child (GTK_BIN (widget));

	if (child && width > 1) {
		gint child_min_height = -1, child_natural_height = -1;

		gtk_widget_get_preferred_height_for_width (child, width, &child_min_height, &child_natural_height);

		if (*minimum_size > child_min_height && child_min_height > 0)
			*minimum_size = child_min_height + 2;

		if (*natural_size > child_natural_height && child_natural_height > 0)
			*natural_size = child_natural_height + 2;
	}
}

static GtkSizeRequestMode
e_scrolled_window_get_request_mode (GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
e_scrolled_window_class_init (EScrolledWindowClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_height_for_width = e_scrolled_window_get_preferred_height_for_width;
	widget_class->get_request_mode = e_scrolled_window_get_request_mode;
}

static void
e_scrolled_window_init (EScrolledWindow *scrolled_window)
{
}

static GtkWidget *
e_scrolled_window_new (void)
{
	return g_object_new (e_scrolled_window_get_type (),
		"hadjustment", NULL,
		"vadjustment", NULL,
		NULL);
}

/* GTK_ICON_SIZE_DIALOG is a tad too big. */
#define ICON_SIZE GTK_ICON_SIZE_DND

/* Dismiss warnings automatically after 5 minutes. */
#define WARNING_TIMEOUT_SECONDS (5 * 60)

/* Maximum height of the text message; after that value the message is scrolled. */
#define MAX_HEIGHT 200

struct _EAlertBarPrivate {
	GQueue alerts;
	GtkWidget *image;		/* not referenced */
	GtkWidget *scrolled_window;	/* not referenced */
	GtkWidget *message_label;	/* not referenced */
	gint max_content_height;
};

G_DEFINE_TYPE_WITH_PRIVATE (EAlertBar, e_alert_bar, GTK_TYPE_INFO_BAR)

static void
alert_bar_response_close (EAlert *alert)
{
	e_alert_response (alert, GTK_RESPONSE_CLOSE);
}

static void
alert_bar_show_alert (EAlertBar *alert_bar)
{
	GtkImage *image;
	GtkInfoBar *info_bar;
	GtkWidget *action_area;
	GtkWidget *widget;
	EAlert *alert;
	GList *link;
	GList *children;
	GtkMessageType message_type;
	const gchar *primary_text;
	const gchar *secondary_text;
	const gchar *icon_name;
	gboolean have_primary_text;
	gboolean have_secondary_text;
	gboolean visible;
	gint response_id;
	guint n_alerts;
	gchar *markup;

	info_bar = GTK_INFO_BAR (alert_bar);
	action_area = gtk_info_bar_get_action_area (info_bar);

	alert = g_queue_peek_head (&alert_bar->priv->alerts);
	g_return_if_fail (E_IS_ALERT (alert));

	/* Remove all buttons from the previous alert. */
	children = gtk_container_get_children (GTK_CONTAINER (action_area));
	while (children != NULL) {
		GtkWidget *child = GTK_WIDGET (children->data);
		gtk_container_remove (GTK_CONTAINER (action_area), child);
		children = g_list_delete_link (children, children);
	}

	/* Add alert-specific buttons. */
	link = e_alert_peek_actions (alert);
	while (link != NULL) {
		EUIAction *action = E_UI_ACTION (link->data);

		/* These actions are already wired to trigger an
		 * EAlert::response signal when activated, which
		 * will in turn call gtk_info_bar_response(), so
		 * we can add buttons directly to the action
		 * area without knowning their response IDs. */

		widget = e_alert_create_button_for_action (action);

		gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);

		link = g_list_next (link);
	}

	link = e_alert_peek_widgets (alert);
	while (link != NULL) {
		widget = link->data;

		gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);
		link = g_list_next (link);
	}

	/* Add a dismiss button. */
	widget = e_dialog_button_new_with_icon ("window-close", NULL);
	gtk_button_set_relief (
		GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (
		/* Translators: Escape is a keyboard binding. */
		widget, _("Close this message (Escape)"));
	gtk_box_pack_end (
		GTK_BOX (action_area), widget, FALSE, FALSE, 0);
	gtk_button_box_set_child_non_homogeneous (
		GTK_BUTTON_BOX (action_area), widget, TRUE);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (alert_bar_response_close), alert);

	n_alerts = g_queue_get_length (&alert_bar->priv->alerts);

	if (n_alerts > 1) {
		gchar *str;

		/* Translators: there are always at least two messages to be closed */
		str = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Close a message", "Close all %u messages", n_alerts), n_alerts);

		widget = e_dialog_button_new_with_icon ("edit-clear-all", NULL);
		gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
		gtk_widget_set_tooltip_text (widget, str);
		gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);
		gtk_button_box_set_child_non_homogeneous (GTK_BUTTON_BOX (action_area), widget, TRUE);
		gtk_widget_show (widget);

		g_signal_connect_swapped (
			widget, "clicked",
			G_CALLBACK (e_alert_bar_clear), alert_bar);

		g_free (str);
	}

	widget = gtk_widget_get_toplevel (GTK_WIDGET (alert_bar));

	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (alert_bar->priv->scrolled_window), -1);

	if (widget) {
		gint max_height;

		/* Allow up to 20% of the window height being used by the alert, or at least MAX_HEIGHT pixels */
		max_height = MAX (gtk_widget_get_allocated_height (widget) / 5, MAX_HEIGHT);

		alert_bar->priv->max_content_height = max_height;

		gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (alert_bar->priv->scrolled_window), max_height);
	}

	primary_text = e_alert_get_primary_text (alert);
	secondary_text = e_alert_get_secondary_text (alert);

	if (primary_text == NULL)
		primary_text = "";

	if (secondary_text == NULL)
		secondary_text = "";

	have_primary_text = (*primary_text != '\0');
	have_secondary_text = (*secondary_text != '\0');

	response_id = e_alert_get_default_response (alert);
	gtk_info_bar_set_default_response (info_bar, response_id);

	message_type = e_alert_get_message_type (alert);
	gtk_info_bar_set_message_type (info_bar, message_type);

	if (have_primary_text && have_secondary_text)
		markup = g_markup_printf_escaped ("<b>%s</b>\n\n<small>%s</small>", primary_text, secondary_text);
	else if (have_primary_text)
		markup = g_markup_escape_text (primary_text, -1);
	else
		markup = g_markup_escape_text (secondary_text, -1);
	gtk_label_set_markup (GTK_LABEL (alert_bar->priv->message_label), markup);
	g_free (markup);

	icon_name = e_alert_get_icon_name (alert);
	image = GTK_IMAGE (alert_bar->priv->image);
	gtk_image_set_from_icon_name (image, icon_name, ICON_SIZE);

	/* Avoid showing an image for empty alerts. */
	visible = have_primary_text || have_secondary_text;
	gtk_widget_set_visible (alert_bar->priv->image, visible);

	gtk_widget_show (GTK_WIDGET (alert_bar));

	/* Warnings are generally meant for transient errors.
	 * No need to leave them up indefinitely.  Close them
	 * automatically if the user hasn't responded after a
	 * reasonable period of time has elapsed. */
	if (message_type == GTK_MESSAGE_WARNING)
		e_alert_start_timer (alert, WARNING_TIMEOUT_SECONDS);
}

static void
alert_bar_response_cb (EAlert *alert,
                       gint response_id,
                       EAlertBar *alert_bar)
{
	GQueue *queue;
	EAlert *head;
	gboolean was_head;

	queue = &alert_bar->priv->alerts;
	head = g_queue_peek_head (queue);
	was_head = (alert == head);

	g_signal_handlers_disconnect_by_func (
		alert, alert_bar_response_cb, alert_bar);

	if (g_queue_remove (queue, alert))
		g_object_unref (alert);

	if (g_queue_is_empty (queue)) {
		GtkWidget *action_area;
		GList *children;

		gtk_widget_hide (GTK_WIDGET (alert_bar));

		action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (alert_bar));

		/* Remove all buttons from the previous alert. */
		children = gtk_container_get_children (GTK_CONTAINER (action_area));
		while (children != NULL) {
			GtkWidget *child = GTK_WIDGET (children->data);
			gtk_container_remove (GTK_CONTAINER (action_area), child);
			children = g_list_delete_link (children, children);
		}
	} else if (was_head) {
		GtkInfoBar *info_bar = GTK_INFO_BAR (alert_bar);
		gtk_info_bar_response (info_bar, response_id);
		alert_bar_show_alert (alert_bar);
	}
}

static gboolean
alert_bar_message_label_size_recalc_cb (gpointer user_data)
{
	GWeakRef *weakref = user_data;
	GtkAllocation allocation;
	GtkScrolledWindow *scrolled_window;
	GtkWidget *vscrollbar;
	EAlertBar *alert_bar;
	gint max_height, use_height;

	alert_bar = g_weak_ref_get (weakref);
	if (!alert_bar)
		return G_SOURCE_REMOVE;

	scrolled_window = GTK_SCROLLED_WINDOW (alert_bar->priv->scrolled_window);

	max_height = alert_bar->priv->max_content_height;
	gtk_widget_get_allocation (alert_bar->priv->message_label, &allocation);

	if (allocation.height > 0 && allocation.height <= max_height)
		use_height = allocation.height;
	else if (allocation.height <= 0)
		use_height = -1;
	else
		use_height = max_height;

	/* To avoid runtime warnings about min being larger than the new max */
	gtk_scrolled_window_set_min_content_height (scrolled_window, -1);

	if (use_height > 0 && use_height < max_height)
		gtk_scrolled_window_set_max_content_height (scrolled_window, use_height);
	else
		gtk_scrolled_window_set_max_content_height (scrolled_window, max_height);

	gtk_scrolled_window_set_min_content_height (scrolled_window, use_height);

	vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (alert_bar->priv->scrolled_window));
	gtk_widget_set_visible (vscrollbar, use_height > 0 && allocation.height > max_height);

	gtk_widget_queue_resize (alert_bar->priv->scrolled_window);

	g_object_unref (alert_bar);

	return G_SOURCE_REMOVE;
}

static void
alert_bar_message_label_size_allocate_cb (GtkWidget *message_label,
					  GdkRectangle *allocation,
					  gpointer user_data)
{
	EAlertBar *alert_bar = user_data;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));

	g_timeout_add_full (G_PRIORITY_HIGH_IDLE, 1, alert_bar_message_label_size_recalc_cb,
		e_weak_ref_new (alert_bar), (GDestroyNotify) e_weak_ref_free);
}

static void
alert_bar_dispose (GObject *object)
{
	EAlertBar *self = E_ALERT_BAR (object);

	if (self->priv->message_label) {
		g_signal_handlers_disconnect_by_func (self->priv->message_label,
			G_CALLBACK (alert_bar_message_label_size_allocate_cb), object);
		self->priv->message_label = NULL;
	}

	while (!g_queue_is_empty (&self->priv->alerts)) {
		EAlert *alert = g_queue_pop_head (&self->priv->alerts);
		g_signal_handlers_disconnect_by_func (
			alert, alert_bar_response_cb, object);
		g_object_unref (alert);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_alert_bar_parent_class)->dispose (object);
}

static void
alert_bar_add_css_style (GtkWidget *widget,
			 const gchar *css)
{
	GtkCssProvider *provider;
	GError *error = NULL;

	provider = gtk_css_provider_new ();

	if (gtk_css_provider_load_from_data (provider, css, -1, &error)) {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (widget);

		gtk_style_context_add_provider (style_context, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	} else {
		g_warning ("%s: Failed to parse CSS for %s: %s", G_STRFUNC, G_OBJECT_TYPE_NAME (widget), error ? error->message : "Unknown error");
	}

	g_clear_object (&provider);
	g_clear_error (&error);
}

static void
alert_bar_constructed (GObject *object)
{
	EAlertBar *self;
	GtkInfoBar *info_bar;
	GtkWidget *action_area;
	GtkWidget *content_area;
	GtkWidget *container;
	GtkWidget *widget;
	GObject *revealer;

	self = E_ALERT_BAR (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_alert_bar_parent_class)->constructed (object);

	g_queue_init (&self->priv->alerts);

	info_bar = GTK_INFO_BAR (object);
	action_area = gtk_info_bar_get_action_area (info_bar);
	content_area = gtk_info_bar_get_content_area (info_bar);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (action_area), GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_valign (action_area, GTK_ALIGN_START);

	container = content_area;

	widget = gtk_image_new ();
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	self->priv->image = widget;
	gtk_widget_show (widget);

	widget = e_scrolled_window_new ();
	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_CENTER,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"hscrollbar-policy", GTK_POLICY_NEVER,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	self->priv->scrolled_window = widget;
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (widget), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	gtk_container_add (GTK_CONTAINER (container), widget);
	self->priv->message_label = widget;
	gtk_widget_show (widget);

	g_signal_connect (self->priv->message_label, "size-allocate",
		G_CALLBACK (alert_bar_message_label_size_allocate_cb), object);

	widget = gtk_bin_get_child (GTK_BIN (container));

	if (GTK_IS_VIEWPORT (widget)) {
		gtk_viewport_set_shadow_type (GTK_VIEWPORT (widget), GTK_SHADOW_NONE);

		alert_bar_add_css_style (widget, "viewport { background: none; border: none; }");
	}

	alert_bar_add_css_style (container, "scrolledwindow { background: none; border: none; }");

	/* Disable animation of the revealer, until GtkInfoBar's bug #710888 is fixed */
	revealer = gtk_widget_get_template_child (GTK_WIDGET (object), GTK_TYPE_INFO_BAR, "revealer");
	if (revealer) {
		gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
		gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 0);
	}
}

static GtkSizeRequestMode
alert_bar_get_request_mode (GtkWidget *widget)
{
	/* GtkBox does width-for-height by default.  But we
	 * want the alert bar to be as short as possible. */
	return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
alert_bar_close (GtkInfoBar *info_bar)
{
	/* GtkInfoBar's close() method looks for a button with a response
	 * code of GTK_RESPONSE_CANCEL.  But that does not apply here, so
	 * we have to override the method. */
	e_alert_bar_close_alert (E_ALERT_BAR (info_bar));
}

static void
e_alert_bar_class_init (EAlertBarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkInfoBarClass *info_bar_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = alert_bar_dispose;
	object_class->constructed = alert_bar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_request_mode = alert_bar_get_request_mode;

	info_bar_class = GTK_INFO_BAR_CLASS (class);
	info_bar_class->close = alert_bar_close;
}

static void
e_alert_bar_init (EAlertBar *alert_bar)
{
	alert_bar->priv = e_alert_bar_get_instance_private (alert_bar);
	alert_bar->priv->max_content_height = MAX_HEIGHT;
}

GtkWidget *
e_alert_bar_new (void)
{
	return g_object_new (E_TYPE_ALERT_BAR, NULL);
}

void
e_alert_bar_clear (EAlertBar *alert_bar)
{
	GQueue *queue;
	EAlert *alert;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));

	queue = &alert_bar->priv->alerts;

	while ((alert = g_queue_pop_head (queue)) != NULL)
		alert_bar_response_close (alert);
}

gboolean
e_alert_bar_remove_alert_by_tag (EAlertBar *alert_bar,
				 const gchar *tag)
{
	GList *link;

	g_return_val_if_fail (E_IS_ALERT_BAR (alert_bar), FALSE);
	g_return_val_if_fail (tag != NULL, FALSE);

	for (link = g_queue_peek_head_link (&alert_bar->priv->alerts); link; link = g_list_next (link)) {
		EAlert *alert = link->data;

		if (g_strcmp0 (tag, e_alert_get_tag (alert)) == 0) {
			alert_bar_response_close (alert);
			return TRUE;
		}
	}

	return FALSE;
}

typedef struct {
	gboolean found;
	EAlert *looking_for;
} DuplicateData;

static void
alert_bar_find_duplicate_cb (EAlert *alert,
                             DuplicateData *dd)
{
	g_return_if_fail (dd->looking_for != NULL);

	dd->found |= (
		e_alert_get_message_type (alert) ==
		e_alert_get_message_type (dd->looking_for) &&
		g_strcmp0 (
			e_alert_get_primary_text (alert),
			e_alert_get_primary_text (dd->looking_for)) == 0 &&
		g_strcmp0 (
			e_alert_get_secondary_text (alert),
			e_alert_get_secondary_text (dd->looking_for)) == 0);
}

void
e_alert_bar_add_alert (EAlertBar *alert_bar,
                       EAlert *alert)
{
	DuplicateData dd;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));
	g_return_if_fail (E_IS_ALERT (alert));

	dd.found = FALSE;
	dd.looking_for = alert;

	g_queue_foreach (
		&alert_bar->priv->alerts,
		(GFunc) alert_bar_find_duplicate_cb, &dd);

	if (dd.found)
		return;

	g_signal_connect (
		alert, "response",
		G_CALLBACK (alert_bar_response_cb), alert_bar);

	g_queue_push_head (&alert_bar->priv->alerts, g_object_ref (alert));

	alert_bar_show_alert (alert_bar);
}

/**
 * e_alert_bar_close_alert:
 * @alert_bar: an #EAlertBar
 *
 * Closes the active #EAlert and returns %TRUE, or else returns %FALSE if
 * there is no active #EAlert.
 *
 * Returns: whether an #EAlert was closed
 **/
gboolean
e_alert_bar_close_alert (EAlertBar *alert_bar)
{
	EAlert *alert;
	gboolean alert_closed = FALSE;

	g_return_val_if_fail (E_IS_ALERT_BAR (alert_bar), FALSE);

	alert = g_queue_peek_head (&alert_bar->priv->alerts);

	if (alert != NULL) {
		alert_bar_response_close (alert);
		alert_closed = TRUE;
	}

	return alert_closed;
}

/**
 * e_alert_bar_submit_alert:
 * @alert_bar: an #EAlertBar
 * @alert: an #EAlert
 *
 * Depending on the @alert type either shows a dialog or adds
 * the alert into the @alert_bar. This is meant to be used
 * by #EAlertSink implementations which use the #EAlertBar.
 *
 * Since: 3.26
 **/
void
e_alert_bar_submit_alert (EAlertBar *alert_bar,
			  EAlert *alert)
{
	GtkWidget *toplevel;
	GtkWidget *widget;
	GtkWindow *parent;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));
	g_return_if_fail (E_IS_ALERT (alert));

	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
		case GTK_MESSAGE_WARNING:
		case GTK_MESSAGE_QUESTION:
		case GTK_MESSAGE_ERROR:
			e_alert_bar_add_alert (alert_bar, alert);
			break;

		default:
			toplevel = gtk_widget_get_toplevel (GTK_WIDGET (alert_bar));
			if (GTK_IS_WINDOW (toplevel))
				parent = GTK_WINDOW (toplevel);
			else
				parent = NULL;
			widget = e_alert_dialog_new (parent, alert);
			gtk_dialog_run (GTK_DIALOG (widget));
			gtk_widget_destroy (widget);
	}
}
