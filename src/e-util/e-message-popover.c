/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-util-enums.h"
#include "e-util-enumtypes.h"

#include "e-message-popover.h"

/**
 * SECTION: e-message-popover
 * @include: e-util/e-util.h
 * @short_description: a GtkPopover with a message text
 *
 * #EMessagePopover is a simple modal GtkPopover showing a message attached
 * to a provided widget.
 *
 * Create the instance with e_message_popover_new(), then
 * set the text of the message with either of e_message_popover_set_text(),
 * e_message_popover_set_markup() or their "literal" variants,
 * and then show the popover with e_message_popover_show().
 * After this the popover is freed when closed.
 *
 * Since: 3.60
 **/

struct _EMessagePopover {
	GtkPopover parent_object;

	GtkLabel *label; /* not owned */

	GtkStyleProvider *style_provider;
	EMessagePopoverFlags flags;
};

enum {
	PROP_0,
	PROP_FLAGS,
	LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };

G_DEFINE_TYPE (EMessagePopover, e_message_popover, GTK_TYPE_POPOVER)

static void
e_message_popover_style_widget (EMessagePopover *self,
				GtkWidget *widget) /* NULL to style self */
{
	GtkStyleContext *style_context;

	style_context = widget ? gtk_widget_get_style_context (widget) : gtk_widget_get_style_context (GTK_WIDGET (self));

	if ((self->flags & E_MESSAGE_POPOVER_FLAG_INFO) != 0)
		gtk_style_context_add_class (style_context, widget ? "e-message-popover-info" : "e-message-popover-info-border");
	else if ((self->flags & E_MESSAGE_POPOVER_FLAG_WARNING) != 0)
		gtk_style_context_add_class (style_context, widget ? "e-message-popover-warning" : "e-message-popover-warning-border");
	else if ((self->flags & E_MESSAGE_POPOVER_FLAG_ERROR) != 0)
		gtk_style_context_add_class (style_context, widget ? "e-message-popover-error" : "e-message-popover-error-border");
	else
		return;

	gtk_style_context_add_provider (style_context, self->style_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static gboolean
e_message_popover_key_press_event_cb (GtkWidget *widget,
				      GdkEvent *event,
				      gpointer user_data)
{
	gtk_popover_popdown (GTK_POPOVER (widget));

	return FALSE;
}

static void
e_message_popover_closed_cb (GtkWidget *popover,
			     gpointer user_data)
{
	EMessagePopover *self = E_MESSAGE_POPOVER (popover);

	if ((self->flags & E_MESSAGE_POPOVER_FLAG_HIGHLIGHT_WIDGET) != 0) {
		GtkWidget *widget;

		widget = gtk_popover_get_relative_to (GTK_POPOVER (self));
		if (widget) {
			GtkStyleContext *style_context;

			style_context = gtk_widget_get_style_context (widget);

			if ((self->flags & E_MESSAGE_POPOVER_FLAG_INFO) != 0)
				gtk_style_context_remove_class (style_context, "e-message-popover-info");
			if ((self->flags & E_MESSAGE_POPOVER_FLAG_WARNING) != 0)
				gtk_style_context_remove_class (style_context, "e-message-popover-warning");
			if ((self->flags & E_MESSAGE_POPOVER_FLAG_ERROR) != 0)
				gtk_style_context_remove_class (style_context, "e-message-popover-error");

			gtk_style_context_remove_provider (style_context, self->style_provider);
		}
	}

	gtk_widget_destroy (popover);
}

static void
e_message_popover_set_property (GObject *object,
				guint property_id,
				const GValue *value,
				GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_FLAGS:
		E_MESSAGE_POPOVER (object)->flags = g_value_get_flags (value);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_message_popover_get_property (GObject *object,
				guint property_id,
				GValue *value,
				GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_FLAGS:
		g_value_set_flags (value, E_MESSAGE_POPOVER (object)->flags);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_message_popover_constructed (GObject *object)
{
	EMessagePopover *self = E_MESSAGE_POPOVER (object);
	GtkCssProvider *css_provider;
	GtkWidget *widget;
	GtkBox *box;
	GError *local_error = NULL;

	G_OBJECT_CLASS (e_message_popover_parent_class)->constructed (object);

	css_provider = gtk_css_provider_new ();

	self->style_provider = GTK_STYLE_PROVIDER (css_provider);

	gtk_css_provider_load_from_data (css_provider,
		".e-message-popover-info { box-shadow: inset 0 0 0 2px @theme_selected_bg_color; }\n"
		".e-message-popover-info-border { border: solid 2px @theme_selected_bg_color; }\n"
		".e-message-popover-warning { box-shadow: inset 0 0 0 2px #f57900; }\n"
		".e-message-popover-warning-border { border: solid 2px #f57900; }\n"
		".e-message-popover-error { box-shadow: inset 0 0 0 2px #ff7777; }\n"
		".e-message-popover-error-border { border: solid 2px #ff7777; }",
		-1, &local_error);

	if (local_error) {
		g_warning ("%s: Failed to parse CSS: %s", G_STRFUNC, local_error->message);
		g_clear_error (&local_error);
	}

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

	gtk_container_add (GTK_CONTAINER (self), widget);

	box = GTK_BOX (widget);

	widget = gtk_label_new ("");
	g_object_set (widget,
		"width-chars", 30,
		"max-width-chars", 40,
		"wrap", TRUE,
		NULL);

	g_object_set (self,
		"border-width", 6,
		"position", GTK_POS_BOTTOM,
		"modal", TRUE,
		NULL);

	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	self->label = GTK_LABEL (widget);

	g_signal_connect (self, "key-press-event",
		G_CALLBACK (e_message_popover_key_press_event_cb), NULL);
	g_signal_connect (self, "closed",
		G_CALLBACK (e_message_popover_closed_cb), NULL);

	if ((self->flags & E_MESSAGE_POPOVER_FLAG_HIGHLIGHT_POPOVER) != 0)
		e_message_popover_style_widget (self, NULL);
}

static void
e_message_popover_dispose (GObject *object)
{
	EMessagePopover *self = E_MESSAGE_POPOVER (object);

	g_clear_object (&self->style_provider);

	G_OBJECT_CLASS (e_message_popover_parent_class)->dispose (object);
}

static void
e_message_popover_class_init (EMessagePopoverClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_message_popover_set_property;
	object_class->get_property = e_message_popover_get_property;
	object_class->constructed = e_message_popover_constructed;
	object_class->dispose = e_message_popover_dispose;

	properties[PROP_FLAGS] = g_param_spec_flags ("flags", NULL, NULL,
		E_TYPE_MESSAGE_POPOVER_FLAGS,
		E_MESSAGE_POPOVER_FLAG_NONE,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
e_message_popover_init (EMessagePopover *self)
{
}

/**
 * e_message_popover_new:
 * @relative_to: a #GtkWidget to tight the message popover to
 * @flags: a bit-or of #EMessagePopoverFlags flags
 *
 * Creates a new #EMessagePopover. The @flags influence the behaviour
 * and look out of the popover.
 *
 * The @relative_to widget cannot be changed after the creation.
 *
 * Returns: (transfer full): a new #EMessagePopover
 *
 * Since: 3.60
 **/
EMessagePopover *
e_message_popover_new (GtkWidget *relative_to,
		       EMessagePopoverFlags flags)
{
	EMessagePopover *self;

	self = g_object_new (E_TYPE_MESSAGE_POPOVER,
		"relative-to", relative_to,
		"flags", flags,
		NULL);

	if (relative_to && (self->flags & E_MESSAGE_POPOVER_FLAG_HIGHLIGHT_WIDGET) != 0)
		e_message_popover_style_widget (self, relative_to);

	return self;
}

/**
 * e_message_popover_set_text:
 * @self: an #EMessagePopover
 * @format: a printf-like format of the text
 * @...: parameters for the @format
 *
 * Sets plain text content of the message popover, formatted
 * according to @format. Use e_message_popover_set_text_literal()
 * to set text directly.
 *
 * See e_message_popover_set_markup().
 *
 * Since: 3.60
 **/
void
e_message_popover_set_text (EMessagePopover *self,
			    const gchar *format,
			    ...)
{
	va_list va;
	gchar *text;

	g_return_if_fail (E_IS_MESSAGE_POPOVER (self));

	va_start (va, format);
	text = g_strdup_vprintf (format, va);
	va_end (va);

	e_message_popover_set_text_literal (self, text);

	g_free (text);
}

/**
 * e_message_popover_set_text_literal:
 * @self: an #EMessagePopover
 * @text: a text to set
 *
 * Sets the @text as the message shown in the @self.
 *
 * See e_message_popover_set_text(), e_message_popover_set_markup_literal().
 *
 * Since: 3.60
 **/
void
e_message_popover_set_text_literal (EMessagePopover *self,
				    const gchar *text)
{
	g_return_if_fail (E_IS_MESSAGE_POPOVER (self));
	g_return_if_fail (text != NULL);

	gtk_label_set_text (self->label, text);
}

/**
 * e_message_popover_set_markup:
 * @self: an #EMessagePopover
 * @format: a printf-like format of the markup
 * @...: parameters for the @format
 *
 * Sets markup content of the message popover, formatted
 * according to @format. Use e_message_popover_set_markup_literal()
 * to set markup directly.
 *
 * See e_message_popover_set_text().
 *
 * Since: 3.60
 **/
void
e_message_popover_set_markup (EMessagePopover *self,
			      const gchar *format,
			      ...)
{
	va_list va;
	gchar *markup;

	g_return_if_fail (E_IS_MESSAGE_POPOVER (self));

	va_start (va, format);
	markup = g_markup_vprintf_escaped (format, va);
	va_end (va);

	e_message_popover_set_markup_literal (self, markup);

	g_free (markup);
}

/**
 * e_message_popover_set_markup_literal:
 * @self: an #EMessagePopover
 * @markup: a markup to set
 *
 * Sets the @markup as the message shown in the @self.
 *
 * See e_message_popover_set_markup(), e_message_popover_set_text_literal().
 *
 * Since: 3.60
 **/
void
e_message_popover_set_markup_literal (EMessagePopover *self,
				      const gchar *markup)
{
	g_return_if_fail (E_IS_MESSAGE_POPOVER (self));
	g_return_if_fail (markup != NULL);

	gtk_label_set_markup (self->label, markup);
}

/**
 * e_message_popover_show:
 * @self: (transfer full): an #EMessagePopover
 *
 * Shows the message popover and frees it once it's closed.
 *
 * Since: 3.60
 **/
void
e_message_popover_show (EMessagePopover *self)
{
	g_return_if_fail (E_IS_MESSAGE_POPOVER (self));

	gtk_widget_show_all (GTK_WIDGET (self));
}
