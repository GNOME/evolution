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

#include <string.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-misc-utils.h"
#include "e-categories-config.h"
#include "e-category-completion.h"

struct _ECategoryCompletionPrivate {
	GtkWidget *last_known_entry;
	gchar *create;
	gchar *prefix;

	gulong notify_cursor_position_id;
	gulong notify_text_id;
};

enum {
	COLUMN_PIXBUF,
	COLUMN_CATEGORY,
	COLUMN_NORMALIZED,
	NUM_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (ECategoryCompletion, e_category_completion, GTK_TYPE_ENTRY_COMPLETION)

/* Forward Declarations */

static void
category_completion_track_entry (GtkEntryCompletion *completion);

static void
category_completion_build_model (GtkEntryCompletion *completion)
{
	GtkListStore *store;
	GList *list;

	store = gtk_list_store_new (
		NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	list = e_categories_dup_list ();
	while (list != NULL) {
		const gchar *category = list->data;
		gchar *normalized;
		gchar *casefolded;
		GdkPixbuf *pixbuf = NULL;
		GtkTreeIter iter;

		/* Only add user-visible categories. */
		if (!e_categories_is_searchable (category)) {
			g_free (list->data);
			list = g_list_delete_link (list, list);
			continue;
		}

		if (!e_categories_config_get_icon_for (category, &pixbuf))
			pixbuf = NULL;

		normalized = g_utf8_normalize (
			category, -1, G_NORMALIZE_DEFAULT);
		casefolded = g_utf8_casefold (normalized, -1);

		gtk_list_store_append (store, &iter);

		gtk_list_store_set (
			store, &iter, COLUMN_PIXBUF, pixbuf,
			COLUMN_CATEGORY, category, COLUMN_NORMALIZED,
			casefolded, -1);

		g_free (normalized);
		g_free (casefolded);

		if (pixbuf != NULL)
			g_object_unref (pixbuf);

		g_free (list->data);
		list = g_list_delete_link (list, list);
	}

	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
}

static void
category_completion_categories_changed_cb (GObject *some_private_object,
                                           GtkEntryCompletion *completion)
{
	category_completion_build_model (completion);
}

static void
category_completion_complete (GtkEntryCompletion *completion,
                              const gchar *category)
{
	GtkEditable *editable;
	GtkWidget *entry;
	const gchar *text;
	const gchar *cp;
	gint start_pos;
	gint end_pos;
	glong offset;

	entry = gtk_entry_completion_get_entry (completion);

	editable = GTK_EDITABLE (entry);
	text = gtk_entry_get_text (GTK_ENTRY (entry));

	/* Get the cursor position as a character offset. */
	offset = gtk_editable_get_position (editable);

	/* Find the rightmost comma before the cursor. */
	cp = g_utf8_offset_to_pointer (text, offset);
	cp = g_utf8_strrchr (text, (gssize) (cp - text), ',');

	/* Calculate the selection start position as a character offset. */
	if (cp == NULL)
		offset = 0;
	else {
		cp = g_utf8_next_char (cp);
		if (g_unichar_isspace (g_utf8_get_char (cp)))
			cp = g_utf8_next_char (cp);
		offset = g_utf8_pointer_to_offset (text, cp);
	}
	start_pos = (gint) offset;

	/* Find the leftmost comma after the cursor. */
	cp = g_utf8_offset_to_pointer (text, offset);
	cp = g_utf8_strchr (cp, -1, ',');

	/* Calculate the selection end position as a character offset. */
	if (cp == NULL)
		offset = -1;
	else {
		cp = g_utf8_next_char (cp);
		if (g_unichar_isspace (g_utf8_get_char (cp)))
			cp = g_utf8_next_char (cp);
		offset = g_utf8_pointer_to_offset (text, cp);
	}
	end_pos = (gint) offset;

	/* Complete the partially typed category. */
	gtk_editable_delete_text (editable, start_pos, end_pos);
	gtk_editable_insert_text (editable, category, -1, &start_pos);
	gtk_editable_insert_text (editable, ",", 1, &start_pos);
	gtk_editable_set_position (editable, start_pos);
}

static gboolean
category_completion_is_match (GtkEntryCompletion *completion,
                              const gchar *key,
                              GtkTreeIter *iter)
{
	ECategoryCompletion *self;
	GtkTreeModel *model;
	GtkWidget *entry;
	GValue value = { 0, };
	gboolean match;

	self = E_CATEGORY_COMPLETION (completion);
	entry = gtk_entry_completion_get_entry (completion);
	model = gtk_entry_completion_get_model (completion);

	/* XXX This would be easier if GtkEntryCompletion had an 'entry'
	 *     property that we could listen to for notifications. */
	if (entry != self->priv->last_known_entry)
		category_completion_track_entry (completion);

	if (!self->priv->prefix)
		return FALSE;

	gtk_tree_model_get_value (model, iter, COLUMN_NORMALIZED, &value);
	match = g_str_has_prefix (g_value_get_string (&value), self->priv->prefix);
	g_value_unset (&value);

	return match;
}

static void
category_completion_update_prefix (GtkEntryCompletion *completion)
{
	ECategoryCompletion *self;
	GtkEditable *editable;
	GtkTreeModel *model;
	GtkWidget *entry;
	GtkTreeIter iter;
	const gchar *text;
	const gchar *start;
	const gchar *end;
	const gchar *cp;
	gboolean valid;
	gchar *input;
	glong offset;

	self = E_CATEGORY_COMPLETION (completion);
	entry = gtk_entry_completion_get_entry (completion);
	model = gtk_entry_completion_get_model (completion);

	/* XXX This would be easier if GtkEntryCompletion had an 'entry'
	 *     property that we could listen to for notifications. */
	if (entry != self->priv->last_known_entry) {
		category_completion_track_entry (completion);
		return;
	}

	editable = GTK_EDITABLE (entry);
	text = gtk_entry_get_text (GTK_ENTRY (entry));

	/* Get the cursor position as a character offset. */
	offset = gtk_editable_get_position (editable);

	/* Find the rightmost comma before the cursor. */
	cp = g_utf8_offset_to_pointer (text, offset);
	cp = g_utf8_strrchr (text, (gsize) (cp - text), ',');

	/* Mark the start of the prefix. */
	if (cp == NULL)
		start = text;
	else {
		cp = g_utf8_next_char (cp);
		if (g_unichar_isspace (g_utf8_get_char (cp)))
			cp = g_utf8_next_char (cp);
		start = cp;
	}

	/* Find the leftmost comma after the cursor. */
	cp = g_utf8_offset_to_pointer (text, offset);
	cp = g_utf8_strchr (cp, -1, ',');

	/* Mark the end of the prefix. */
	if (cp == NULL)
		end = text + strlen (text);
	else
		end = cp;

	if (self->priv->create != NULL)
		gtk_entry_completion_delete_action (completion, 0);

	g_clear_pointer (&self->priv->create, g_free);
	g_clear_pointer (&self->priv->prefix, g_free);

	if (start == end)
		return;

	input = g_strstrip (g_strndup (start, end - start));
	self->priv->create = input;

	input = g_utf8_normalize (input, -1, G_NORMALIZE_DEFAULT);
	self->priv->prefix = g_utf8_casefold (input, -1);
	g_free (input);

	if (*self->priv->create == '\0') {
		g_clear_pointer (&self->priv->create, g_free);
		return;
	}

	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
		GValue value = { 0, };

		gtk_tree_model_get_value (
			model, &iter, COLUMN_NORMALIZED, &value);
		if (strcmp (g_value_get_string (&value), self->priv->prefix) == 0) {
			g_value_unset (&value);
			g_clear_pointer (&self->priv->create, g_free);
			return;
		}
		g_value_unset (&value);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	input = g_strdup_printf (_("Create category “%s”"), self->priv->create);
	gtk_entry_completion_insert_action_text (completion, 0, input);
	g_free (input);
}

static gboolean
category_completion_sanitize_suffix (GtkEntry *entry,
                                     GdkEventFocus *event,
                                     GtkEntryCompletion *completion)
{
	const gchar *text;

	g_return_val_if_fail (entry != NULL, FALSE);
	g_return_val_if_fail (completion != NULL, FALSE);

	text = gtk_entry_get_text (entry);
	if (text) {
		gint len = strlen (text), old_len = len;

		while (len > 0 && (text[len -1] == ' ' || text[len - 1] == ','))
			len--;

		if (old_len != len) {
			gchar *tmp = g_strndup (text, len);

			gtk_entry_set_text (entry, tmp);

			g_free (tmp);
		}
	}

	return FALSE;
}

static void
category_completion_track_entry (GtkEntryCompletion *completion)
{
	ECategoryCompletion *self = E_CATEGORY_COMPLETION (completion);

	if (self->priv->last_known_entry != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->last_known_entry, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, completion);
		e_signal_disconnect_notify_handler (self->priv->last_known_entry, &self->priv->notify_cursor_position_id);
		e_signal_disconnect_notify_handler (self->priv->last_known_entry, &self->priv->notify_text_id);
		g_clear_object (&self->priv->last_known_entry);
	}

	g_clear_pointer (&self->priv->prefix, g_free);

	self->priv->last_known_entry = gtk_entry_completion_get_entry (completion);
	if (!self->priv->last_known_entry)
		return;

	g_object_ref (self->priv->last_known_entry);

	self->priv->notify_cursor_position_id = e_signal_connect_notify_swapped (
		self->priv->last_known_entry, "notify::cursor-position",
		G_CALLBACK (category_completion_update_prefix), completion);

	self->priv->notify_text_id = e_signal_connect_notify_swapped (
		self->priv->last_known_entry, "notify::text",
		G_CALLBACK (category_completion_update_prefix), completion);

	g_signal_connect (
		self->priv->last_known_entry, "focus-out-event",
		G_CALLBACK (category_completion_sanitize_suffix), completion);

	category_completion_update_prefix (completion);
}

static void
category_completion_constructed (GObject *object)
{
	GtkCellRenderer *renderer;
	GtkEntryCompletion *completion;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_category_completion_parent_class)->constructed (object);

