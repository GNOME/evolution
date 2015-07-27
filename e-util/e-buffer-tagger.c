/*
 * e-buffer-tagger.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <regex.h>
#include <string.h>
#include <ctype.h>

#include "e-buffer-tagger.h"

#include "e-misc-utils.h"

enum EBufferTaggerState {
	E_BUFFER_TAGGER_STATE_NONE = 0,
	E_BUFFER_TAGGER_STATE_INSDEL = 1 << 0, /* set when was called insert or delete of a text */
	E_BUFFER_TAGGER_STATE_CHANGED = 1 << 1, /* remark of the buffer is scheduled */
	E_BUFFER_TAGGER_STATE_IS_HOVERING = 1 << 2, /* mouse is over the link */
	E_BUFFER_TAGGER_STATE_IS_HOVERING_TOOLTIP = 1 << 3, /* mouse is over the link and the tooltip can be shown */
	E_BUFFER_TAGGER_STATE_CTRL_DOWN = 1 << 4  /* Ctrl key is down */
};

#define E_BUFFER_TAGGER_DATA_STATE "EBufferTagger::state"
#define E_BUFFER_TAGGER_LINK_TAG   "EBufferTagger::link"

struct _MagicInsertMatch {
	const gchar *regex;
	regex_t *preg;
	const gchar *prefix;
};

typedef struct _MagicInsertMatch MagicInsertMatch;

static MagicInsertMatch mim[] = {
	/* prefixed expressions */
	{ "(news|telnet|nntp|file|http|ftp|sftp|https|webcal)://([-a-z0-9]+(:[-a-z0-9]+)?@)?[-a-z0-9.]+[-a-z0-9](:[0-9]*)?(([.])?/[-a-z0-9_$.+!*(),;:@%&=?/~#']*[^]'.}>\\) \n\r\t,?!;:\"]?)?", NULL, NULL },
	{ "(sip|h323|callto|tel):([-_a-z0-9.'\\+]+(:[0-9]{1,5})?(/[-_a-z0-9.']+)?)(@([-_a-z0-9.%=?]+|([0-9]{1,3}.){3}[0-9]{1,3})?)?(:[0-9]{1,5})?", NULL, NULL },
	{ "mailto:[-_a-z0-9.'\\+]+@[-_a-z0-9.%=?]+", NULL, NULL },
	/* not prefixed expression */
	{ "www\\.[-a-z0-9.]+[-a-z0-9](:[0-9]*)?(([.])?/[-A-Za-z0-9_$.+!*(),;:@%&=?/~#]*[^]'.}>\\) \n\r\t,?!;:\"]?)?", NULL, "http://" },
	{ "ftp\\.[-a-z0-9.]+[-a-z0-9](:[0-9]*)?(([.])?/[-A-Za-z0-9_$.+!*(),;:@%&=?/~#]*[^]'.}>\\) \n\r\t,?!;:\"]?)?", NULL, "ftp://" },
	{ "[-_a-z0-9.'\\+]+@[-_a-z0-9.%=?]+", NULL, "mailto:" }
};

static void
init_magic_links (void)
{
	static gboolean done = FALSE;
	gint i;

	if (done)
		return;

	done = TRUE;

	for (i = 0; i < G_N_ELEMENTS (mim); i++) {
		mim[i].preg = g_new0 (regex_t, 1);
		if (regcomp (mim[i].preg, mim[i].regex, REG_EXTENDED | REG_ICASE)) {
			/* error */
			g_free (mim[i].preg);
			mim[i].preg = 0;
		}
	}
}

static void
markup_text (GtkTextBuffer *buffer)
{
	GtkTextIter start, end;
	gchar *text;
	gint i;
	regmatch_t pmatch[2];
	gboolean any;
	const gchar *str;
	gint offset = 0;

	g_return_if_fail (buffer != NULL);

	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);
	gtk_text_buffer_remove_tag_by_name (buffer, E_BUFFER_TAGGER_LINK_TAG, &start, &end);
	text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	str = text;
	any = TRUE;
	while (any) {
		any = FALSE;
		for (i = 0; i < G_N_ELEMENTS (mim); i++) {
			if (mim[i].preg && !regexec (mim[i].preg, str, 2, pmatch, 0)) {
				gint char_so, char_eo;

				char_so = g_utf8_pointer_to_offset (str, str + pmatch[0].rm_so);
				char_eo = g_utf8_pointer_to_offset (str, str + pmatch[0].rm_eo);

				gtk_text_buffer_get_iter_at_offset (buffer, &start, offset + char_so);
				gtk_text_buffer_get_iter_at_offset (buffer, &end, offset + char_eo);
				gtk_text_buffer_apply_tag_by_name (buffer, E_BUFFER_TAGGER_LINK_TAG, &start, &end);

				any = TRUE;
				str += pmatch[0].rm_eo;
				offset += char_eo;
				break;
			}
		}
	}

	g_free (text);
}

