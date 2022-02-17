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
 *
 * Authors:
 *		Milan Crha <mcrha@redhat.com>
 *
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <string.h>

#include "e-focus-tracker.h"
#include "e-widget-undo.h"

#define DEFAULT_MAX_UNDO_LEVEL 256
#define UNDO_DATA_KEY "e-undo-data-ptr"

/* calculates real index in EUndoData::undo_stack */
#define REAL_INDEX(x) ((data->undo_from + (x) + 2 * data->undo_len) % data->undo_len)

typedef enum {
	E_UNDO_INSERT,
	E_UNDO_DELETE,
	E_UNDO_GROUP
} EUndoType;

typedef enum {
	E_UNDO_DO_UNDO,
	E_UNDO_DO_REDO
} EUndoDoType;

typedef struct _EUndoInfo {
	EUndoType type;
	union _data {
		gchar *text;
		GPtrArray *group; /* EUndoInfo */
	} data;
	gint position_start;
	gint position_end; /* valid for delete type only */
} EUndoInfo;

typedef struct _EUndoData {
	EUndoInfo **undo_stack; /* stack for undo, with max_undo_level elements, some are NULL */
	gint undo_len; /* how many undo actions can be saved */
	gint undo_from; /* where the first undo action begins */
	gint n_undos; /* how many undo actions are saved;
		[(undo_from + n_undos) % undo_len] is the next free undo item (or the first redo) */
	gint n_redos; /* how many redo actions are saved */

	EUndoInfo *current_info; /* the top undo action */

	gulong insert_handler_id;
	gulong delete_handler_id;

	guint user_action_counter;
	GPtrArray *user_action_array; /* EUndoInfo * */
} EUndoData;

static void
free_undo_info (gpointer ptr)
{
	EUndoInfo *info = ptr;

	if (info) {
		if (info->type == E_UNDO_GROUP) {
			if (info->data.group)
				g_ptr_array_free (info->data.group, TRUE);
		} else {
			g_free (info->data.text);
		}
		g_free (info);
	}
}

static void
free_undo_data (gpointer ptr)
{
	EUndoData *data = ptr;

	if (data) {
		gint ii;

		if (data->user_action_array)
			g_ptr_array_free (data->user_action_array, TRUE);

		for (ii = 0; ii < data->undo_len; ii++) {
			free_undo_info (data->undo_stack[ii]);
		}
		g_free (data->undo_stack);
		g_free (data);
	}
}

static void
reset_redos (EUndoData *data)
{
	gint ii, index;

	for (ii = 0; ii < data->n_redos; ii++) {
		index = REAL_INDEX (data->n_undos + ii);

		free_undo_info (data->undo_stack[index]);
		data->undo_stack[index] = NULL;
	}

	data->n_redos = 0;
}

static void
push_undo (EUndoData *data,
           EUndoInfo *info)
{
	gint index;

	if (data->user_action_counter) {
		g_ptr_array_add (data->user_action_array, info);
		return;
	}

	reset_redos (data);

	if (data->n_undos == data->undo_len) {
		data->undo_from = (data->undo_from + 1) % data->undo_len;
	} else {
		data->n_undos++;
	}

	index = REAL_INDEX (data->n_undos - 1);
	free_undo_info (data->undo_stack[index]);
	data->undo_stack[index] = info;
}

static gboolean
can_merge_insert_undos (EUndoInfo *current_info,
                        const gchar *text,
                        gint text_len,
                        gint position)
{
	gint len;

	/* allow only one letter merge */
	if (!current_info || current_info->type != E_UNDO_INSERT ||
	    !text || text_len <= 0 || text_len > 1)
		return FALSE;

	if (text[0] == '\r' || text[0] == '\n')
		return FALSE;

	len = strlen (current_info->data.text);
	if (position != current_info->position_start + len)
		return FALSE;

	if (g_ascii_isspace (text[0])) {
		if (len <= 0 || !g_ascii_isspace (current_info->data.text[len - 1]))
			return FALSE;
	}

	return TRUE;
}

