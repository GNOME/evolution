/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libedataserverui/libedataserverui.h>

#include "e-content-editor.h"
#include "e-markdown-utils.h"
#include "e-misc-utils.h"
#include "e-spell-text-view.h"
#include "e-web-view.h"
#include "e-widget-undo.h"

#include "e-markdown-editor.h"

/* Where an existing signature is */
#define EVO_SIGNATURE_START_MARK "x-evo-signature-start"
#define EVO_SIGNATURE_END_MARK "x-evo-signature-end"

struct _EMarkdownEditorPrivate {
	GtkNotebook *notebook;
	GtkTextView *text_view;
	EWebView *web_view;
	GtkToolbar *action_toolbar;
	gboolean is_dark_theme;

	/* EContentEditor properties */
	gboolean can_copy;
	gboolean can_cut;
	gboolean can_paste;
	gboolean can_redo;
	gboolean can_undo;
	gboolean changed;
	EContentEditorMode mode;
	ESpellChecker *spell_checker; /* this is not used internally */
	EThreeState start_bottom;
	EThreeState top_signature;
	gchar *signature_uid;
	gboolean selection_saved;
	GtkTextIter selection_start; /* valid only if selection_saved is TRUE */
	GtkTextIter selection_end; /* valid only if selection_saved is TRUE */
};

static void e_markdown_editor_content_editor_init (EContentEditorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMarkdownEditor, e_markdown_editor, GTK_TYPE_BOX,
	G_ADD_PRIVATE (EMarkdownEditor)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_EDITOR, e_markdown_editor_content_editor_init))

enum {
	PROP_0,
	PROP_IS_MALFUNCTION,
	PROP_CAN_COPY,
	PROP_CAN_CUT,
	PROP_CAN_PASTE,
	PROP_CAN_REDO,
	PROP_CAN_UNDO,
	PROP_CHANGED,
	PROP_EDITABLE,
	PROP_MODE,
	PROP_SPELL_CHECK_ENABLED,
	PROP_SPELL_CHECKER,
	PROP_START_BOTTOM,
	PROP_TOP_SIGNATURE,
	PROP_VISUALLY_WRAP_LONG_LINES,
	PROP_LAST_ERROR,

	PROP_ALIGNMENT,
	PROP_BACKGROUND_COLOR,
	PROP_BLOCK_FORMAT,
	PROP_BOLD,
	PROP_FONT_COLOR,
	PROP_FONT_NAME,
	PROP_FONT_SIZE,
	PROP_INDENT_LEVEL,
	PROP_ITALIC,
	PROP_STRIKETHROUGH,
	PROP_SUBSCRIPT,
	PROP_SUPERSCRIPT,
	PROP_UNDERLINE
};

enum {
	CHANGED,
	FORMAT_BOLD,
	FORMAT_ITALIC,
	FORMAT_QUOTE,
	FORMAT_CODE,
	FORMAT_BULLET_LIST,
	FORMAT_NUMBERED_LIST,
	FORMAT_HEADER,
	INSERT_LINK,
	INSERT_EMOJI,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef void (* AsyncCallback) (EMarkdownEditor *self,
				gpointer user_data);

typedef struct _AsyncData {
	EMarkdownEditor *self;
	AsyncCallback callback;
	gpointer user_data;
} AsyncData;

static AsyncData *
async_data_new (EMarkdownEditor *self,
		AsyncCallback callback,
		gpointer user_data)
{
	AsyncData *data;

	data = g_slice_new (AsyncData);
	data->self = g_object_ref (self);
	data->callback = callback;
	data->user_data = user_data;

	return data;
}

static void
async_data_free (gpointer ptr)
{
	AsyncData *data = ptr;

	if (data) {
		g_clear_object (&data->self);
		g_slice_free (AsyncData, data);
	}
}

static gboolean
e_markdown_editor_call_async_cb (gpointer user_data)
{
	AsyncData *data = user_data;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (data->callback != NULL, FALSE);

	data->callback (data->self, data->user_data);

	return FALSE;
}

static void
e_markdown_editor_call_async (EMarkdownEditor *self,
			      AsyncCallback callback,
			      gpointer user_data)
{
	g_timeout_add_full (G_PRIORITY_HIGH, 1,
		e_markdown_editor_call_async_cb,
		async_data_new (self, callback, user_data),
		async_data_free);
}

static gboolean
e_markdown_editor_supports_mode (EContentEditor *cnt_editor,
				 EContentEditorMode mode)
{
	return mode == E_CONTENT_EDITOR_MODE_MARKDOWN ||
	#ifdef HAVE_MARKDOWN
		mode == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML ||
	#endif
		mode == E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT;
}

static void
e_markdown_editor_grab_focus (EContentEditor *cnt_editor)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);

	gtk_widget_grab_focus (GTK_WIDGET (self->priv->text_view));
}

static gboolean
e_markdown_editor_is_focus (EContentEditor *cnt_editor)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);

	return gtk_widget_is_focus (GTK_WIDGET (self->priv->text_view));
}

typedef struct _InitAsyncData {
	EContentEditorInitializedCallback callback;
	gpointer user_data;
} InitAsyncData;

static void
e_markdown_editor_initialize_done (EMarkdownEditor *self,
				   gpointer user_data)
{
	InitAsyncData *data = user_data;

	g_return_if_fail (data != NULL);
	g_return_if_fail (data->callback != NULL);

	data->callback (E_CONTENT_EDITOR (self), data->user_data);

	g_slice_free (InitAsyncData, data);
}

static void
e_markdown_editor_initialize (EContentEditor *cnt_editor,
			      EContentEditorInitializedCallback callback,
			      gpointer user_data)
{
	InitAsyncData *data;

	data = g_slice_new (InitAsyncData);
	data->callback = callback;
	data->user_data = user_data;

	e_markdown_editor_call_async (E_MARKDOWN_EDITOR (cnt_editor), e_markdown_editor_initialize_done, data);
}

static gboolean
e_markdown_editor_is_ready (EContentEditor *cnt_editor)
{
	return TRUE;
}

static void
e_markdown_editor_update_styles (EContentEditor *cnt_editor)
{
}

static void
e_markdown_editor_insert_content (EContentEditor *cnt_editor,
				  const gchar *content,
				  EContentEditorInsertContentFlags flags)
{
	EMarkdownEditor *self;
	gchar *text = NULL;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (cnt_editor));
	g_return_if_fail (content != NULL);

	self = E_MARKDOWN_EDITOR (cnt_editor);

	if ((flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML) != 0) {
		text = e_markdown_utils_html_to_text (content, -1, E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS);
		content = text;
	}

	if ((flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) != 0) {
		e_markdown_editor_set_text (self, content);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT) != 0) {
		GtkTextBuffer *text_buffer;
		GString *quoted;
		gint ii;

		quoted = g_string_sized_new (strlen (content) + 4);
		g_string_append (quoted, "> ");
		g_string_append (quoted, content);

		for (ii = 0; ii < quoted->len; ii++) {
			if (quoted->str[ii] == '\n' && ii + 1 < quoted->len)
				g_string_insert (quoted, ii + 1, "> ");
		}

		text_buffer = gtk_text_view_get_buffer (self->priv->text_view);
		gtk_text_buffer_insert_at_cursor (text_buffer, quoted->str, -1);

		g_string_free (quoted, TRUE);
	} else {
		GtkTextBuffer *text_buffer;

		text_buffer = gtk_text_view_get_buffer (self->priv->text_view);
		gtk_text_buffer_insert_at_cursor (text_buffer, content, -1);
	}

	g_free (text);
}