static void
get_pointer_position (GtkTextView *text_view,
                      gint *x,
                      gint *y)
{
	GdkWindow *window;
	GdkDisplay *display;
	GdkDeviceManager *device_manager;
	GdkDevice *device;

	window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_WIDGET);
	display = gdk_window_get_display (window);
	device_manager = gdk_display_get_device_manager (display);
	device = gdk_device_manager_get_client_pointer (device_manager);

	gdk_window_get_device_position (window, device, x, y, NULL);
}

static guint32
get_state (GtkTextBuffer *buffer)
{
	g_return_val_if_fail (buffer != NULL, E_BUFFER_TAGGER_STATE_NONE);
	g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), E_BUFFER_TAGGER_STATE_NONE);

	return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (buffer), E_BUFFER_TAGGER_DATA_STATE));
}

static void
set_state (GtkTextBuffer *buffer,
           guint32 state)
{
	g_object_set_data (G_OBJECT (buffer), E_BUFFER_TAGGER_DATA_STATE, GINT_TO_POINTER (state));
}

static void
update_state (GtkTextBuffer *buffer,
              guint32 value,
              gboolean do_set)
{
	guint32 state;

	g_return_if_fail (buffer != NULL);
	g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

	state = get_state (buffer);

	if (do_set)
		state = state | value;
	else
		state = state & (~value);

	set_state (buffer, state);
}

static gboolean
get_tag_bounds (GtkTextIter *iter,
                GtkTextTag *tag,
                GtkTextIter *start,
                GtkTextIter *end)
{
	gboolean res = FALSE;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (tag != NULL, FALSE);
	g_return_val_if_fail (start != NULL, FALSE);
	g_return_val_if_fail (end != NULL, FALSE);

	if (gtk_text_iter_has_tag (iter, tag)) {
		*start = *iter;
		*end = *iter;

		if (!gtk_text_iter_begins_tag (start, tag))
			gtk_text_iter_backward_to_tag_toggle (start, tag);

		if (!gtk_text_iter_ends_tag (end, tag))
			gtk_text_iter_forward_to_tag_toggle (end, tag);

		res = TRUE;
	}

	return res;
}

static gchar *
get_url_at_iter (GtkTextBuffer *buffer,
                 GtkTextIter *iter)
{
	GtkTextTagTable *tag_table;
	GtkTextTag *tag;
	GtkTextIter start, end;
	gchar *url = NULL;

	g_return_val_if_fail (buffer != NULL, NULL);

	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, E_BUFFER_TAGGER_LINK_TAG);
	g_return_val_if_fail (tag != NULL, NULL);

	if (get_tag_bounds (iter, tag, &start, &end))
		url = gtk_text_iter_get_text (&start, &end);

	return url;
}

static gboolean
invoke_link_if_present (GtkTextBuffer *buffer,
                        GtkTextIter *iter)
{
	gboolean res;
	gchar *url;

	g_return_val_if_fail (buffer != NULL, FALSE);

	url = get_url_at_iter (buffer, iter);

	res = url && *url;
	if (res)
		e_show_uri (NULL, url);

	g_free (url);

	return res;
}

static void
remove_tag_if_present (GtkTextBuffer *buffer,
                       GtkTextIter *where)
{
	GtkTextTagTable *tag_table;
	GtkTextTag *tag;
	GtkTextIter start, end;

	g_return_if_fail (buffer != NULL);
	g_return_if_fail (where != NULL);

	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, E_BUFFER_TAGGER_LINK_TAG);
	g_return_if_fail (tag != NULL);

	if (get_tag_bounds (where, tag, &start, &end))
		gtk_text_buffer_remove_tag (buffer, tag, &start, &end);
}

static void
buffer_insert_text (GtkTextBuffer *buffer,
                    GtkTextIter *location,
                    gchar *text,
                    gint len,
                    gpointer user_data)
{
	update_state (buffer, E_BUFFER_TAGGER_STATE_INSDEL, TRUE);
	remove_tag_if_present (buffer, location);
}