static void
push_insert_undo (GObject *object,
                  const gchar *text,
                  gint text_len,
                  gint position)
{
	EUndoData *data;
	EUndoInfo *info;

	data = g_object_get_data (object, UNDO_DATA_KEY);
	if (!data) {
		g_warn_if_reached ();
		return;
	}

	/* one letter long text, divide undos on spaces */
	if (data->current_info &&
	    can_merge_insert_undos (data->current_info, text, text_len, position)) {
		gchar *new_text;

		new_text = g_strdup_printf ("%s%*s", data->current_info->data.text, text_len, text);
		g_free (data->current_info->data.text);
		data->current_info->data.text = new_text;

		return;
	}

	info = g_new0 (EUndoInfo, 1);
	info->type = E_UNDO_INSERT;
	info->data.text = g_strndup (text, text_len);
	info->position_start = position;

	push_undo (data, info);

	data->current_info = info;
}

static void
push_delete_undo (GObject *object,
                  gchar *text, /* takes ownership of the 'text' */
                  gint position_start,
                  gint position_end)
{
	EUndoData *data;
	EUndoInfo *info;

	data = g_object_get_data (object, UNDO_DATA_KEY);
	if (!data) {
		g_warn_if_reached ();
		return;
	}

	if (data->current_info && data->current_info->type == E_UNDO_DELETE &&
	    position_end - position_start == 1 && !g_ascii_isspace (*text)) {
		info = data->current_info;

		if (info->position_start == position_start) {
			gchar *new_text;

			new_text = g_strconcat (info->data.text, text, NULL);
			g_free (info->data.text);
			info->data.text = new_text;
			g_free (text);

			info->position_end++;

			return;
		} else if (data->current_info->position_start == position_end) {
			gchar *new_text;

			new_text = g_strconcat (text, info->data.text, NULL);
			g_free (info->data.text);
			info->data.text = new_text;
			g_free (text);

			info->position_start = position_start;

			return;
		}
	}

	info = g_new0 (EUndoInfo, 1);
	info->type = E_UNDO_DELETE;
	info->data.text = text;
	info->position_start = position_start;
	info->position_end = position_end;

	push_undo (data, info);

	data->current_info = info;
}

static void
editable_undo_insert_text_cb (GtkEditable *editable,
                              gchar *text,
                              gint text_length,
                              gint *position,
                              gpointer user_data)
{
	push_insert_undo (G_OBJECT (editable), text, text_length, *position);
}

static void
editable_undo_delete_text_cb (GtkEditable *editable,
                              gint start_pos,
                              gint end_pos,
                              gpointer user_data)
{
	push_delete_undo (G_OBJECT (editable), gtk_editable_get_chars (editable, start_pos, end_pos), start_pos, end_pos);
}

static void
editable_undo_insert_text (GObject *object,
                           const gchar *text,
                           gint position)
{
	g_return_if_fail (GTK_IS_EDITABLE (object));

	gtk_editable_insert_text (GTK_EDITABLE (object), text, -1, &position);
}

static void
editable_undo_delete_text (GObject *object,
                           gint position_start,
                           gint position_end)
{
	g_return_if_fail (GTK_IS_EDITABLE (object));

	gtk_editable_delete_text (GTK_EDITABLE (object), position_start, position_end);
}

static void
text_buffer_undo_insert_text_cb (GtkTextBuffer *text_buffer,
                                 GtkTextIter *location,
                                 gchar *text,
                                 gint text_length,
                                 gpointer user_data)
{
	push_insert_undo (G_OBJECT (text_buffer), text, text_length, gtk_text_iter_get_offset (location));
}

static void
text_buffer_undo_delete_range_cb (GtkTextBuffer *text_buffer,
                                  GtkTextIter *start,
                                  GtkTextIter *end,
                                  gpointer user_data)
{
	push_delete_undo (
		G_OBJECT (text_buffer),
		gtk_text_iter_get_text (start, end),
		gtk_text_iter_get_offset (start),
		gtk_text_iter_get_offset (end));
}