static void
e_markdown_editor_get_content (EContentEditor *cnt_editor,
			       guint32 flags, /* bit-or of EContentEditorGetContentFlags */
			       const gchar *inline_images_from_domain,
			       GCancellable *cancellable,
			       GAsyncReadyCallback callback,
			       gpointer user_data)
{
	GTask *task;
	EContentEditorContentHash *content_hash;

	content_hash = e_content_editor_util_new_content_hash ();
	if ((flags & E_CONTENT_EDITOR_GET_RAW_BODY_HTML) != 0 ||
	    (flags & E_CONTENT_EDITOR_GET_TO_SEND_HTML) != 0) {
		gchar *html;

		html = e_markdown_editor_dup_html (E_MARKDOWN_EDITOR (cnt_editor));

		if (html) {
			if ((flags & E_CONTENT_EDITOR_GET_RAW_BODY_HTML) != 0 &&
			    (flags & E_CONTENT_EDITOR_GET_TO_SEND_HTML) != 0) {
				e_content_editor_util_put_content_data (content_hash,
					E_CONTENT_EDITOR_GET_RAW_BODY_HTML, html);
				e_content_editor_util_take_content_data (content_hash,
					E_CONTENT_EDITOR_GET_TO_SEND_HTML, html, g_free);
			} else if ((flags & E_CONTENT_EDITOR_GET_RAW_BODY_HTML) != 0) {
				e_content_editor_util_take_content_data (content_hash,
					E_CONTENT_EDITOR_GET_RAW_BODY_HTML, html, g_free);
			} else {
				e_content_editor_util_take_content_data (content_hash,
					E_CONTENT_EDITOR_GET_TO_SEND_HTML, html, g_free);
			}
		}
	}

	if ((flags & E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN) != 0 ||
	    (flags & E_CONTENT_EDITOR_GET_RAW_DRAFT) != 0 ||
	    (flags & E_CONTENT_EDITOR_GET_TO_SEND_PLAIN) != 0) {
		gchar *text;

		text = e_markdown_editor_dup_text (E_MARKDOWN_EDITOR (cnt_editor));

		if (text) {
			gint n_formats = ((flags & E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN) != 0 ? 1 : 0) +
					 ((flags & E_CONTENT_EDITOR_GET_RAW_DRAFT) != 0 ? 1 : 0) +
					 ((flags & E_CONTENT_EDITOR_GET_TO_SEND_PLAIN) != 0 ? 1 : 0);

			if (n_formats == 1) {
				EContentEditorGetContentFlags format;
				if ((flags & E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN) != 0)
					format = E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN;
				else if ((flags & E_CONTENT_EDITOR_GET_RAW_DRAFT) != 0)
					format = E_CONTENT_EDITOR_GET_RAW_DRAFT;
				else
					format = E_CONTENT_EDITOR_GET_TO_SEND_PLAIN;

				e_content_editor_util_take_content_data (content_hash,
					format, text, g_free);
			} else {
				if ((flags & E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN) != 0)
					e_content_editor_util_put_content_data (content_hash,
						E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN, text);

				if ((flags & E_CONTENT_EDITOR_GET_RAW_DRAFT) != 0)
					e_content_editor_util_put_content_data (content_hash,
						E_CONTENT_EDITOR_GET_RAW_DRAFT, text);

				if ((flags & E_CONTENT_EDITOR_GET_TO_SEND_PLAIN) != 0)
					e_content_editor_util_put_content_data (content_hash,
						E_CONTENT_EDITOR_GET_TO_SEND_PLAIN, text);

				g_free (text);
			}
		}
	}

	if ((flags & E_CONTENT_EDITOR_GET_RAW_BODY_STRIPPED) != 0) {
		gchar *text;

		text = e_markdown_editor_dup_text (E_MARKDOWN_EDITOR (cnt_editor));

		if (text) {
			gchar *separator;

			separator = strstr (text, "-- \n");

			if (separator)
				*separator = '\0';
		}

		if (text) {
			e_content_editor_util_take_content_data (content_hash,
					E_CONTENT_EDITOR_GET_RAW_BODY_STRIPPED, text, g_free);
		} else {
			e_content_editor_util_put_content_data (content_hash,
					E_CONTENT_EDITOR_GET_RAW_BODY_STRIPPED, "");
		}
	}

	task = g_task_new (cnt_editor, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_markdown_editor_get_content);
	g_task_return_pointer (task, content_hash, (GDestroyNotify) e_content_editor_util_free_content_hash);
	g_object_unref (task);
}

static EContentEditorContentHash *
e_markdown_editor_get_content_finish (EContentEditor *cnt_editor,
				      GAsyncResult *result,
				      GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, cnt_editor), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

static void
e_markdown_editor_move_caret_on_coordinates (EContentEditor *cnt_editor,
					     gint x,
					     gint y,
					     gboolean cancel_if_not_collapsed)
{
}

static void
e_markdown_editor_cut (EContentEditor *cnt_editor)
{
	/* Handled by GtktextView itself */
}

static void
e_markdown_editor_copy (EContentEditor *cnt_editor)
{
	/* Handled by GtktextView itself */
}

static void
e_markdown_editor_paste (EContentEditor *cnt_editor)
{
	/* Handled by GtktextView itself */
}

static void
e_markdown_editor_paste_primary (EContentEditor *cnt_editor)
{
	/* Handled by GtktextView itself */
}

static void
e_markdown_editor_undo (EContentEditor *cnt_editor)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);

	e_widget_undo_do_undo (GTK_WIDGET (self->priv->text_view));
}

static void
e_markdown_editor_redo (EContentEditor *cnt_editor)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);

	e_widget_undo_do_redo (GTK_WIDGET (self->priv->text_view));
}

static void
e_markdown_editor_clear_undo_redo_history (EContentEditor *cnt_editor)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);

	e_widget_undo_reset (GTK_WIDGET (self->priv->text_view));

	g_object_notify (G_OBJECT (self), "can-undo");
	g_object_notify (G_OBJECT (self), "can-redo");
}

static void
e_markdown_editor_set_spell_checking_languages (EContentEditor *cnt_editor,
						const gchar **languages)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);

	e_spell_text_view_set_languages (self->priv->text_view, languages);
}

static gchar *
e_markdown_editor_get_caret_word (EContentEditor *cnt_editor)
{
	return NULL;
}

static void
e_markdown_editor_replace_caret_word (EContentEditor *cnt_editor,
				      const gchar *replacement)
{
}

static void
e_markdown_editor_select_all (EContentEditor *cnt_editor)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);

	g_signal_emit_by_name (self->priv->text_view, "select-all", 0, TRUE, NULL);
}

static gunichar *
e_markdown_editor_prepare_search_text (const gchar *text,
				       guint32 *flags)
{
	gunichar *search_text;
	guint ii;

	if (!text || !*text)
		return NULL;

	/* Fine-tune the direction flags:
	   forward & next = forward
	   forward & previous = backward
	   backward & next = backward
	   backward & previous = forward
	*/
	if ((*flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS) == 0 &&
	    (*flags & E_CONTENT_EDITOR_FIND_PREVIOUS) != 0) {
		*flags = ((*flags) & ~(E_CONTENT_EDITOR_FIND_MODE_BACKWARDS | E_CONTENT_EDITOR_FIND_PREVIOUS | E_CONTENT_EDITOR_FIND_NEXT)) |
			E_CONTENT_EDITOR_FIND_MODE_BACKWARDS;
	} else if ((*flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS) != 0 &&
		   (*flags & E_CONTENT_EDITOR_FIND_PREVIOUS) != 0) {
		*flags = ((*flags) & ~(E_CONTENT_EDITOR_FIND_MODE_BACKWARDS | E_CONTENT_EDITOR_FIND_PREVIOUS | E_CONTENT_EDITOR_FIND_NEXT));
	}

	search_text = g_utf8_to_ucs4 (text, -1, NULL, NULL, NULL);

	if (search_text && (*flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS) != 0) {
		guint len;

		for (len = 0; search_text[len]; len++) {
			/* Just count them */
		}

		if (len) {
			len--;

			/* Swap the letters backwards */
			for (ii = 0; ii < len; ii++) {
				gunichar chr = search_text[ii];
				search_text[ii] = search_text[len];
				search_text[len] = chr;
				len--;
			}
		}
	}

	if (search_text && (*flags & E_CONTENT_EDITOR_FIND_CASE_INSENSITIVE) != 0) {
		for (ii = 0; search_text[ii]; ii++) {
			search_text[ii] = g_unichar_tolower (search_text[ii]);
		}
	}

	return search_text;
}