	completion = GTK_ENTRY_COMPLETION (object);

	gtk_entry_completion_set_match_func (
		completion, (GtkEntryCompletionMatchFunc)
		category_completion_is_match, NULL, NULL);

	gtk_entry_completion_set_text_column (completion, COLUMN_CATEGORY);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (completion), renderer, FALSE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (completion),
		renderer, "pixbuf", COLUMN_PIXBUF);
	gtk_cell_layout_reorder (
		GTK_CELL_LAYOUT (completion), renderer, 0);

	e_categories_register_change_listener (
		G_CALLBACK (category_completion_categories_changed_cb),
		completion);

	category_completion_build_model (completion);
}

static void
category_completion_dispose (GObject *object)
{
	ECategoryCompletion *self = E_CATEGORY_COMPLETION (object);

	if (self->priv->last_known_entry != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->last_known_entry, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		e_signal_disconnect_notify_handler (self->priv->last_known_entry, &self->priv->notify_cursor_position_id);
		e_signal_disconnect_notify_handler (self->priv->last_known_entry, &self->priv->notify_text_id);
		g_clear_object (&self->priv->last_known_entry);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_category_completion_parent_class)->dispose (object);
}

static void
category_completion_finalize (GObject *object)
{
	ECategoryCompletion *self = E_CATEGORY_COMPLETION (object);

	g_free (self->priv->create);
	g_free (self->priv->prefix);

	e_categories_unregister_change_listener (
		G_CALLBACK (category_completion_categories_changed_cb),
		object);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_category_completion_parent_class)->finalize (object);
}