static void
text_buffer_undo_begin_user_action_cb (GtkTextBuffer *text_buffer,
				       gpointer user_data)
{
	EUndoData *data;

	data = g_object_get_data (G_OBJECT (text_buffer), UNDO_DATA_KEY);

	if (!data)
		return;

	data->user_action_counter++;

	if (data->user_action_counter == 1 && !data->user_action_array)
		data->user_action_array = g_ptr_array_new_with_free_func (free_undo_info);
}

static void
text_buffer_undo_end_user_action_cb (GtkTextBuffer *text_buffer,
				     gpointer user_data)
{
	EUndoData *data;

	data = g_object_get_data (G_OBJECT (text_buffer), UNDO_DATA_KEY);

	if (!data || !data->user_action_counter)
		return;

	data->user_action_counter--;

	if (!data->user_action_counter && data->user_action_array && data->user_action_array->len) {
		EUndoInfo *info;

		if (data->user_action_array->len == 1) {
			info = g_ptr_array_steal_index (data->user_action_array, 0);
			data->current_info = info;
		} else {
			info = g_new0 (EUndoInfo, 1);
			info->type = E_UNDO_GROUP;
			info->data.group = data->user_action_array;

			data->user_action_array = NULL;
			data->current_info = NULL;
		}

		push_undo (data, info);
	}
}

static void
text_buffer_undo_insert_text (GObject *object,
                              const gchar *text,
                              gint position)
{
	GtkTextBuffer *text_buffer;
	GtkTextIter iter;

	g_return_if_fail (GTK_IS_TEXT_BUFFER (object));

	text_buffer = GTK_TEXT_BUFFER (object);

	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, position);
	gtk_text_buffer_insert (text_buffer, &iter, text, -1);
}

static void
text_buffer_undo_delete_text (GObject *object,
                              gint position_start,
                              gint position_end)
{
	GtkTextBuffer *text_buffer;
	GtkTextIter start_iter, end_iter;

	g_return_if_fail (GTK_IS_TEXT_BUFFER (object));

	text_buffer = GTK_TEXT_BUFFER (object);

	gtk_text_buffer_get_iter_at_offset (text_buffer, &start_iter, position_start);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end_iter, position_end);
	gtk_text_buffer_delete (text_buffer, &start_iter, &end_iter);
}

static void
widget_undo_place_cursor_at (GObject *object,
                             gint char_pos)
{
	if (GTK_IS_EDITABLE (object))
		gtk_editable_set_position (GTK_EDITABLE (object), char_pos);
	else if (GTK_IS_TEXT_BUFFER (object)) {
		GtkTextBuffer *buffer;
		GtkTextIter pos;

		buffer = GTK_TEXT_BUFFER (object);

		gtk_text_buffer_get_iter_at_offset (buffer, &pos, char_pos);
		gtk_text_buffer_place_cursor (buffer, &pos);
	}
}

static void
undo_apply_info (EUndoInfo *info,
		 GObject *object,
		 EUndoDoType todo,
		 void (* insert_func) (GObject *object,
			const gchar *text,
			gint position),
		 void (* delete_func) (GObject *object,
			gint position_start,
			gint position_end))
{
	if (info->type == E_UNDO_INSERT) {
		if (todo == E_UNDO_DO_UNDO) {
			delete_func (object, info->position_start, info->position_start + g_utf8_strlen (info->data.text, -1));
			widget_undo_place_cursor_at (object, info->position_start);
		} else {
			insert_func (object, info->data.text, info->position_start);
			widget_undo_place_cursor_at (object, info->position_start + g_utf8_strlen (info->data.text, -1));
		}
	} else if (info->type == E_UNDO_DELETE) {
		if (todo == E_UNDO_DO_UNDO) {
			insert_func (object, info->data.text, info->position_start);
			widget_undo_place_cursor_at (object, info->position_start + g_utf8_strlen (info->data.text, -1));
		} else {
			delete_func (object, info->position_start, info->position_end);
			widget_undo_place_cursor_at (object, info->position_start);
		}
	} else if (info->type == E_UNDO_GROUP) {
		guint ii;

		for (ii = 0; ii < info->data.group->len; ii++) {
			EUndoInfo *info2;

			if (todo == E_UNDO_DO_UNDO)
				info2 = g_ptr_array_index (info->data.group, info->data.group->len - ii - 1);
			else
				info2 = g_ptr_array_index (info->data.group, ii);

			if (!info2)
				continue;

			undo_apply_info (info2, object, todo, insert_func, delete_func);
		}
	}
}