static gboolean
e_markdown_editor_do_search_text (GtkTextBuffer *buffer,
				  const gunichar *search_text,
				  guint32 flags,
				  gboolean *did_wrap_around,
				  const GtkTextIter *from_iter,
				  const GtkTextIter *limit, /* used only after wrap around */
				  GtkTextIter *out_occur_start,
				  GtkTextIter *out_occur_end)
{
	GtkTextIter iter, from_cursor;
	gboolean case_insensitive = (flags & E_CONTENT_EDITOR_FIND_CASE_INSENSITIVE) != 0;
	gboolean wrap_around = (!*did_wrap_around) && (flags & E_CONTENT_EDITOR_FIND_WRAP_AROUND) != 0;
	gboolean backwards = (flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS) != 0;
	gboolean found = FALSE;

	if (from_iter) {
		iter = *from_iter;
	} else {
		gtk_text_buffer_get_selection_bounds (buffer, &from_cursor, &iter);

		if (!backwards)
			from_cursor = iter;

		if (!limit)
			limit = &from_cursor;

		iter = from_cursor;
	}

	if (backwards && !gtk_text_iter_backward_char (&iter)) {
		if (wrap_around) {
			wrap_around = FALSE;
			gtk_text_buffer_get_end_iter (buffer, &iter);
			if (!gtk_text_iter_backward_char (&iter))
				return FALSE;
		} else {
			return FALSE;
		}
	}

	while (!found) {
		gunichar chr;

		chr = gtk_text_iter_get_char (&iter);

		if (chr) {
			if ((case_insensitive && g_unichar_tolower (chr) == search_text[0]) ||
			    (!case_insensitive && chr == search_text[0])) {
				GtkTextIter next = iter;
				guint ii;

				for (ii = 1; !found; ii++) {
					if (!search_text[ii]) {
						/* To have selected also the last character */
						if (backwards)
							gtk_text_iter_forward_char (&iter);
						else
							gtk_text_iter_forward_char (&next);

						found = TRUE;
						if (backwards) {
							*out_occur_start = next;
							*out_occur_end = iter;
						} else {
							*out_occur_start = iter;
							*out_occur_end = next;
						}
						break;
					}

					if ((backwards && !gtk_text_iter_backward_char (&next)) ||
					    (!backwards && !gtk_text_iter_forward_char (&next)))
						break;

					if (*did_wrap_around && !gtk_text_iter_compare (&iter, limit))
						break;

					chr = gtk_text_iter_get_char (&next);

					if (!chr)
						break;

					if ((case_insensitive && g_unichar_tolower (chr) == search_text[ii]) ||
					    (!case_insensitive && chr == search_text[ii])) {
						/* matched the next letter */
					} else {
						break;
					}
				}

				if (found)
					break;
			}

			if (!found && *did_wrap_around && !gtk_text_iter_compare (&iter, limit))
				break;
		}

		if ((backwards && !gtk_text_iter_backward_char (&iter)) ||
		    (!backwards && !gtk_text_iter_forward_char (&iter))) {
			if (!wrap_around)
				break;

			*did_wrap_around = TRUE;
			wrap_around = FALSE;

			if (backwards)
				gtk_text_buffer_get_end_iter (buffer, &iter);
			else
				gtk_text_buffer_get_start_iter (buffer, &iter);

			if (!gtk_text_iter_compare (&iter, limit))
				break;
		}
	}

	return found;
}

static void
e_markdown_editor_find (EContentEditor *cnt_editor,
			guint32 flags,
			const gchar *text)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);
	GtkTextBuffer *buffer;
	GtkTextIter occur_start, occur_end;
	gboolean did_wrap_around = FALSE;
	gunichar *search_text;

	search_text = e_markdown_editor_prepare_search_text (text, &flags);

	if (!search_text) {
		e_content_editor_emit_find_done (cnt_editor, 0);
		return;
	}

	buffer = gtk_text_view_get_buffer (self->priv->text_view);

	if (e_markdown_editor_do_search_text (buffer, search_text, flags, &did_wrap_around, NULL, NULL, &occur_start, &occur_end)) {
		gtk_text_buffer_select_range (buffer, &occur_start, &occur_end);
		e_content_editor_emit_find_done (cnt_editor, 1);
	} else {
		e_content_editor_emit_find_done (cnt_editor, 0);
	}

	g_free (search_text);
}

static void
e_markdown_editor_replace (EContentEditor *cnt_editor,
			   const gchar *replacement)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (self->priv->text_view);
	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	gtk_text_buffer_delete (buffer, &start, &end);
	gtk_text_buffer_insert_at_cursor (buffer, replacement, -1);
}

static void
e_markdown_editor_replace_all (EContentEditor *cnt_editor,
			       guint32 flags,
			       const gchar *find_text,
			       const gchar *replace_with)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);
	GtkTextBuffer *buffer;
	GtkTextIter occur_start, occur_end, limit, last_replace;
	gboolean did_wrap_around = FALSE;
	gunichar *search_text;
	guint count = 0, replace_len = 0;

	search_text = e_markdown_editor_prepare_search_text (find_text, &flags);

	if (!search_text) {
		e_content_editor_emit_replace_all_done (cnt_editor, 0);
		return;
	}

	buffer = gtk_text_view_get_buffer (self->priv->text_view);
	gtk_text_buffer_get_selection_bounds (buffer, &occur_start, &occur_end);

	/* Different bound than in search, to replace also current match */
	if ((flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS) != 0)
		limit = occur_end;
	else
		limit = occur_start;

	if (replace_with)
		replace_len = g_utf8_strlen (replace_with, -1);

	last_replace = limit;

	while (e_markdown_editor_do_search_text (buffer, search_text, flags, &did_wrap_around, &last_replace, &limit, &occur_start, &occur_end)) {
		GtkTextMark *mark;
		gboolean last_match;

		last_match = did_wrap_around && !gtk_text_iter_compare (&occur_start, &limit);

		if (last_match && !(flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS))
			break;

		/* Remember where the limit was... */
		mark = gtk_text_buffer_create_mark (buffer, NULL, &limit, !(flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS));

		gtk_text_buffer_delete (buffer, &occur_start, &occur_end);

		last_replace = occur_start;

		if (replace_with && *replace_with) {
			gtk_text_buffer_insert (buffer, &occur_start, replace_with, -1);

			/* Get on the first letter of the replaced word */
			if ((flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS) != 0 &&
			    !gtk_text_iter_backward_chars (&occur_start, replace_len)) {
				break;
			}

			last_replace = occur_start;
		}

		/* ... then restore the limit, after the buffer changed (which invalidated the iterator) */
		gtk_text_buffer_get_iter_at_mark (buffer, &limit, mark);
		gtk_text_buffer_delete_mark (buffer, mark);

		count++;

		if (last_match)
			break;
	}

	g_free (search_text);

	if (count)
		gtk_text_buffer_select_range (buffer, &last_replace, &last_replace);

	e_content_editor_emit_replace_all_done (cnt_editor, count);
}

static void
e_markdown_editor_selection_save (EContentEditor *cnt_editor)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (self->priv->text_view);
	gtk_text_buffer_get_selection_bounds (buffer, &self->priv->selection_start, &self->priv->selection_end);

	self->priv->selection_saved = TRUE;
}

static void
e_markdown_editor_selection_restore (EContentEditor *cnt_editor)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);

	if (self->priv->selection_saved) {
		GtkTextBuffer *buffer;

		self->priv->selection_saved = FALSE;

		buffer = gtk_text_view_get_buffer (self->priv->text_view);
		gtk_text_buffer_select_range (buffer, &self->priv->selection_start, &self->priv->selection_end);
	}
}

static gchar *
e_markdown_editor_get_current_signature_uid (EContentEditor *cnt_editor)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);

	return self->priv->signature_uid;
}

