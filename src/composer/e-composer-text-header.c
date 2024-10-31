/*Modified for Anjal - Johnny Jacob <jjohnny@novell.com>*/

/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include "e-composer-text-header.h"

/* Convenience macro */
#define E_COMPOSER_TEXT_HEADER_GET_ENTRY(header) \
	(GTK_ENTRY (E_COMPOSER_HEADER (header)->input_widget))

G_DEFINE_TYPE (
	EComposerTextHeader,
	e_composer_text_header,
	E_TYPE_COMPOSER_HEADER)

static void
composer_text_header_changed_cb (GtkEntry *entry,
                                 EComposerTextHeader *header)
{
	g_signal_emit_by_name (header, "changed");
}

static gboolean
composer_text_header_query_tooltip_cb (GtkEntry *entry,
                                       gint x,
                                       gint y,
                                       gboolean keyboard_mode,
                                       GtkTooltip *tooltip)
{
	const gchar *text;

	text = gtk_entry_get_text (entry);

	if (keyboard_mode || text == NULL || *text == '\0')
		return FALSE;

	gtk_tooltip_set_text (tooltip, text);

	return TRUE;
}

static void
composer_text_header_constructed (GObject *object)
{
	GtkWidget *widget;
	EComposerTextHeader *header;
	EComposerTextHeaderClass *class;

	header = E_COMPOSER_TEXT_HEADER (object);
	class = E_COMPOSER_TEXT_HEADER_GET_CLASS (header);

	/* Input widget must be set before chaining up. */

	widget = g_object_new (class->entry_type, NULL);
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (composer_text_header_changed_cb), header);
	g_signal_connect (
		widget, "query-tooltip",
		G_CALLBACK (composer_text_header_query_tooltip_cb), NULL);
	gtk_widget_set_has_tooltip (widget, TRUE);
	E_COMPOSER_HEADER (object)->input_widget = g_object_ref_sink (widget);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_text_header_parent_class)->constructed (object);
}

static void
e_composer_text_header_class_init (EComposerTextHeaderClass *class)
{
	GObjectClass *object_class;

	class->entry_type = GTK_TYPE_ENTRY;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = composer_text_header_constructed;
}

static void
e_composer_text_header_init (EComposerTextHeader *header)
{
}

EComposerHeader *
e_composer_text_header_new_label (ESourceRegistry *registry,
                                  const gchar *label)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_TEXT_HEADER,
		"label", label, "button", FALSE,
		"registry", registry, NULL);
}

EComposerHeader *
e_composer_text_header_new_button (ESourceRegistry *registry,
                                   const gchar *label)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_TEXT_HEADER,
		"label", label, "button", TRUE,
		"registry", registry, NULL);
}

const gchar *
e_composer_text_header_get_text (EComposerTextHeader *header)
{
	GtkEntry *entry;

	g_return_val_if_fail (E_IS_COMPOSER_TEXT_HEADER (header), NULL);

	entry = E_COMPOSER_TEXT_HEADER_GET_ENTRY (header);

	return gtk_entry_get_text (entry);
}

void
e_composer_text_header_set_text (EComposerTextHeader *header,
                                 const gchar *text)
{
	GtkEntry *entry;

	g_return_if_fail (E_IS_COMPOSER_TEXT_HEADER (header));

	entry = E_COMPOSER_TEXT_HEADER_GET_ENTRY (header);

	gtk_entry_set_text (entry, (text != NULL) ? text : "");
}