static void
undo_do_something (GObject *object,
                   EUndoDoType todo,
                   void (* insert_func) (GObject *object,
                   const gchar *text,
                   gint position),
                   void (* delete_func) (GObject *object,
                   gint position_start,
                   gint position_end))
{
	EUndoData *data;
	EUndoInfo *info = NULL;

	data = g_object_get_data (object, UNDO_DATA_KEY);
	if (!data)
		return;

	if (todo == E_UNDO_DO_UNDO && data->n_undos > 0) {
		info = data->undo_stack[REAL_INDEX (data->n_undos - 1)];
		data->n_undos--;
		data->n_redos++;
	} else if (todo == E_UNDO_DO_REDO && data->n_redos > 0) {
		info = data->undo_stack[REAL_INDEX (data->n_undos)];
		data->n_undos++;
		data->n_redos--;
	}

	if (!info)
		return;

	g_signal_handler_block (object, data->insert_handler_id);
	g_signal_handler_block (object, data->delete_handler_id);

	undo_apply_info (info, object, todo, insert_func, delete_func);

	data->current_info = NULL;

	g_signal_handler_unblock (object, data->delete_handler_id);
	g_signal_handler_unblock (object, data->insert_handler_id);
}

static gchar *
undo_describe_info (EUndoInfo *info,
                    EUndoDoType undo_type)
{
	if (!info)
		return NULL;

	if (info->type == E_UNDO_INSERT) {
		if (undo_type == E_UNDO_DO_UNDO)
			return g_strdup (_("Undo “Insert text”"));
		else
			return g_strdup (_("Redo “Insert text”"));
		/* if (strlen (info->data.text) > 15) {
			if (undo_type == E_UNDO_DO_UNDO)
				return g_strdup_printf (_("Undo “Insert “%.12s...””"), info->data.text);
			else
				return g_strdup_printf (_("Redo “Insert “%.12s...””"), info->data.text);
		}

		if (undo_type == E_UNDO_DO_UNDO)
			return g_strdup_printf (_("Undo “Insert “%s””"), info->data.text);
		else
			return g_strdup_printf (_("Redo “Insert “%s””"), info->data.text); */
	} else if (info->type == E_UNDO_DELETE) {
		if (undo_type == E_UNDO_DO_UNDO)
			return g_strdup (_("Undo “Delete text”"));
		else
			return g_strdup (_("Redo “Delete text”"));
		/* if (strlen (info->data.text) > 15) {
			if (undo_type == E_UNDO_DO_UNDO)
				return g_strdup_printf (_("Undo “Delete “%.12s...””"), info->data.text);
			else
				return g_strdup_printf (_("Redo “Delete “%.12s...””"), info->data.text);
		}

		if (undo_type == E_UNDO_DO_UNDO)
			return g_strdup_printf (_("Undo “Delete “%s””"), info->data.text);
		else
			return g_strdup_printf (_("Redo “Delete “%s””"), info->data.text); */
	}

	return NULL;
}

static gboolean
undo_check_undo (GObject *object,
                 gchar **description)
{
	EUndoData *data;

	data = g_object_get_data (object, UNDO_DATA_KEY);
	if (!data)
		return FALSE;

	if (data->n_undos <= 0)
		return FALSE;

	if (description)
		*description = undo_describe_info (data->undo_stack[REAL_INDEX (data->n_undos - 1)], E_UNDO_DO_UNDO);

	return TRUE;
}