static gchar *
e_markdown_editor_insert_signature (EContentEditor *cnt_editor,
				    const gchar *content,
				    EContentEditorMode editor_mode,
				    gboolean can_reposition_caret,
				    const gchar *signature_id,
				    gboolean *set_signature_from_message,
				    gboolean *check_if_signature_is_changed,
				    gboolean *ignore_next_signature_change)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);
	GtkTextMark *sig_start_mark, *sig_end_mark;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *plain_text = NULL;

	g_clear_pointer (&self->priv->signature_uid, g_free);
	self->priv->signature_uid = g_strdup (signature_id);

	if (content && *content && editor_mode == E_CONTENT_EDITOR_MODE_HTML) {
		plain_text = e_markdown_utils_html_to_text (content, -1, E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT);
		content = plain_text;
		editor_mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;
	}

	if (content && *content && editor_mode == E_CONTENT_EDITOR_MODE_PLAIN_TEXT) {
		gchar *tmp;

		tmp = g_strconcat ("```\n", content,
			g_str_has_suffix (content, "\n") ? "" : "\n",
			"```\n", NULL);

		g_free (plain_text);
		plain_text = tmp;
		content = plain_text;
	}

	if (!e_content_editor_util_three_state_to_bool (E_THREE_STATE_INCONSISTENT, "composer-no-signature-delim") &&
	    content && *content && !g_str_has_prefix (content, "-- \n") && !strstr (content, "\n-- \n")) {
		gchar *tmp;

		tmp = g_strconcat ("-- \n",
			/* Add an empty line between the delimiter and the markdown signature */
			"\n",
			content, NULL);

		g_free (plain_text);
		plain_text = tmp;
		content = plain_text;
	}

	buffer = gtk_text_view_get_buffer (self->priv->text_view);
	gtk_text_buffer_get_bounds (buffer, &start, &end);

	sig_start_mark = gtk_text_buffer_get_mark (buffer, EVO_SIGNATURE_START_MARK);
	sig_end_mark = gtk_text_buffer_get_mark (buffer, EVO_SIGNATURE_END_MARK);

	if (content && *content) {
		gtk_text_buffer_begin_user_action (buffer);

		if (sig_start_mark && sig_end_mark) {
			GtkTextIter sig_start, sig_end;

			gtk_text_buffer_get_iter_at_mark (buffer, &sig_start, sig_start_mark);
			gtk_text_buffer_get_iter_at_mark (buffer, &sig_end, sig_end_mark);

			gtk_text_buffer_delete (buffer, &sig_start, &sig_end);

			gtk_text_buffer_insert (buffer, &sig_start, content, -1);

			gtk_text_buffer_get_bounds (buffer, &start, &end);
		} else if (e_content_editor_util_three_state_to_bool (e_content_editor_get_top_signature (cnt_editor), "composer-top-signature")) {
			if (!g_str_has_suffix (content, "\n\n")) {
				if (g_str_has_suffix (content, "\n"))
					gtk_text_buffer_insert (buffer, &start, "\n", 1);
				else
					gtk_text_buffer_insert (buffer, &start, "\n\n", 2);

				gtk_text_buffer_get_start_iter (buffer, &start);
			}

			if (sig_start_mark)
				gtk_text_buffer_delete_mark_by_name (buffer, EVO_SIGNATURE_START_MARK);
			if (sig_end_mark)
				gtk_text_buffer_delete_mark_by_name (buffer, EVO_SIGNATURE_END_MARK);

			gtk_text_buffer_create_mark (buffer, EVO_SIGNATURE_END_MARK, &start, FALSE);

			gtk_text_buffer_insert (buffer, &start, content, -1);
			gtk_text_buffer_get_start_iter (buffer, &start);

			gtk_text_buffer_create_mark (buffer, EVO_SIGNATURE_START_MARK, &start, TRUE);

			gtk_text_buffer_insert (buffer, &start, "\n", 1);
			gtk_text_buffer_get_start_iter (buffer, &start);
		} else {
			GtkTextIter iter = end;

			if (gtk_text_iter_backward_char (&iter) &&
			    gtk_text_iter_get_char (&iter) != '\n') {
				gtk_text_buffer_insert (buffer, &end, "\n", 1);
				gtk_text_buffer_get_end_iter (buffer, &end);
			}

			if (sig_start_mark)
				gtk_text_buffer_delete_mark_by_name (buffer, EVO_SIGNATURE_START_MARK);
			if (sig_end_mark)
				gtk_text_buffer_delete_mark_by_name (buffer, EVO_SIGNATURE_END_MARK);

			gtk_text_buffer_create_mark (buffer, EVO_SIGNATURE_START_MARK, &end, TRUE);

			gtk_text_buffer_insert (buffer, &end, content, -1);
			gtk_text_buffer_get_end_iter (buffer, &end);

			gtk_text_buffer_create_mark (buffer, EVO_SIGNATURE_END_MARK, &end, FALSE);
		}

		gtk_text_buffer_end_user_action (buffer);
	} else if (sig_start_mark && sig_end_mark) {
		GtkTextIter sig_start, sig_end;

		gtk_text_buffer_begin_user_action (buffer);
		gtk_text_buffer_get_iter_at_mark (buffer, &sig_start, sig_start_mark);
		gtk_text_buffer_get_iter_at_mark (buffer, &sig_end, sig_end_mark);
		gtk_text_buffer_delete (buffer, &sig_start, &sig_end);
		gtk_text_buffer_get_bounds (buffer, &start, &end);
		gtk_text_buffer_end_user_action (buffer);
	} else {
		if (sig_start_mark)
			gtk_text_buffer_delete_mark_by_name (buffer, EVO_SIGNATURE_START_MARK);
		if (sig_end_mark)
			gtk_text_buffer_delete_mark_by_name (buffer, EVO_SIGNATURE_END_MARK);
	}

	if (can_reposition_caret) {
		if (e_content_editor_util_three_state_to_bool (e_content_editor_get_start_bottom (cnt_editor), "composer-reply-start-bottom")) {
			gtk_text_buffer_select_range (buffer, &end, &end);
		} else {
			gtk_text_buffer_get_start_iter (buffer, &start);
			gtk_text_buffer_select_range (buffer, &start, &start);
		}
	}

	g_free (plain_text);

	return g_strdup (self->priv->signature_uid);
}

static void
e_markdown_editor_on_dialog_open (EContentEditor *cnt_editor,
				  const gchar *name)
{
	if (g_strcmp0 (name, E_CONTENT_EDITOR_DIALOG_REPLACE) == 0) {
		EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (self->priv->text_view);
		gtk_text_buffer_begin_user_action (buffer);
	}
}

static void
e_markdown_editor_on_dialog_close (EContentEditor *cnt_editor,
				   const gchar *name)
{
	if (g_strcmp0 (name, E_CONTENT_EDITOR_DIALOG_REPLACE) == 0) {
		EMarkdownEditor *self = E_MARKDOWN_EDITOR (cnt_editor);
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (self->priv->text_view);
		gtk_text_buffer_end_user_action (buffer);
	}
}

static gboolean
e_markdown_editor_can_copy (EMarkdownEditor *self)
{
	return self->priv->can_copy;
}

static gboolean
e_markdown_editor_can_cut (EMarkdownEditor *self)
{
	return self->priv->can_cut;
}

static gboolean
e_markdown_editor_can_paste (EMarkdownEditor *self)
{
	return self->priv->can_paste;
}

static gboolean
e_markdown_editor_can_redo (EMarkdownEditor *self)
{
	return e_widget_undo_has_redo (GTK_WIDGET (self->priv->text_view));
}

static gboolean
e_markdown_editor_can_undo (EMarkdownEditor *self)
{
	return e_widget_undo_has_undo (GTK_WIDGET (self->priv->text_view));
}

static gboolean
e_markdown_editor_get_changed (EMarkdownEditor *self)
{
	return self->priv->changed;
}

static void
e_markdown_editor_set_changed (EMarkdownEditor *self,
			       gboolean value)
{
	if ((self->priv->changed ? 1 : 0) != (value ? 1 : 0)) {
		self->priv->changed = value;
		g_object_notify (G_OBJECT (self), "changed");
	}
}

static gboolean
e_markdown_editor_is_editable (EMarkdownEditor *self)
{
	return gtk_text_view_get_editable (self->priv->text_view);
}

static void
e_markdown_editor_set_editable (EMarkdownEditor *self,
				gboolean value)
{
	if ((gtk_text_view_get_editable (self->priv->text_view) ? 1 : 0) != (value ? 1 : 0)) {
		gtk_text_view_set_editable (self->priv->text_view, value);
		g_object_notify (G_OBJECT (self), "editable");
	}
}

static EContentEditorMode
e_markdown_editor_get_mode (EMarkdownEditor *self)
{
	return self->priv->mode;
}

static void
e_markdown_editor_set_mode (EMarkdownEditor *self,
			    EContentEditorMode mode)
{
	g_return_if_fail (mode == E_CONTENT_EDITOR_MODE_MARKDOWN ||
		mode == E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT ||
		mode == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML);

	if (self->priv->mode != mode) {
		self->priv->mode = mode;
		g_object_notify (G_OBJECT (self), "mode");
	}
}

static gboolean
e_markdown_editor_get_spell_check_enabled (EMarkdownEditor *self)
{
	return e_spell_text_view_get_enabled (self->priv->text_view);
}

static void
e_markdown_editor_set_spell_check_enabled (EMarkdownEditor *self,
					   gboolean value)
{
	gboolean spell_check_enabled = e_markdown_editor_get_spell_check_enabled (self);
	if ((spell_check_enabled ? 1 : 0) != (value ? 1 : 0)) {
		e_spell_text_view_set_enabled (self->priv->text_view, value);
		g_object_notify (G_OBJECT (self), "spell-check-enabled");
	}
}

static ESpellChecker *
e_markdown_editor_get_spell_checker (EMarkdownEditor *self)
{
	return self->priv->spell_checker;
}

static EThreeState
e_markdown_editor_get_start_bottom (EMarkdownEditor *self)
{
	return self->priv->start_bottom;
}

static void
e_markdown_editor_set_start_bottom (EMarkdownEditor *self,
				    EThreeState value)
{
	if (self->priv->start_bottom != value) {
		self->priv->start_bottom = value;
		g_object_notify (G_OBJECT (self), "start-bottom");
	}
}

static EThreeState
e_markdown_editor_get_top_signature (EMarkdownEditor *self)
{
	return self->priv->top_signature;
}

static void
e_markdown_editor_set_top_signature (EMarkdownEditor *self,
				     EThreeState value)
{
	if (self->priv->top_signature != value) {
		self->priv->top_signature = value;
		g_object_notify (G_OBJECT (self), "top-signature");
	}
}

static void
e_markdown_editor_get_selection (EMarkdownEditor *self,
				 GtkTextIter *out_start,
				 GtkTextIter *out_end,
				 gchar **out_selected_text)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer (self->priv->text_view);

	if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end) && out_selected_text) {
		*out_selected_text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
	} else if (out_selected_text) {
		*out_selected_text = NULL;
	}

	if (out_start)
		*out_start = start;

	if (out_end)
		*out_end = end;
}