static void
buffer_delete_range (GtkTextBuffer *buffer,
                     GtkTextIter *start,
                     GtkTextIter *end,
                     gpointer user_data)
{
	update_state (buffer, E_BUFFER_TAGGER_STATE_INSDEL, TRUE);
	remove_tag_if_present (buffer, start);
	remove_tag_if_present (buffer, end);
}

static void
buffer_cursor_position (GtkTextBuffer *buffer,
                        gpointer user_data)
{
	guint32 state;

	state = get_state (buffer);
	if (state & E_BUFFER_TAGGER_STATE_INSDEL) {
		state = (state & (~E_BUFFER_TAGGER_STATE_INSDEL)) | E_BUFFER_TAGGER_STATE_CHANGED;
	} else {
		if (state & E_BUFFER_TAGGER_STATE_CHANGED) {
			markup_text (buffer);
		}

		state = state & (~ (E_BUFFER_TAGGER_STATE_CHANGED | E_BUFFER_TAGGER_STATE_INSDEL));
	}

	set_state (buffer, state);
}

static void
update_mouse_cursor (GtkTextView *text_view,
                     gint x,
                     gint y)
{
	static GdkCursor *hand_cursor = NULL;
	static GdkCursor *regular_cursor = NULL;
	gboolean hovering = FALSE, hovering_over_link = FALSE, hovering_real;
	guint32 state;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (text_view);
	GtkTextTagTable *tag_table;
	GtkTextTag *tag;
	GtkTextIter iter;

	if (!hand_cursor) {
		hand_cursor = gdk_cursor_new (GDK_HAND2);
		regular_cursor = gdk_cursor_new (GDK_XTERM);
	}

	g_return_if_fail (buffer != NULL);

	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, E_BUFFER_TAGGER_LINK_TAG);
	g_return_if_fail (tag != NULL);

	state = get_state (buffer);

	gtk_text_view_get_iter_at_location (text_view, &iter, x, y);
	hovering_real = gtk_text_iter_has_tag (&iter, tag);

	hovering_over_link = (state & E_BUFFER_TAGGER_STATE_IS_HOVERING) != 0;
	if ((state & E_BUFFER_TAGGER_STATE_CTRL_DOWN) == 0) {
		hovering = FALSE;
	} else {
		hovering = hovering_real;
	}

	if (hovering != hovering_over_link) {
		update_state (buffer, E_BUFFER_TAGGER_STATE_IS_HOVERING, hovering);

		if (hovering && gtk_widget_has_focus (GTK_WIDGET (text_view)))
			gdk_window_set_cursor (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT), hand_cursor);
		else
			gdk_window_set_cursor (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT), regular_cursor);

		/* XXX Is this necessary?  Appears to be a no-op. */
		get_pointer_position (text_view, NULL, NULL);
	}

	hovering_over_link = (state & E_BUFFER_TAGGER_STATE_IS_HOVERING_TOOLTIP) != 0;

	if (hovering_real != hovering_over_link) {
		update_state (buffer, E_BUFFER_TAGGER_STATE_IS_HOVERING_TOOLTIP, hovering_real);

		gtk_widget_trigger_tooltip_query (GTK_WIDGET (text_view));
	}
}

static gboolean
textview_query_tooltip (GtkTextView *text_view,
                        gint x,
                        gint y,
                        gboolean keyboard_mode,
                        GtkTooltip *tooltip,
                        gpointer user_data)
{
	GtkTextBuffer *buffer;
	guint32 state;
	gboolean res = FALSE;

	if (keyboard_mode)
		return FALSE;

	buffer = gtk_text_view_get_buffer (text_view);
	g_return_val_if_fail (buffer != NULL, FALSE);

	state = get_state (buffer);

	if ((state & E_BUFFER_TAGGER_STATE_IS_HOVERING_TOOLTIP) != 0) {
		gchar *url;
		GtkTextIter iter;

		gtk_text_view_window_to_buffer_coords (
			text_view,
			GTK_TEXT_WINDOW_WIDGET,
			x, y, &x, &y);
		gtk_text_view_get_iter_at_location (text_view, &iter, x, y);

		url = get_url_at_iter (buffer, &iter);
		res = url && *url;

		if (res) {
			gchar *str;

			/* To Translators: The text is concatenated to a form: "Ctrl-click to open a link http://www.example.com" */
			str = g_strconcat (_("Ctrl-click to open a link"), " ", url, NULL);
			gtk_tooltip_set_text (tooltip, str);
			g_free (str);
		}

		g_free (url);
	}

	return res;
}