static gboolean
undo_check_redo (GObject *object,
                 gchar **description)
{
	EUndoData *data;

	data = g_object_get_data (object, UNDO_DATA_KEY);
	if (!data)
		return FALSE;

	if (data->n_redos <= 0)
		return FALSE;

	if (description)
		*description = undo_describe_info (data->undo_stack[REAL_INDEX (data->n_undos)], E_UNDO_DO_REDO);

	return TRUE;
}

static void
undo_reset (GObject *object)
{
	EUndoData *data;

	data = g_object_get_data (object, UNDO_DATA_KEY);
	if (!data)
		return;

	data->n_undos = 0;
	data->n_redos = 0;
	data->current_info = NULL;
}

static void
widget_undo_popup_activate_cb (GObject *menu_item,
                               GtkWidget *widget)
{
	EUndoDoType undo_type = GPOINTER_TO_INT (g_object_get_data (menu_item, UNDO_DATA_KEY));

	if (undo_type == E_UNDO_DO_UNDO)
		e_widget_undo_do_undo (widget);
	else
		e_widget_undo_do_redo (widget);
}

static gboolean
widget_undo_prepend_popup (GtkWidget *widget,
                           GtkMenuShell *menu,
                           EUndoDoType undo_type,
                           gboolean already_added)
{
	gchar *description = NULL;
	const gchar *icon_name = NULL;

	if (undo_type == E_UNDO_DO_UNDO && e_widget_undo_has_undo (widget)) {
		description = e_widget_undo_describe_undo (widget);
		icon_name = "edit-undo";
	} else if (undo_type == E_UNDO_DO_REDO && e_widget_undo_has_redo (widget)) {
		description = e_widget_undo_describe_redo (widget);
		icon_name = "edit-redo";
	}

	if (description) {
		GtkWidget *item, *image;

		if (!already_added) {
			item = gtk_separator_menu_item_new ();
			gtk_widget_show (item);
			gtk_menu_shell_prepend (menu, item);

			already_added = TRUE;
		}

		image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
		item = gtk_image_menu_item_new_with_label (description);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		gtk_widget_show (item);

		g_object_set_data (G_OBJECT (item), UNDO_DATA_KEY, GINT_TO_POINTER (undo_type));
		g_signal_connect (item, "activate", G_CALLBACK (widget_undo_popup_activate_cb), widget);

		gtk_menu_shell_prepend (menu, item);

		g_free (description);
	}

	return already_added;
}

static void
widget_undo_populate_popup_cb (GtkWidget *widget,
                               GtkWidget *popup,
                               gpointer user_data)
{
	GtkMenuShell *menu;
	gboolean added = FALSE;

	if (!GTK_IS_MENU (popup))
		return;

	menu = GTK_MENU_SHELL (popup);

	/* first redo, because prependend, thus undo gets before it */
	if (e_widget_undo_has_redo (widget))
		added = widget_undo_prepend_popup (widget, menu, E_UNDO_DO_REDO, added);

	if (e_widget_undo_has_undo (widget))
		widget_undo_prepend_popup (widget, menu, E_UNDO_DO_UNDO, added);
}

/**
 * e_widget_undo_attach:
 * @widget: a #GtkWidget, where to attach undo functionality
 * @focus_tracker: an #EFocusTracker, can be %NULL
 *
 * The function does nothing, if the widget is not of a supported type
 * for undo functionality, same as when the undo is already attached.
 * It is ensured that the actions of the provided @focus_tracker are
 * updated on change of the @widget.
 *
 * See @e_widget_undo_is_attached().
 *
 * Since: 3.12
 **/