static void
e_markdown_editor_surround_selection (EMarkdownEditor *self,
				      gboolean whole_lines,
				      const gchar *prefix,
				      const gchar *suffix)
{
	GtkTextIter start, end;
	GtkTextBuffer *buffer;

	e_markdown_editor_get_selection (self, &start, &end, NULL);

	buffer = gtk_text_view_get_buffer (self->priv->text_view);

	gtk_text_buffer_begin_user_action (buffer);

	if (whole_lines) {
		gint to_line, ii;

		to_line = gtk_text_iter_get_line (&end);

		for (ii = gtk_text_iter_get_line (&start); ii <= to_line; ii++) {
			GtkTextIter iter;

			gtk_text_buffer_get_iter_at_line (buffer, &iter, ii);

			if (prefix && *prefix)
				gtk_text_buffer_insert (buffer, &iter, prefix, -1);

			if (suffix && *suffix) {
				gtk_text_iter_forward_to_line_end (&iter);
				gtk_text_buffer_insert (buffer, &iter, suffix, -1);
			}
		}
	} else {
		gint end_offset = gtk_text_iter_get_offset (&end);

		if (prefix && *prefix) {
			gtk_text_buffer_insert (buffer, &start, prefix, -1);
			/* Keep the cursor where it is, move it only when the suffix is used */
			end_offset += strlen (prefix);
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset);
		}

		if (suffix && *suffix) {
			gtk_text_buffer_insert (buffer, &end, suffix, -1);
			/* Place the cursor before the suffix */
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset);
			gtk_text_buffer_select_range (buffer, &end, &end);
		}
	}

	gtk_text_buffer_end_user_action (buffer);
}

static void
e_markdown_editor_format_bold_text_cb (EMarkdownEditor *self)
{
	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, FALSE, "**", "**");
}

static void
e_markdown_editor_format_italic_text_cb (EMarkdownEditor *self)
{
	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, FALSE, "*", "*");
}

static void
e_markdown_editor_format_quote_cb (EMarkdownEditor *self)
{
	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, TRUE, "> ", NULL);
}

static void
e_markdown_editor_format_code_cb (EMarkdownEditor *self)
{
	GtkTextIter start, end;
	gchar *selection = NULL;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_get_selection (self, &start, &end, &selection);

	if (selection && strchr (selection, '\n')) {
		GtkTextBuffer *buffer;
		GtkTextIter iter;
		gint start_line, end_line;

		buffer = gtk_text_view_get_buffer (self->priv->text_view);

		gtk_text_buffer_begin_user_action (buffer);

		start_line = gtk_text_iter_get_line (&start);
		end_line = gtk_text_iter_get_line (&end);

		gtk_text_buffer_get_iter_at_line (buffer, &iter, start_line);
		gtk_text_buffer_insert (buffer, &iter, "```\n", -1);

		/* One line added above + 1 for the end line itself */
		end_line = end_line + 2;
		gtk_text_buffer_get_iter_at_line (buffer, &iter, end_line);
		if (gtk_text_iter_is_end (&iter) && gtk_text_iter_get_line_offset (&iter) > 0) {
			gtk_text_buffer_insert (buffer, &iter, "\n```\n", -1);
		} else {
			if (gtk_text_iter_is_end (&iter))
				end_line--;
			gtk_text_buffer_insert (buffer, &iter, "```\n", -1);
		}

		/* Place the cursor before the suffix */
		gtk_text_buffer_get_iter_at_line (buffer, &iter, end_line);
		gtk_text_buffer_select_range (buffer, &iter, &iter);

		gtk_text_buffer_end_user_action (buffer);
	} else {
		e_markdown_editor_surround_selection (self, FALSE, "`", "`");
	}

	g_free (selection);
}

static void
e_markdown_editor_insert_link_cb (EMarkdownEditor *self)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *selection = NULL;
	gint offset;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_get_selection (self, &start, &end, &selection);

	buffer = gtk_text_view_get_buffer (self->priv->text_view);
	offset = gtk_text_iter_get_offset (&start);

	gtk_text_buffer_begin_user_action (buffer);

	if (selection && *selection) {
		gint end_offset = gtk_text_iter_get_offset (&end);

		if (g_ascii_strncasecmp (selection, "http:", 5) == 0 ||
		    g_ascii_strncasecmp (selection, "https:", 6) == 0) {
			gtk_text_buffer_insert (buffer, &start, "[](", -1);
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset + 3);
			gtk_text_buffer_insert (buffer, &end, ")", -1);
			gtk_text_buffer_get_iter_at_offset (buffer, &start, offset + 1);
			end = start;
		} else {
			gtk_text_buffer_insert (buffer, &start, "[", -1);
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset + 1);
			gtk_text_buffer_insert (buffer, &end, "](https://)", -1);
			gtk_text_buffer_get_iter_at_offset (buffer, &start, end_offset + 1 + 2);
			gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset + 1 + 10);
		}

		gtk_text_buffer_select_range (buffer, &start, &end);
	} else {
		gtk_text_buffer_insert (buffer, &start, "[](https://)", -1);

		/* skip "[](" */
		offset += 3;

		gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);

		/* after the "https://" text */
		gtk_text_buffer_get_iter_at_offset (buffer, &end, offset + 8);

		gtk_text_buffer_select_range (buffer, &start, &end);
	}

	gtk_text_buffer_end_user_action (buffer);
}

static void
e_markdown_editor_format_bullet_list_cb (EMarkdownEditor *self)
{
	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, TRUE, "- ", NULL);
}

static void
e_markdown_editor_format_numbered_list_cb (EMarkdownEditor *self)
{
	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, TRUE, "1. ", NULL);
}

static void
e_markdown_editor_format_header_cb (EMarkdownEditor *self)
{
	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_surround_selection (self, TRUE, "# ", NULL);
}

static void
e_markdown_editor_insert_emoji_cb (EMarkdownEditor *self)
{
	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	g_signal_emit_by_name (self->priv->text_view, "insert-emoji", 0, NULL);
}

static void
e_markdown_editor_markdown_syntax_cb (EMarkdownEditor *self)
{
	GtkWidget *toplevel;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

	e_show_uri (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL, "https://commonmark.org/help/");
}

#ifdef HAVE_MARKDOWN
static void
e_markdown_editor_switch_page_cb (GtkNotebook *notebook,
				  GtkWidget *page,
				  guint page_num,
				  gpointer user_data)
{
	EMarkdownEditor *self = user_data;
	gchar *converted;
	gchar *html;
	gint n_items, ii;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	n_items = gtk_toolbar_get_n_items (self->priv->action_toolbar);

	for (ii = 0; ii < n_items; ii++) {
		GtkToolItem *item = gtk_toolbar_get_nth_item (self->priv->action_toolbar, ii);

		if (item) {
			GtkWidget *widget = GTK_WIDGET (item);

			/* Keep only the help button and hide any other */
			if (g_strcmp0 (gtk_widget_get_name (widget), "markdown-help") != 0)
				gtk_widget_set_visible (widget, page_num != 1);
		}
	}

	/* Not the Preview page */
	if (page_num != 1)
		return;

	converted = e_markdown_editor_dup_html (self);

	html = g_strconcat ("<div class=\"-e-web-view-background-color -e-web-view-text-color\" style=\"border: none; padding: 0px; margin: 0;\">",
		converted ? converted : "",
		"</div>",
		NULL);

	e_web_view_load_string (self->priv->web_view, html);

	g_free (converted);
	g_free (html);
}
#endif /* HAVE_MARKDOWN */

static gboolean
e_markdown_editor_is_dark_theme (EMarkdownEditor *self)
{
	GtkStyleContext *style_context;
	GdkRGBA rgba;
	gdouble brightness;

	g_return_val_if_fail (self->priv->action_toolbar != NULL, FALSE);

	style_context = gtk_widget_get_style_context (GTK_WIDGET (self->priv->action_toolbar));
	gtk_style_context_get_color (style_context, gtk_style_context_get_state (style_context), &rgba);

	brightness =
		(0.2109 * 255.0 * rgba.red) +
		(0.5870 * 255.0 * rgba.green) +
		(0.1021 * 255.0 * rgba.blue);

	return brightness > 140;
}

struct _toolbar_items {
	const gchar *label;
	const gchar *icon_name;
	const gchar *icon_name_dark;
	GCallback callback;
};