/* Links can be activated by pressing Enter. */
static gboolean
textview_key_press_event (GtkWidget *text_view,
                          GdkEventKey *event)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;

	if ((event->state & GDK_CONTROL_MASK) == 0)
		return FALSE;

	switch (event->keyval) {
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));
		if (invoke_link_if_present (buffer, &iter))
			return TRUE;
		break;

	default:
		break;
	}

	return FALSE;
}

static void
update_ctrl_state (GtkTextView *textview,
                   gboolean ctrl_is_down)
{
	GtkTextBuffer *buffer;
	gint x, y;

	buffer = gtk_text_view_get_buffer (textview);
	if (buffer) {
		if (((get_state (buffer) & E_BUFFER_TAGGER_STATE_CTRL_DOWN) != 0) != (ctrl_is_down != FALSE)) {
			update_state (buffer, E_BUFFER_TAGGER_STATE_CTRL_DOWN, ctrl_is_down != FALSE);
		}

		get_pointer_position (textview, &x, &y);
		gtk_text_view_window_to_buffer_coords (textview, GTK_TEXT_WINDOW_WIDGET, x, y, &x, &y);
		update_mouse_cursor (textview, x, y);
	}
}

/* Links can also be activated by clicking. */
static gboolean
textview_event_after (GtkTextView *textview,
                      GdkEvent *event)
{
	GtkTextIter start, end, iter;
	GtkTextBuffer *buffer;
	gint x, y;
	GdkModifierType mt = 0;
	guint event_button = 0;
	gdouble event_x_win = 0;
	gdouble event_y_win = 0;

	g_return_val_if_fail (GTK_IS_TEXT_VIEW (textview), FALSE);

	if (event->type == GDK_KEY_PRESS || event->type == GDK_KEY_RELEASE) {
		guint event_keyval = 0;

		gdk_event_get_keyval (event, &event_keyval);

		switch (event_keyval) {
			case GDK_KEY_Control_L:
			case GDK_KEY_Control_R:
				update_ctrl_state (
					textview,
					event->type == GDK_KEY_PRESS);
				break;
		}

		return FALSE;
	}

	if (!gdk_event_get_state (event, &mt)) {
		GdkWindow *window;
		GdkDisplay *display;
		GdkDeviceManager *device_manager;
		GdkDevice *device;

		window = gtk_widget_get_parent_window (GTK_WIDGET (textview));
		display = gdk_window_get_display (window);
		device_manager = gdk_display_get_device_manager (display);
		device = gdk_device_manager_get_client_pointer (device_manager);

		gdk_window_get_device_position (window, device, NULL, NULL, &mt);
	}

	update_ctrl_state (textview, (mt & GDK_CONTROL_MASK) != 0);

	if (event->type != GDK_BUTTON_RELEASE)
		return FALSE;

	gdk_event_get_button (event, &event_button);
	gdk_event_get_coords (event, &event_x_win, &event_y_win);

	if (event_button != 1 || (mt & GDK_CONTROL_MASK) == 0)
		return FALSE;

	buffer = gtk_text_view_get_buffer (textview);

	/* we shouldn't follow a link if the user has selected something */
	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
		return FALSE;

	gtk_text_view_window_to_buffer_coords (
		textview,
		GTK_TEXT_WINDOW_WIDGET,
		event_x_win, event_y_win, &x, &y);

	gtk_text_view_get_iter_at_location (textview, &iter, x, y);

	invoke_link_if_present (buffer, &iter);
	update_mouse_cursor (textview, x, y);

	return FALSE;
}

static gboolean
textview_motion_notify_event (GtkTextView *textview,
                              GdkEventMotion *event)
{
	gint x, y;

	g_return_val_if_fail (GTK_IS_TEXT_VIEW (textview), FALSE);

	gtk_text_view_window_to_buffer_coords (
		textview,
		GTK_TEXT_WINDOW_WIDGET,
		event->x, event->y, &x, &y);

	update_mouse_cursor (textview, x, y);

	return FALSE;
}

static gboolean
textview_visibility_notify_event (GtkTextView *textview,
                                  GdkEventVisibility *event)
{
	gint wx, wy, bx, by;

	g_return_val_if_fail (GTK_IS_TEXT_VIEW (textview), FALSE);

	get_pointer_position (textview, &wx, &wy);

	gtk_text_view_window_to_buffer_coords (
		textview,
		GTK_TEXT_WINDOW_WIDGET,
		wx, wy, &bx, &by);

	update_mouse_cursor (textview, bx, by);

	return FALSE;
}