void
e_widget_undo_attach (GtkWidget *widget,
                      EFocusTracker *focus_tracker)
{
	EUndoData *data;

	if (e_widget_undo_is_attached (widget))
		return;

	if (GTK_IS_EDITABLE (widget)) {
		data = g_new0 (EUndoData, 1);
		data->undo_len = DEFAULT_MAX_UNDO_LEVEL;
		data->undo_stack = g_new0 (EUndoInfo *, data->undo_len);

		g_object_set_data_full (G_OBJECT (widget), UNDO_DATA_KEY, data, free_undo_data);

		data->insert_handler_id = g_signal_connect (
			widget, "insert-text",
			G_CALLBACK (editable_undo_insert_text_cb), NULL);
		data->delete_handler_id = g_signal_connect (
			widget, "delete-text",
			G_CALLBACK (editable_undo_delete_text_cb), NULL);

		if (focus_tracker)
			g_signal_connect_swapped (
				widget, "changed",
				G_CALLBACK (e_focus_tracker_update_actions), focus_tracker);

		if (GTK_IS_ENTRY (widget))
			g_signal_connect (
				widget, "populate-popup",
				G_CALLBACK (widget_undo_populate_popup_cb), NULL);
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

		data = g_new0 (EUndoData, 1);
		data->undo_len = DEFAULT_MAX_UNDO_LEVEL;
		data->undo_stack = g_new0 (EUndoInfo *, data->undo_len);

		g_object_set_data_full (G_OBJECT (text_buffer), UNDO_DATA_KEY, data, free_undo_data);

		data->insert_handler_id = g_signal_connect (
			text_buffer, "insert-text",
			G_CALLBACK (text_buffer_undo_insert_text_cb), NULL);
		data->delete_handler_id = g_signal_connect (
			text_buffer, "delete-range",
			G_CALLBACK (text_buffer_undo_delete_range_cb), NULL);
		g_signal_connect (text_buffer, "begin-user-action",
			G_CALLBACK (text_buffer_undo_begin_user_action_cb), NULL);
		g_signal_connect (text_buffer, "end-user-action",
			G_CALLBACK (text_buffer_undo_end_user_action_cb), NULL);

		if (focus_tracker)
			g_signal_connect_swapped (
				text_buffer, "changed",
				G_CALLBACK (e_focus_tracker_update_actions), focus_tracker);

		g_signal_connect (
			widget, "populate-popup",
			G_CALLBACK (widget_undo_populate_popup_cb), NULL);
	}
}

/**
 * e_widget_undo_is_attached:
 * @widget: a #GtkWidget, where to test whether undo functionality is attached.
 *
 * Checks whether the given widget has already attached an undo
 * functionality - it is done with @e_widget_undo_attach().
 *
 * Returns: Whether the given @widget has already attached undo functionality.
 *
 * Since: 3.12
 **/
gboolean
e_widget_undo_is_attached (GtkWidget *widget)
{
	gboolean res = FALSE;

	if (GTK_IS_EDITABLE (widget)) {
		res = g_object_get_data (G_OBJECT (widget), UNDO_DATA_KEY) != NULL;
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

		res = g_object_get_data (G_OBJECT (text_buffer), UNDO_DATA_KEY) != NULL;
	}

	return res;
}

/**
 * e_widget_undo_has_undo:
 * @widget: a #GtkWidget
 *
 * Returns: Whether the given @widget has any undo available.
 *
 * See: @e_widget_undo_describe_undo, @e_widget_undo_do_undo
 *
 * Since: 3.12
 **/
gboolean
e_widget_undo_has_undo (GtkWidget *widget)
{
	if (GTK_IS_EDITABLE (widget)) {
		return undo_check_undo (G_OBJECT (widget), NULL);
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

		return undo_check_undo (G_OBJECT (text_buffer), NULL);
	}

	return FALSE;
}

/**
 * e_widget_undo_has_redo:
 * @widget: a #GtkWidget
 *
 * Returns: Whether the given @widget has any redo available.
 *
 * See: @e_widget_undo_describe_redo, @e_widget_undo_do_redo
 *
 * Since: 3.12
 **/
gboolean
e_widget_undo_has_redo (GtkWidget *widget)
{
	if (GTK_IS_EDITABLE (widget)) {
		return undo_check_redo (G_OBJECT (widget), NULL);
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

		return undo_check_redo (G_OBJECT (text_buffer), NULL);
	}

	return FALSE;
}