static struct _toolbar_items toolbar_items[] = {
	#define ITEM(lbl, icn, cbk) { lbl, icn, icn "-dark", G_CALLBACK (cbk) }
	ITEM (N_("Add bold text"), "markdown-bold", e_markdown_editor_format_bold_text_cb),
	ITEM (N_("Add italic text"), "markdown-italic", e_markdown_editor_format_italic_text_cb),
	ITEM (N_("Insert a quote"), "markdown-quote", e_markdown_editor_format_quote_cb),
	ITEM (N_("Insert code"), "markdown-code", e_markdown_editor_format_code_cb),
	ITEM (N_("Add a link"), "markdown-link", e_markdown_editor_insert_link_cb),
	ITEM (N_("Add a bullet list"), "markdown-bullets", e_markdown_editor_format_bullet_list_cb),
	ITEM (N_("Add a numbered list"), "markdown-numbers", e_markdown_editor_format_numbered_list_cb),
	ITEM (N_("Add a header"), "markdown-header", e_markdown_editor_format_header_cb),
	ITEM (N_("Insert Emoji"), "markdown-emoji", e_markdown_editor_insert_emoji_cb),
	ITEM (NULL, "", NULL),
	ITEM (N_("Open online common mark documentation"), "markdown-help", G_CALLBACK (e_markdown_editor_markdown_syntax_cb))
	#undef ITEM
};

static void
e_markdown_editor_style_updated_cb (GtkWidget *widget,
				    gpointer user_data)
{
	EMarkdownEditor *self;
	gboolean is_dark_theme;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (widget));

	self = E_MARKDOWN_EDITOR (widget);
	is_dark_theme = e_markdown_editor_is_dark_theme (self);

	if (self->priv->is_dark_theme != is_dark_theme) {
		gint n_items, ii, jj, idx = 0;

		self->priv->is_dark_theme = is_dark_theme;

		n_items = gtk_toolbar_get_n_items (self->priv->action_toolbar);

		for (ii = 0; ii < n_items; ii++) {
			GtkToolItem *item = gtk_toolbar_get_nth_item (self->priv->action_toolbar, ii);
			const gchar *name;

			if (!item || !GTK_IS_TOOL_BUTTON (item))
				continue;

			name = gtk_widget_get_name (GTK_WIDGET (item));

			if (!name || !*name)
				continue;

			for (jj = 0; jj < G_N_ELEMENTS (toolbar_items); jj++) {
				gint index = (jj + idx) % G_N_ELEMENTS (toolbar_items);

				if (g_strcmp0 (name, toolbar_items[index].icon_name) == 0) {
					const gchar *icon_name = is_dark_theme ? toolbar_items[index].icon_name_dark : toolbar_items[index].icon_name;

					if (icon_name) {
						GtkWidget *icon_widget = gtk_tool_button_get_icon_widget (GTK_TOOL_BUTTON (item));

						if (icon_widget)
							gtk_image_set_from_icon_name (GTK_IMAGE (icon_widget), icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
						else
							gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), icon_name);
					}

					idx = jj + 1;
					break;
				}
			}
		}
	}
}

static void
e_markdown_editor_notify_editable_cb (GObject *object,
				      GParamSpec *param,
				      gpointer user_data)
{
	EMarkdownEditor *self = user_data;
	gboolean sensitive = FALSE;
	gint n_items, ii;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	g_object_get (object, "editable", &sensitive, NULL);

	n_items = gtk_toolbar_get_n_items (self->priv->action_toolbar);

	for (ii = 0; ii < n_items; ii++) {
		GtkToolItem *item = gtk_toolbar_get_nth_item (self->priv->action_toolbar, ii);

		if (item) {
			GtkWidget *widget = GTK_WIDGET (item);

			/* Keep only the help button and hide any other */
			if (g_strcmp0 (gtk_widget_get_name (widget), "markdown-help") != 0)
				gtk_widget_set_sensitive (widget, sensitive);
		}
	}
}

static void
e_markdown_editor_text_view_changed_cb (GtkTextView *text_view,
					gpointer user_data)
{
	EMarkdownEditor *self = user_data;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	e_markdown_editor_set_changed (self, TRUE);

	g_signal_emit (self, signals[CHANGED], 0, NULL);
	e_content_editor_emit_content_changed (E_CONTENT_EDITOR (self));
}

