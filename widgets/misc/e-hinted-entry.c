/*
 * e-hinted-entry.c
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

#include "e-hinted-entry.h"

#define E_HINTED_ENTRY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HINTED_ENTRY, EHintedEntryPrivate))

struct _EHintedEntryPrivate {
	gchar *hint;
	guint hint_shown : 1;
};

enum {
	PROP_0,
	PROP_HINT,
	PROP_HINT_SHOWN
};

G_DEFINE_TYPE (
	EHintedEntry,
	e_hinted_entry,
	GTK_TYPE_ENTRY)

static void
hinted_entry_show_hint (EHintedEntry *entry)
{
	GtkStyle *style;
	const GdkColor *color;
	const gchar *hint;

	entry->priv->hint_shown = TRUE;

	hint = e_hinted_entry_get_hint (entry);
	gtk_entry_set_text (GTK_ENTRY (entry), hint);

	style = gtk_widget_get_style (GTK_WIDGET (entry));
	color = &style->text[GTK_STATE_INSENSITIVE];
	gtk_widget_modify_text (GTK_WIDGET (entry), GTK_STATE_NORMAL, color);

	g_object_notify (G_OBJECT (entry), "hint-shown");
}

static void
hinted_entry_show_text (EHintedEntry *entry,
                        const gchar *text)
{
	entry->priv->hint_shown = FALSE;

	gtk_entry_set_text (GTK_ENTRY (entry), text);

	gtk_widget_modify_text (GTK_WIDGET (entry), GTK_STATE_NORMAL, NULL);

	g_object_notify (G_OBJECT (entry), "hint-shown");
}

static void
hinted_entry_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HINT:
			e_hinted_entry_set_hint (
				E_HINTED_ENTRY (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
hinted_entry_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_HINT:
			g_value_set_string (
				value, e_hinted_entry_get_hint (
				E_HINTED_ENTRY (object)));
			return;

		case PROP_HINT_SHOWN:
			g_value_set_boolean (
				value, e_hinted_entry_get_hint_shown (
				E_HINTED_ENTRY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
hinted_entry_finalize (GObject *object)
{
	EHintedEntryPrivate *priv;

	priv = E_HINTED_ENTRY_GET_PRIVATE (object);

	g_free (priv->hint);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_hinted_entry_parent_class)->finalize (object);
}

static void
hinted_entry_grab_focus (GtkWidget *widget)
{
	GtkWidgetClass *chain_class;

	/* We don't want hints to be selected so we chain to
	 * the GtkEntry parent if we have a hint set */
	chain_class = e_hinted_entry_parent_class;
	if (e_hinted_entry_get_hint_shown (E_HINTED_ENTRY (widget)))
		chain_class = g_type_class_peek_parent (chain_class);

	/* Chain up to parent's grab_focus() method. */
	GTK_WIDGET_CLASS (chain_class)->grab_focus (widget);
}

static gboolean
hinted_entry_focus_in_event (GtkWidget *widget,
                             GdkEventFocus *event)
{
	EHintedEntry *entry = E_HINTED_ENTRY (widget);

	if (e_hinted_entry_get_hint_shown (entry))
		hinted_entry_show_text (entry, "");

	/* Chain up to parent's focus_in_event() method. */
	return GTK_WIDGET_CLASS (e_hinted_entry_parent_class)->
		focus_in_event (widget, event);
}

static gboolean
hinted_entry_focus_out_event (GtkWidget *widget,
                              GdkEventFocus *event)
{
	EHintedEntry *entry = E_HINTED_ENTRY (widget);
	const gchar *text;

	text = e_hinted_entry_get_text (entry);

	if (text == NULL || *text == '\0')
		hinted_entry_show_hint (E_HINTED_ENTRY (widget));

	/* Chain up to parent's focus_out_event() method. */
	return GTK_WIDGET_CLASS (e_hinted_entry_parent_class)->
		focus_out_event (widget, event);
}

static void
e_hinted_entry_class_init (EHintedEntryClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHintedEntryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = hinted_entry_set_property;
	object_class->get_property = hinted_entry_get_property;
	object_class->finalize = hinted_entry_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->grab_focus = hinted_entry_grab_focus;
	widget_class->focus_in_event = hinted_entry_focus_in_event;
	widget_class->focus_out_event = hinted_entry_focus_out_event;

	g_object_class_install_property (
		object_class,
		PROP_HINT,
		g_param_spec_string (
			"hint",
			"Hint",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_HINT_SHOWN,
		g_param_spec_boolean (
			"hint-shown",
			"Hint Shown",
			NULL,
			FALSE,
			G_PARAM_READABLE));
}

static void
e_hinted_entry_init (EHintedEntry *entry)
{
	entry->priv = E_HINTED_ENTRY_GET_PRIVATE (entry);
	entry->priv->hint = g_strdup ("");  /* hint must never be NULL */
	hinted_entry_show_hint (entry);
}

GtkWidget *
e_hinted_entry_new (void)
{
	return g_object_new (E_TYPE_HINTED_ENTRY, NULL);
}

const gchar *
e_hinted_entry_get_hint (EHintedEntry *entry)
{
	g_return_val_if_fail (E_IS_HINTED_ENTRY (entry), NULL);

	return entry->priv->hint;
}

void
e_hinted_entry_set_hint (EHintedEntry *entry,
                         const gchar *hint)
{
	g_return_if_fail (E_IS_HINTED_ENTRY (entry));

	if (hint == NULL)
		hint = "";

	g_free (entry->priv->hint);
	entry->priv->hint = g_strdup (hint);

	if (e_hinted_entry_get_hint_shown (entry))
		gtk_entry_set_text (GTK_ENTRY (entry), hint);

	g_object_notify (G_OBJECT (entry), "hint");
}

gboolean
e_hinted_entry_get_hint_shown (EHintedEntry *entry)
{
	g_return_val_if_fail (E_IS_HINTED_ENTRY (entry), FALSE);

	return entry->priv->hint_shown;
}

const gchar *
e_hinted_entry_get_text (EHintedEntry *entry)
{
	const gchar *text = "";

	/* XXX This clumsily overrides gtk_entry_get_text(). */

	g_return_val_if_fail (E_IS_HINTED_ENTRY (entry), NULL);

	if (!e_hinted_entry_get_hint_shown (entry))
		text = gtk_entry_get_text (GTK_ENTRY (entry));

	return text;
}

void
e_hinted_entry_set_text (EHintedEntry *entry,
                         const gchar *text)
{
	/* XXX This clumsily overrides gtk_entry_set_text(). */

	g_return_if_fail (E_IS_HINTED_ENTRY (entry));

	if (text == NULL)
		text = "";

	if (*text == '\0' && !gtk_widget_has_focus (GTK_WIDGET (entry)))
		hinted_entry_show_hint (entry);
	else
		hinted_entry_show_text (entry, text);
}