void
e_buffer_tagger_connect (GtkTextView *textview)
{
	GtkTextBuffer *buffer;
	GtkTextTagTable *tag_table;
	GtkTextTag *tag;

	init_magic_links ();

	g_return_if_fail (textview != NULL);
	g_return_if_fail (GTK_IS_TEXT_VIEW (textview));

	buffer = gtk_text_view_get_buffer (textview);
	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, E_BUFFER_TAGGER_LINK_TAG);

	/* if tag is there already, then it is connected, thus claim */
	g_return_if_fail (tag == NULL);

	gtk_text_buffer_create_tag (
		buffer, E_BUFFER_TAGGER_LINK_TAG,
		"foreground", "blue",
		"underline", PANGO_UNDERLINE_SINGLE,
		NULL);

	set_state (buffer, E_BUFFER_TAGGER_STATE_NONE);

	g_signal_connect (
		buffer, "insert-text",
		G_CALLBACK (buffer_insert_text), NULL);
	g_signal_connect (
		buffer, "delete-range",
		G_CALLBACK (buffer_delete_range), NULL);
	g_signal_connect (
		buffer, "notify::cursor-position",
		G_CALLBACK (buffer_cursor_position), NULL);

	gtk_widget_set_has_tooltip (GTK_WIDGET (textview), TRUE);

	g_signal_connect (
		textview, "query-tooltip",
		G_CALLBACK (textview_query_tooltip), NULL);
	g_signal_connect (
		textview, "key-press-event",
		G_CALLBACK (textview_key_press_event), NULL);
	g_signal_connect (
		textview, "event-after",
		G_CALLBACK (textview_event_after), NULL);
	g_signal_connect (
		textview, "motion-notify-event",
		G_CALLBACK (textview_motion_notify_event), NULL);
	g_signal_connect (
		textview, "visibility-notify-event",
		G_CALLBACK (textview_visibility_notify_event), NULL);
}

void
e_buffer_tagger_disconnect (GtkTextView *textview)
{
	GtkTextBuffer *buffer;
	GtkTextTagTable *tag_table;
	GtkTextTag *tag;

	g_return_if_fail (textview != NULL);
	g_return_if_fail (GTK_IS_TEXT_VIEW (textview));

	buffer = gtk_text_view_get_buffer (textview);
	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, E_BUFFER_TAGGER_LINK_TAG);

	/* if tag is not there, then it is not connected, thus claim */
	g_return_if_fail (tag != NULL);

	gtk_text_tag_table_remove (tag_table, tag);

	set_state (buffer, E_BUFFER_TAGGER_STATE_NONE);

	g_signal_handlers_disconnect_by_func (buffer, G_CALLBACK (buffer_insert_text), NULL);
	g_signal_handlers_disconnect_by_func (buffer, G_CALLBACK (buffer_delete_range), NULL);
	g_signal_handlers_disconnect_by_func (buffer, G_CALLBACK (buffer_cursor_position), NULL);

	gtk_widget_set_has_tooltip (GTK_WIDGET (textview), FALSE);

	g_signal_handlers_disconnect_by_func (textview, G_CALLBACK (textview_query_tooltip), NULL);
	g_signal_handlers_disconnect_by_func (textview, G_CALLBACK (textview_key_press_event), NULL);
	g_signal_handlers_disconnect_by_func (textview, G_CALLBACK (textview_event_after), NULL);
	g_signal_handlers_disconnect_by_func (textview, G_CALLBACK (textview_motion_notify_event), NULL);
	g_signal_handlers_disconnect_by_func (textview, G_CALLBACK (textview_visibility_notify_event), NULL);
}

void
e_buffer_tagger_update_tags (GtkTextView *textview)
{
	GtkTextBuffer *buffer;
	GtkTextTagTable *tag_table;
	GtkTextTag *tag;

	g_return_if_fail (textview != NULL);
	g_return_if_fail (GTK_IS_TEXT_VIEW (textview));

	buffer = gtk_text_view_get_buffer (textview);
	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, E_BUFFER_TAGGER_LINK_TAG);

	/* if tag is not there, then it is not connected, thus claim */
	g_return_if_fail (tag != NULL);

	update_state (buffer, E_BUFFER_TAGGER_STATE_INSDEL | E_BUFFER_TAGGER_STATE_CHANGED, FALSE);

	markup_text (buffer);
}