/**
 * e_widget_undo_describe_undo:
 * @widget: a #GtkWidget
 *
 * Returns: (transfer full): Description of a top undo action available
 *    for the @widget, %NULL when there is no undo action. Returned pointer,
 *    if not %NULL, should be freed with g_free().
 *
 * See: @e_widget_undo_has_undo, @e_widget_undo_do_undo
 *
 * Since: 3.12
 **/
gchar *
e_widget_undo_describe_undo (GtkWidget *widget)
{
	gchar *res = NULL;

	if (GTK_IS_EDITABLE (widget)) {
		if (!undo_check_undo (G_OBJECT (widget), &res)) {
			g_warn_if_fail (res == NULL);
		}
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

		if (!undo_check_undo (G_OBJECT (text_buffer), &res)) {
			g_warn_if_fail (res == NULL);
		}
	}

	return res;
}

/**
 * e_widget_undo_describe_redo:
 * @widget: a #GtkWidget
 *
 * Returns: (transfer full): Description of a top redo action available
 *    for the @widget, %NULL when there is no redo action. Returned pointer,
 *    if not %NULL, should be freed with g_free().
 *
 * See: @e_widget_undo_has_redo, @e_widget_undo_do_redo
 *
 * Since: 3.12
 **/
gchar *
e_widget_undo_describe_redo (GtkWidget *widget)
{
	gchar *res = NULL;

	if (GTK_IS_EDITABLE (widget)) {
		if (!undo_check_redo (G_OBJECT (widget), &res)) {
			g_warn_if_fail (res == NULL);
		}
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

		if (!undo_check_redo (G_OBJECT (text_buffer), &res)) {
			g_warn_if_fail (res == NULL);
		}
	}

	return res;
}

/**
 * e_widget_undo_do_undo:
 * @widget: a #GtkWidget
 *
 * Applies the top undo action on the @widget, which also remembers
 * a redo action. It does nothing if the widget doesn't have
 * attached undo functionality (@e_widget_undo_attach()), neither
 * when there is no undo action available.
 *
 * See: @e_widget_undo_attach, @e_widget_undo_has_undo, @e_widget_undo_describe_undo
 *
 * Since: 3.12
 **/
void
e_widget_undo_do_undo (GtkWidget *widget)
{
	if (GTK_IS_EDITABLE (widget)) {
		undo_do_something (
			G_OBJECT (widget),
			E_UNDO_DO_UNDO,
			editable_undo_insert_text,
			editable_undo_delete_text);
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

		undo_do_something (
			G_OBJECT (text_buffer),
			E_UNDO_DO_UNDO,
			text_buffer_undo_insert_text,
			text_buffer_undo_delete_text);
	}
}

/**
 * e_widget_undo_do_redo:
 * @widget: a #GtkWidget
 *
 * Applies the top redo action on the @widget, which also remembers
 * an undo action. It does nothing if the widget doesn't have
 * attached undo functionality (@e_widget_undo_attach()), neither
 * when there is no redo action available.
 *
 * See: @e_widget_undo_attach, @e_widget_undo_has_redo, @e_widget_undo_describe_redo
 *
 * Since: 3.12
 **/
void
e_widget_undo_do_redo (GtkWidget *widget)
{
	if (GTK_IS_EDITABLE (widget)) {
		undo_do_something (
			G_OBJECT (widget),
			E_UNDO_DO_REDO,
			editable_undo_insert_text,
			editable_undo_delete_text);
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

		undo_do_something (
			G_OBJECT (text_buffer),
			E_UNDO_DO_REDO,
			text_buffer_undo_insert_text,
			text_buffer_undo_delete_text);
	}
}

/**
 * e_widget_undo_reset:
 * @widget: a #GtkWidget, on which might be attached undo functionality
 *
 * Resets undo and redo stack to empty on a widget with attached
 * undo functionality. It does nothing, if the widget does not have
 * the undo functionality attached (see @e_widget_undo_attach()).
 *
 * Since: 3.12
 **/
void
e_widget_undo_reset (GtkWidget *widget)
{
	if (GTK_IS_EDITABLE (widget)) {
		undo_reset (G_OBJECT (widget));
	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

		undo_reset (G_OBJECT (text_buffer));
	}
}