static void
e_markdown_editor_set_property (GObject *object,
				guint property_id,
				const GValue *value,
				GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			e_markdown_editor_set_changed (
				E_MARKDOWN_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_EDITABLE:
			e_markdown_editor_set_editable (
				E_MARKDOWN_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_MODE:
			e_markdown_editor_set_mode (
				E_MARKDOWN_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_SPELL_CHECK_ENABLED:
			e_markdown_editor_set_spell_check_enabled (
				E_MARKDOWN_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_START_BOTTOM:
			e_markdown_editor_set_start_bottom (
				E_MARKDOWN_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_TOP_SIGNATURE:
			e_markdown_editor_set_top_signature (
				E_MARKDOWN_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_VISUALLY_WRAP_LONG_LINES:
		case PROP_LAST_ERROR:
		case PROP_ALIGNMENT:
		case PROP_BACKGROUND_COLOR:
		case PROP_BLOCK_FORMAT:
		case PROP_BOLD:
		case PROP_FONT_COLOR:
		case PROP_FONT_NAME:
		case PROP_FONT_SIZE:
		case PROP_INDENT_LEVEL:
		case PROP_ITALIC:
		case PROP_STRIKETHROUGH:
		case PROP_SUBSCRIPT:
		case PROP_SUPERSCRIPT:
		case PROP_UNDERLINE:
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_markdown_editor_get_property (GObject *object,
				guint property_id,
				GValue *value,
				GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IS_MALFUNCTION:
			g_value_set_boolean (value, FALSE);
			return;

		case PROP_CAN_COPY:
			g_value_set_boolean (
				value, e_markdown_editor_can_copy (
				E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_CAN_CUT:
			g_value_set_boolean (
				value, e_markdown_editor_can_cut (
				E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_CAN_PASTE:
			g_value_set_boolean (
				value, e_markdown_editor_can_paste (
				E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, e_markdown_editor_can_redo (
				E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, e_markdown_editor_can_undo (
				E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_CHANGED:
			g_value_set_boolean (
				value, e_markdown_editor_get_changed (
				E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value, e_markdown_editor_is_editable (
				E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_MODE:
			g_value_set_enum (
				value, e_markdown_editor_get_mode (
				E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_SPELL_CHECK_ENABLED:
			g_value_set_boolean (
				value,
				e_markdown_editor_get_spell_check_enabled (
					E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_SPELL_CHECKER:
			g_value_set_object (
				value,
				e_markdown_editor_get_spell_checker (
					E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_START_BOTTOM:
			g_value_set_enum (
				value,
				e_markdown_editor_get_start_bottom (
					E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_TOP_SIGNATURE:
			g_value_set_enum (
				value,
				e_markdown_editor_get_top_signature (
					E_MARKDOWN_EDITOR (object)));
			return;

		case PROP_VISUALLY_WRAP_LONG_LINES:
			g_value_set_boolean (value, FALSE);
			return;

		case PROP_LAST_ERROR:
			g_value_set_boxed (value, NULL);
			return;

		case PROP_ALIGNMENT:
			g_value_set_enum (value, E_CONTENT_EDITOR_ALIGNMENT_LEFT);
			return;

		case PROP_BACKGROUND_COLOR:
			g_value_set_boxed (value, NULL);
			return;

		case PROP_BLOCK_FORMAT:
			g_value_set_enum (value, E_CONTENT_EDITOR_BLOCK_FORMAT_PRE);
			return;

		case PROP_FONT_COLOR:
			g_value_set_boxed (value, NULL);
			return;

		case PROP_FONT_NAME:
			g_value_set_string (value, NULL);
			return;

		case PROP_FONT_SIZE:
			g_value_set_int (value, E_CONTENT_EDITOR_FONT_SIZE_NORMAL);
			return;

		case PROP_INDENT_LEVEL:
			g_value_set_int (value, 0);
			return;

		case PROP_BOLD:
		case PROP_ITALIC:
		case PROP_STRIKETHROUGH:
		case PROP_SUBSCRIPT:
		case PROP_SUPERSCRIPT:
		case PROP_UNDERLINE:
			g_value_set_boolean (value, FALSE);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_markdown_editor_constructed (GObject *object)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (object);
	GtkWidget *widget;
	GtkScrolledWindow *scrolled_window;
	guint ii;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_markdown_editor_parent_class)->constructed (object);

	widget = gtk_notebook_new ();
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"show-border", TRUE,
		"show-tabs", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (self), widget, TRUE, TRUE, 0);

	self->priv->notebook = GTK_NOTEBOOK (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_notebook_append_page (self->priv->notebook, widget, gtk_label_new_with_mnemonic (_("_Write")));

	scrolled_window = GTK_SCROLLED_WINDOW (widget);

	widget = gtk_text_view_new ();
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"margin", 4,
		"monospace", TRUE,
		"wrap-mode", GTK_WRAP_WORD_CHAR,
		NULL);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	self->priv->text_view = GTK_TEXT_VIEW (widget);

	e_buffer_tagger_connect (self->priv->text_view);
	e_spell_text_view_attach (self->priv->text_view);

	#ifdef HAVE_MARKDOWN
	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_notebook_append_page (self->priv->notebook, widget, gtk_label_new_with_mnemonic (_("_Preview")));

	scrolled_window = GTK_SCROLLED_WINDOW (widget);

	widget = e_web_view_new ();
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"margin", 4,
		NULL);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	self->priv->web_view = E_WEB_VIEW (widget);
	#endif /* HAVE_MARKDOWN */

	widget = gtk_toolbar_new ();
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (widget), GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_widget_show (widget);
	gtk_notebook_set_action_widget (self->priv->notebook, widget, GTK_PACK_END);

	self->priv->action_toolbar = GTK_TOOLBAR (widget);
	self->priv->is_dark_theme = e_markdown_editor_is_dark_theme (self);

	for (ii = 0; ii < G_N_ELEMENTS (toolbar_items); ii++) {
		GtkToolItem *item;

		if (toolbar_items[ii].callback) {
			GtkWidget *icon;
			const gchar *icon_name;

			icon_name = self->priv->is_dark_theme ? toolbar_items[ii].icon_name_dark : toolbar_items[ii].icon_name;
			icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
			gtk_widget_show (GTK_WIDGET (icon));
			item = gtk_tool_button_new (icon, _(toolbar_items[ii].label));
			gtk_widget_set_name (GTK_WIDGET (item), toolbar_items[ii].icon_name);
			gtk_tool_item_set_tooltip_text (item, _(toolbar_items[ii].label));
			g_signal_connect_object (item, "clicked", toolbar_items[ii].callback, self, G_CONNECT_SWAPPED);
		} else {
			item = gtk_separator_tool_item_new ();
		}

		gtk_widget_show (GTK_WIDGET (item));
		gtk_toolbar_insert (self->priv->action_toolbar, item, -1);
	}

	#ifdef HAVE_MARKDOWN
	g_signal_connect_object (self->priv->notebook, "switch-page", G_CALLBACK (e_markdown_editor_switch_page_cb), self, 0);
	#endif

	g_signal_connect (self, "style-updated", G_CALLBACK (e_markdown_editor_style_updated_cb), NULL);
	g_signal_connect_object (gtk_text_view_get_buffer (self->priv->text_view), "changed", G_CALLBACK (e_markdown_editor_text_view_changed_cb), self, 0);
	e_signal_connect_notify_object (self->priv->text_view, "notify::editable", G_CALLBACK (e_markdown_editor_notify_editable_cb), self, 0);
}

static void
e_markdown_editor_finalize (GObject *object)
{
	EMarkdownEditor *self = E_MARKDOWN_EDITOR (object);

	g_clear_object (&self->priv->spell_checker);
	g_clear_pointer (&self->priv->signature_uid, g_free);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_markdown_editor_parent_class)->finalize (object);
}

static void
e_markdown_editor_class_init (EMarkdownEditorClass *klass)
{
	GObjectClass *object_class;
	GtkBindingSet *binding_set;

	klass->format_bold = e_markdown_editor_format_bold_text_cb;
	klass->format_italic = e_markdown_editor_format_italic_text_cb;
	klass->format_quote = e_markdown_editor_format_quote_cb;
	klass->format_code = e_markdown_editor_format_code_cb;
	klass->format_bullet_list = e_markdown_editor_format_bullet_list_cb;
	klass->format_numbered_list = e_markdown_editor_format_numbered_list_cb;
	klass->format_header = e_markdown_editor_format_header_cb;
	klass->insert_link = e_markdown_editor_insert_link_cb;
	klass->insert_emoji = e_markdown_editor_insert_emoji_cb;

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = e_markdown_editor_get_property;
	object_class->set_property = e_markdown_editor_set_property;
	object_class->constructed = e_markdown_editor_constructed;
	object_class->finalize = e_markdown_editor_finalize;

	g_object_class_override_property (object_class, PROP_IS_MALFUNCTION, "is-malfunction");
	g_object_class_override_property (object_class, PROP_CAN_COPY, "can-copy");
	g_object_class_override_property (object_class, PROP_CAN_CUT, "can-cut");
	g_object_class_override_property (object_class, PROP_CAN_PASTE, "can-paste");
	g_object_class_override_property (object_class, PROP_CAN_REDO, "can-redo");
	g_object_class_override_property (object_class, PROP_CAN_UNDO, "can-undo");
	g_object_class_override_property (object_class, PROP_CHANGED, "changed");
	g_object_class_override_property (object_class, PROP_MODE, "mode");
	g_object_class_override_property (object_class, PROP_EDITABLE, "editable");
	g_object_class_override_property (object_class, PROP_ALIGNMENT, "alignment");
	g_object_class_override_property (object_class, PROP_BACKGROUND_COLOR, "background-color");
	g_object_class_override_property (object_class, PROP_BLOCK_FORMAT, "block-format");
	g_object_class_override_property (object_class, PROP_BOLD, "bold");
	g_object_class_override_property (object_class, PROP_FONT_COLOR, "font-color");
	g_object_class_override_property (object_class, PROP_FONT_NAME, "font-name");
	g_object_class_override_property (object_class, PROP_FONT_SIZE, "font-size");
	g_object_class_override_property (object_class, PROP_INDENT_LEVEL, "indent-level");
	g_object_class_override_property (object_class, PROP_ITALIC, "italic");
	g_object_class_override_property (object_class, PROP_STRIKETHROUGH, "strikethrough");
	g_object_class_override_property (object_class, PROP_SUBSCRIPT, "subscript");
	g_object_class_override_property (object_class, PROP_SUPERSCRIPT, "superscript");
	g_object_class_override_property (object_class, PROP_UNDERLINE, "underline");
	g_object_class_override_property (object_class, PROP_START_BOTTOM, "start-bottom");
	g_object_class_override_property (object_class, PROP_TOP_SIGNATURE, "top-signature");
	g_object_class_override_property (object_class, PROP_SPELL_CHECK_ENABLED, "spell-check-enabled");
	g_object_class_override_property (object_class, PROP_VISUALLY_WRAP_LONG_LINES, "visually-wrap-long-lines");
	g_object_class_override_property (object_class, PROP_LAST_ERROR, "last-error");
	g_object_class_override_property (object_class, PROP_SPELL_CHECKER, "spell-checker");

	/**
	 * EMarkdownEditor::changed:
	 * @self: an #EMarkdownEditor, which sent the signal
	 *
	 * This signal is emitted the content of the @self changes.
	 *
	 * Since: 3.44
	 **/
	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMarkdownEditorClass, changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMarkdownEditor::format-bold:
	 * @self: an #EMarkdownEditor, which receives the signal
	 *
	 * A signal to set text format to bold.
	 *
	 * Since: 3.44
	 **/
	signals[FORMAT_BOLD] = g_signal_new (
		"format-bold",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMarkdownEditorClass, format_bold),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMarkdownEditor::format-italic:
	 * @self: an #EMarkdownEditor, which receives the signal
	 *
	 * A signal to set text format to italic.
	 *
	 * Since: 3.44
	 **/
	signals[FORMAT_ITALIC] = g_signal_new (
		"format-italic",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMarkdownEditorClass, format_italic),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMarkdownEditor::format-quote:
	 * @self: an #EMarkdownEditor, which receives the signal
	 *
	 * A signal to set text format to quote.
	 *
	 * Since: 3.44
	 **/
	signals[FORMAT_QUOTE] = g_signal_new (
		"format-quote",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMarkdownEditorClass, format_quote),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMarkdownEditor::format-code:
	 * @self: an #EMarkdownEditor, which receives the signal
	 *
	 * A signal to set text format to code.
	 *
	 * Since: 3.44
	 **/
	signals[FORMAT_CODE] = g_signal_new (
		"format-code",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMarkdownEditorClass, format_code),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMarkdownEditor::format-bullet-list:
	 * @self: an #EMarkdownEditor, which receives the signal
	 *
	 * A signal to set text format to bullet list.
	 *
	 * Since: 3.44
	 **/
	signals[FORMAT_BULLET_LIST] = g_signal_new (
		"format-bullet-list",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMarkdownEditorClass, format_bullet_list),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMarkdownEditor::format-numbered-list:
	 * @self: an #EMarkdownEditor, which receives the signal
	 *
	 * A signal to set text format to numbered list.
	 *
	 * Since: 3.44
	 **/
	signals[FORMAT_NUMBERED_LIST] = g_signal_new (
		"format-numbered-list",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMarkdownEditorClass, format_numbered_list),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMarkdownEditor::format-header:
	 * @self: an #EMarkdownEditor, which receives the signal
	 *
	 * A signal to set text format to header.
	 *
	 * Since: 3.44
	 **/
	signals[FORMAT_HEADER] = g_signal_new (
		"format-header",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMarkdownEditorClass, format_header),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMarkdownEditor::insert-link:
	 * @self: an #EMarkdownEditor, which receives the signal
	 *
	 * A signal to insert a link.
	 *
	 * Since: 3.44
	 **/
	signals[INSERT_LINK] = g_signal_new (
		"insert-link",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMarkdownEditorClass, insert_link),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	/**
	 * EMarkdownEditor::insert-emoji:
	 * @self: an #EMarkdownEditor, which receives the signal
	 *
	 * A signal to open a dialog to insert Emoji.
	 *
	 * Since: 3.44
	 **/
	signals[INSERT_EMOJI] = g_signal_new (
		"insert-emoji",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMarkdownEditorClass, insert_emoji),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	binding_set = gtk_binding_set_by_class (klass);

	gtk_binding_entry_add_signal (binding_set, GDK_KEY_b, GDK_CONTROL_MASK, "format-bold", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_i, GDK_CONTROL_MASK, "format-italic", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_k, GDK_CONTROL_MASK, "insert-link", 0);
}

static void
e_markdown_editor_content_editor_init (EContentEditorInterface *iface)
{
	iface->supports_mode = e_markdown_editor_supports_mode;
	iface->grab_focus = e_markdown_editor_grab_focus;
	iface->is_focus = e_markdown_editor_is_focus;
	iface->initialize = e_markdown_editor_initialize;
	iface->is_ready = e_markdown_editor_is_ready;
	iface->update_styles = e_markdown_editor_update_styles;
	iface->insert_content = e_markdown_editor_insert_content;
	iface->get_content = e_markdown_editor_get_content;
	iface->get_content_finish = e_markdown_editor_get_content_finish;
	iface->move_caret_on_coordinates = e_markdown_editor_move_caret_on_coordinates;
	iface->cut = e_markdown_editor_cut;
	iface->copy = e_markdown_editor_copy;
	iface->paste = e_markdown_editor_paste;
	iface->paste_primary = e_markdown_editor_paste_primary;
	iface->undo = e_markdown_editor_undo;
	iface->redo = e_markdown_editor_redo;
	iface->clear_undo_redo_history = e_markdown_editor_clear_undo_redo_history;
	iface->set_spell_checking_languages = e_markdown_editor_set_spell_checking_languages;
	iface->get_caret_word = e_markdown_editor_get_caret_word;
	iface->replace_caret_word = e_markdown_editor_replace_caret_word;
	iface->select_all = e_markdown_editor_select_all;
	iface->find = e_markdown_editor_find;
	iface->replace = e_markdown_editor_replace;
	iface->replace_all = e_markdown_editor_replace_all;
	iface->selection_save = e_markdown_editor_selection_save;
	iface->selection_restore = e_markdown_editor_selection_restore;
	iface->get_current_signature_uid = e_markdown_editor_get_current_signature_uid;
	iface->insert_signature = e_markdown_editor_insert_signature;
	iface->on_dialog_open = e_markdown_editor_on_dialog_open;
	iface->on_dialog_close = e_markdown_editor_on_dialog_close;
}

static void
e_markdown_editor_init (EMarkdownEditor *self)
{
	self->priv = e_markdown_editor_get_instance_private (self);

	self->priv->spell_checker = e_spell_checker_new ();
	self->priv->mode = E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT;
	self->priv->start_bottom = E_THREE_STATE_INCONSISTENT;
	self->priv->top_signature = E_THREE_STATE_INCONSISTENT;
}

/**
 * e_markdown_editor_new:
 *
 * Creates a new #EMarkdownEditor
 *
 * Returns: (transfer full): a new #EMarkdownEditor
 *
 * Since: 3.44
 */
GtkWidget *
e_markdown_editor_new (void)
{
	return g_object_new (E_TYPE_MARKDOWN_EDITOR, NULL);
}

/**
 * e_markdown_editor_connect_focus_tracker:
 * @self: an #EMarkdownEditor
 * @focus_tracker: an #EFocusTracker
 *
 * Connects @self widgets to the @focus_tracker.
 *
 * Since: 3.44
 **/
void
e_markdown_editor_connect_focus_tracker (EMarkdownEditor *self,
					 EFocusTracker *focus_tracker)
{
	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	e_widget_undo_attach (GTK_WIDGET (self->priv->text_view), focus_tracker);
}

/**
 * e_markdown_editor_get_text_view:
 * @self: an #EMarkdownEditor
 *
 * Returns: (transfer none): a #GtkTextView of the @self
 *
 * Since: 3.44
 **/
GtkTextView *
e_markdown_editor_get_text_view (EMarkdownEditor *self)
{
	g_return_val_if_fail (E_IS_MARKDOWN_EDITOR (self), NULL);

	return self->priv->text_view;
}

/**
 * e_markdown_editor_get_action_toolbar:
 * @self: an #EMarkdownEditor
 *
 * Returns: (transfer none): a #GtkToolbar of the @self, where the caller
 *    can add its own action buttons.
 *
 * Since: 3.44
 **/
GtkToolbar *
e_markdown_editor_get_action_toolbar (EMarkdownEditor *self)
{
	g_return_val_if_fail (E_IS_MARKDOWN_EDITOR (self), NULL);

	return self->priv->action_toolbar;
}

/**
 * e_markdown_editor_set_text:
 * @self an #EMarkdownEditor
 * @text: text to set
 *
 * Sets the @text as the editor content.
 *
 * Since: 3.44
 **/
void
e_markdown_editor_set_text (EMarkdownEditor *self,
			    const gchar *text)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));
	g_return_if_fail (text != NULL);

	buffer = gtk_text_view_get_buffer (self->priv->text_view);
	gtk_text_buffer_set_text (buffer, text, -1);
}

/**
 * e_markdown_editor_dup_text:
 * @self: an #EMarkdownEditor
 *
 * Get the markdown text entered in the @self. To get
 * the HTML version of it use e_markdown_editor_dup_html().
 * Free the returned string with g_free(), when no longer needed.
 *
 * Returns: (transfer full): the markdown text
 *
 * Since: 3.44
 **/
gchar *
e_markdown_editor_dup_text (EMarkdownEditor *self)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	g_return_val_if_fail (E_IS_MARKDOWN_EDITOR (self), NULL);

	buffer = gtk_text_view_get_buffer (self->priv->text_view);
	gtk_text_buffer_get_bounds (buffer, &start, &end);

	return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

/**
 * e_markdown_editor_dup_html:
 * @self: an #EMarkdownEditor
 *
 * Get the HTML version of the markdown text entered in the @self.
 * To get the markdown text use e_markdown_editor_dup_text().
 * Free the returned string with g_free(), when no longer needed.
 *
 * Note: The function can return %NULL when was not built
 *    with the markdown support.
 *
 * Returns: (transfer full) (nullable): the markdown text converted
 *    into HTML, or %NULL, when was not built with the markdown support
 *
 * Since: 3.44
 **/
gchar *
e_markdown_editor_dup_html (EMarkdownEditor *self)
{
	#ifdef HAVE_MARKDOWN
	gchar *text, *html;
	#endif

	g_return_val_if_fail (E_IS_MARKDOWN_EDITOR (self), NULL);

	#ifdef HAVE_MARKDOWN
	text = e_markdown_editor_dup_text (self);
	html = e_markdown_utils_text_to_html (text, -1);

	g_free (text);

	return html;
	#else
	return NULL;
	#endif
}

/**
 * e_markdown_editor_get_preview_mode:
 * @self: an #EMarkdownEditor
 *
 * Returns: whether the @self is in the preview mode; %FALSE means
 *    it is in the editing mode
 *
 * Since: 3.44
 **/
gboolean
e_markdown_editor_get_preview_mode (EMarkdownEditor *self)
{
	g_return_val_if_fail (E_IS_MARKDOWN_EDITOR (self), FALSE);

	return gtk_notebook_get_current_page (self->priv->notebook) == 1;
}

/**
 * e_markdown_editor_set_preview_mode:
 * @self: an #EMarkdownEditor
 * @preview_mode: %TRUE to set the preview mode, %FALSE otherwise
 *
 * Sets the @self into the preview mode, when @preview_mode is %TRUE, or
 * into editing mode, when @preview_mode is %FALSE.
 *
 * Note: The request to move to the preview mode can be silently ignored
 *    when the Evolution was not built with the markdown support.
 *
 * Since: 3.44
 **/
void
e_markdown_editor_set_preview_mode (EMarkdownEditor *self,
				    gboolean preview_mode)
{
	g_return_if_fail (E_IS_MARKDOWN_EDITOR (self));

	#ifdef HAVE_MARKDOWN
	gtk_notebook_set_current_page (self->priv->notebook, preview_mode ? 1 : 0);
	#else
	gtk_notebook_set_current_page (self->priv->notebook, 0);
	#endif
}