/*
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

#include "evolution-config.h"

#include "e-msg-composer.h"
#include "e-composer-name-header.h"

#include <glib/gi18n.h>

/* XXX Temporary kludge */
#include "addressbook/gui/contact-editor/e-contact-editor.h"
#include "addressbook/gui/contact-list-editor/e-contact-list-editor.h"

/* Convenience macro */
#define E_COMPOSER_NAME_HEADER_GET_ENTRY(header) \
	(E_NAME_SELECTOR_ENTRY (E_COMPOSER_HEADER (header)->input_widget))

enum {
	PROP_0,
	PROP_NAME_SELECTOR
};

struct _EComposerNameHeaderPrivate {
	ENameSelector *name_selector;
	guint destination_index;
};

G_DEFINE_TYPE_WITH_PRIVATE (EComposerNameHeader, e_composer_name_header, E_TYPE_COMPOSER_HEADER)

static gpointer
contact_editor_fudge_new (EBookClient *book_client,
                          EContact *contact,
                          gboolean is_new,
                          gboolean editable)
{
	EABEditor *editor;
	EShell *shell = e_shell_get_default ();

	/* XXX Putting this function signature in libedataserverui
	 *     was a terrible idea.  Now we're stuck with it. */

	editor = e_contact_editor_new (shell, book_client, contact, is_new, editable);
	eab_editor_show (editor);

	return editor;
}

static gpointer
contact_list_editor_fudge_new (EBookClient *book_client,
                               EContact *contact,
                               gboolean is_new,
                               gboolean editable)
{
	EABEditor *editor;
	EShell *shell = e_shell_get_default ();

	/* XXX Putting this function signature in libedataserverui
	 *     was a terrible idea.  Now we're stuck with it. */

	editor = e_contact_list_editor_new (shell, book_client, contact, is_new, editable);
	eab_editor_show (editor);

	return editor;
}

static void
composer_name_header_entry_changed_cb (ENameSelectorEntry *entry,
                                       EComposerNameHeader *header)
{
	g_signal_emit_by_name (header, "changed");
}