static gboolean
category_completion_match_selected (GtkEntryCompletion *completion,
                                    GtkTreeModel *model,
                                    GtkTreeIter *iter)
{
	GValue value = { 0, };

	gtk_tree_model_get_value (model, iter, COLUMN_CATEGORY, &value);
	category_completion_complete (completion, g_value_get_string (&value));
	g_value_unset (&value);

	return TRUE;
}

static void
category_completion_action_activated (GtkEntryCompletion *completion,
                                      gint index)
{
	ECategoryCompletion *self;
	gchar *category;

	self = E_CATEGORY_COMPLETION (completion);

	category = g_strdup (self->priv->create);
	e_categories_add (category, NULL, NULL, TRUE);
	category_completion_complete (completion, category);
	g_free (category);
}

static void
e_category_completion_class_init (ECategoryCompletionClass *class)
{
	GObjectClass *object_class;
	GtkEntryCompletionClass *entry_completion_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = category_completion_constructed;
	object_class->dispose = category_completion_dispose;
	object_class->finalize = category_completion_finalize;

	entry_completion_class = GTK_ENTRY_COMPLETION_CLASS (class);
	entry_completion_class->match_selected = category_completion_match_selected;
	entry_completion_class->action_activated = category_completion_action_activated;
}

static void
e_category_completion_init (ECategoryCompletion *category_completion)
{
	category_completion->priv = e_category_completion_get_instance_private (category_completion);
}

/**
 * e_category_completion_new:
 *
 * Since: 2.26
 **/
GtkEntryCompletion *
e_category_completion_new (void)
{
	return g_object_new (E_TYPE_CATEGORY_COMPLETION, NULL);
}