static gboolean
composer_name_header_entry_query_tooltip_cb (GtkEntry *entry,
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
composer_name_header_visible_changed_cb (EComposerNameHeader *header)
{
	const gchar *label;
	EComposerNameHeader *self;
	ENameSelectorDialog *dialog;

	self = E_COMPOSER_NAME_HEADER (header);
	label = e_composer_header_get_label (E_COMPOSER_HEADER (header));
	dialog = e_name_selector_peek_dialog (self->priv->name_selector);

	e_name_selector_dialog_set_section_visible (
		dialog, label,
		e_composer_header_get_visible (E_COMPOSER_HEADER (header)));
}

static void
composer_name_header_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	EComposerNameHeader *self = E_COMPOSER_NAME_HEADER (object);

	switch (property_id) {
		case PROP_NAME_SELECTOR:	/* construct only */
			g_return_if_fail (self->priv->name_selector == NULL);
			self->priv->name_selector = g_value_dup_object (value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_name_header_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_NAME_SELECTOR:	/* construct only */
			g_value_set_object (
				value,
				e_composer_name_header_get_name_selector (
				E_COMPOSER_NAME_HEADER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_name_header_dispose (GObject *object)
{
	EComposerNameHeader *self = E_COMPOSER_NAME_HEADER (object);

	g_clear_object (&self->priv->name_selector);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_composer_name_header_parent_class)->dispose (object);
}

static void
composer_name_header_constructed (GObject *object)
{
	EComposerNameHeader *self;
	ENameSelectorModel *model;
	ENameSelectorEntry *entry;
	GList *sections;
	const gchar *label;

	/* Input widget must be set before chaining up. */

	self = E_COMPOSER_NAME_HEADER (object);
	g_return_if_fail (E_IS_NAME_SELECTOR (self->priv->name_selector));

	model = e_name_selector_peek_model (self->priv->name_selector);
	label = e_composer_header_get_label (E_COMPOSER_HEADER (object));
	g_return_if_fail (label != NULL);

	sections = e_name_selector_model_list_sections (model);
	self->priv->destination_index = g_list_length (sections);
	e_name_selector_model_add_section (model, label, label, NULL);
	g_list_foreach (sections, (GFunc) g_free, NULL);
	g_list_free (sections);

	entry = E_NAME_SELECTOR_ENTRY (
		e_name_selector_peek_section_list (
		self->priv->name_selector, label));

	e_name_selector_entry_set_contact_editor_func (
		entry, contact_editor_fudge_new);
	e_name_selector_entry_set_contact_list_editor_func (
		entry, contact_list_editor_fudge_new);

	g_signal_connect (
		entry, "changed",
		G_CALLBACK (composer_name_header_entry_changed_cb), object);
	g_signal_connect (
		entry, "query-tooltip",
		G_CALLBACK (composer_name_header_entry_query_tooltip_cb),
		NULL);
	E_COMPOSER_HEADER (object)->input_widget = GTK_WIDGET (g_object_ref_sink (entry));

	e_signal_connect_notify_swapped (
		object, "notify::visible",
		G_CALLBACK (composer_name_header_visible_changed_cb), object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_name_header_parent_class)->constructed (object);

	e_composer_header_set_title_tooltip (
		E_COMPOSER_HEADER (object),
		_("Click here for the address book"));
}

static void
composer_name_header_clicked (EComposerHeader *header)
{
	EComposerNameHeader *self;
	ENameSelectorDialog *dialog;

	self = E_COMPOSER_NAME_HEADER (header);

	dialog = e_name_selector_peek_dialog (self->priv->name_selector);
	e_name_selector_dialog_set_destination_index (
		dialog, self->priv->destination_index);
	e_name_selector_show_dialog (
		self->priv->name_selector, header->title_widget);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
e_composer_name_header_class_init (EComposerNameHeaderClass *class)
{
	GObjectClass *object_class;
	EComposerHeaderClass *header_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = composer_name_header_set_property;
	object_class->get_property = composer_name_header_get_property;
	object_class->dispose = composer_name_header_dispose;
	object_class->constructed = composer_name_header_constructed;

	header_class = E_COMPOSER_HEADER_CLASS (class);
	header_class->clicked = composer_name_header_clicked;

	g_object_class_install_property (
		object_class,
		PROP_NAME_SELECTOR,
		g_param_spec_object (
			"name-selector",
			NULL,
			NULL,
			E_TYPE_NAME_SELECTOR,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_composer_name_header_init (EComposerNameHeader *header)
{
	header->priv = e_composer_name_header_get_instance_private (header);
}

EComposerHeader *
e_composer_name_header_new (ESourceRegistry *registry,
                            const gchar *label,
                            ENameSelector *name_selector)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (E_IS_NAME_SELECTOR (name_selector), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_NAME_HEADER,
		"label", label, "button", TRUE,
		"name-selector", name_selector,
		"registry", registry, NULL);
}

ENameSelector *
e_composer_name_header_get_name_selector (EComposerNameHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_NAME_HEADER (header), NULL);

	return header->priv->name_selector;
}

EDestination **
e_composer_name_header_get_destinations (EComposerNameHeader *header)
{
	EDestinationStore *store;
	EDestination **destinations;
	ENameSelectorEntry *entry;
	GList *list, *iter;
	gint ii = 0;

	g_return_val_if_fail (E_IS_COMPOSER_NAME_HEADER (header), NULL);

	entry = E_COMPOSER_NAME_HEADER_GET_ENTRY (header);
	store = e_name_selector_entry_peek_destination_store (entry);

	list = e_destination_store_list_destinations (store);
	destinations = g_new0 (EDestination *, g_list_length (list) + 1);

	for (iter = list; iter != NULL; iter = iter->next)
		destinations[ii++] = g_object_ref (iter->data);

	g_list_free (list);

	/* free with e_destination_freev() */
	return destinations;
}

void
e_composer_name_header_add_destinations (EComposerNameHeader *header,
                                         EDestination **destinations)
{
	EDestinationStore *store;
	ENameSelectorEntry *entry;
	gint ii;

	g_return_if_fail (E_IS_COMPOSER_NAME_HEADER (header));

	entry = E_COMPOSER_NAME_HEADER_GET_ENTRY (header);
	store = e_name_selector_entry_peek_destination_store (entry);

	if (destinations == NULL)
		return;

	for (ii = 0; destinations[ii] != NULL; ii++)
		e_destination_store_append_destination (
			store, destinations[ii]);
}

void
e_composer_name_header_set_destinations (EComposerNameHeader *header,
                                         EDestination **destinations)
{
	EDestinationStore *store;
	ENameSelectorEntry *entry;
	GList *list, *iter;

	g_return_if_fail (E_IS_COMPOSER_NAME_HEADER (header));

	entry = E_COMPOSER_NAME_HEADER_GET_ENTRY (header);
	store = e_name_selector_entry_peek_destination_store (entry);

	/* Clear the destination store. */
	list = e_destination_store_list_destinations (store);
	for (iter = list; iter != NULL; iter = iter->next)
		e_destination_store_remove_destination (store, iter->data);
	g_list_free (list);

	e_composer_name_header_add_destinations (header, destinations);
}
