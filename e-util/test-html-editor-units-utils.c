/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "e-util/e-util.h"

#include "test-html-editor-units-utils.h"

static guint event_processing_delay_ms = 5;

void
test_utils_set_event_processing_delay_ms (guint value)
{
	event_processing_delay_ms = value;
}

guint
test_utils_get_event_processing_delay_ms (void)
{
	return event_processing_delay_ms;
}

typedef struct _UndoContent {
	gchar *html;
	gchar *plain;
} UndoContent;

static UndoContent *
undo_content_new (TestFixture *fixture)
{
	EContentEditor *cnt_editor;
	UndoContent *uc;

	g_return_val_if_fail (fixture != NULL, NULL);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), NULL);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);

	uc = g_new0 (UndoContent, 1);
	uc->html = e_content_editor_get_content (cnt_editor, E_CONTENT_EDITOR_GET_PROCESSED | E_CONTENT_EDITOR_GET_TEXT_HTML, NULL, NULL);
	uc->plain = e_content_editor_get_content (cnt_editor, E_CONTENT_EDITOR_GET_PROCESSED | E_CONTENT_EDITOR_GET_TEXT_PLAIN, NULL, NULL);

	g_warn_if_fail (uc->html != NULL);
	g_warn_if_fail (uc->plain != NULL);

	return uc;
}

static void
undo_content_free (gpointer ptr)
{
	UndoContent *uc = ptr;

	if (uc) {
		g_free (uc->html);
		g_free (uc->plain);
		g_free (uc);
	}
}

static gboolean
undo_content_test (TestFixture *fixture,
		   const UndoContent *uc)
{
	EContentEditor *cnt_editor;
	gchar *text;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (uc != NULL, FALSE);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);

	text = e_content_editor_get_content (cnt_editor, E_CONTENT_EDITOR_GET_PROCESSED | E_CONTENT_EDITOR_GET_TEXT_HTML, NULL, NULL);
	g_return_val_if_fail (text != NULL, FALSE);

	if (!test_utils_html_equal (fixture, text, uc->html)) {
		g_warning ("%s: returned HTML\n---%s---\n and expected HTML\n---%s---\n do not match", G_STRFUNC, text, uc->html);
		g_free (text);
		return FALSE;
	}

	g_free (text);

	text = e_content_editor_get_content (cnt_editor, E_CONTENT_EDITOR_GET_PROCESSED | E_CONTENT_EDITOR_GET_TEXT_PLAIN, NULL, NULL);
	g_return_val_if_fail (text != NULL, FALSE);

	if (!test_utils_html_equal (fixture, text, uc->plain)) {
		g_warning ("%s: returned Plain\n---%s---\n and expected Plain\n---%s---\n do not match", G_STRFUNC, text, uc->plain);
		g_free (text);
		return FALSE;
	}

	g_free (text);

	return TRUE;
}

void
test_utils_fixture_set_up (TestFixture *fixture,
			   gconstpointer user_data)
{
	EContentEditor *cnt_editor;
	GSettings *settings;
	gpointer async_data;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	fixture->prompt_on_composer_mode_switch = g_settings_get_boolean (settings, "prompt-on-composer-mode-switch");
	fixture->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	fixture->editor = E_HTML_EDITOR (e_html_editor_new ());
	fixture->undo_stack = NULL;

	g_object_set (G_OBJECT (fixture->editor),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_widget_show (GTK_WIDGET (fixture->editor));
	gtk_container_add (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->editor));

	/* Turn this off; it may be left off on test failures */
	g_settings_set_boolean (settings, "prompt-on-composer-mode-switch", FALSE);

	g_clear_object (&settings);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	g_object_set (G_OBJECT (cnt_editor),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"height-request", 150,
		NULL);

	async_data = test_utils_async_call_prepare ();

	g_signal_connect_swapped (cnt_editor, "notify::web-extension",
		G_CALLBACK (test_utils_async_call_finish), async_data);

	gtk_window_set_focus (GTK_WINDOW (fixture->window), GTK_WIDGET (cnt_editor));
	gtk_widget_show (fixture->window);

	test_utils_async_call_wait (async_data, 5);
}

void
test_utils_fixture_tear_down (TestFixture *fixture,
			      gconstpointer user_data)
{
	GSettings *settings;

	gtk_widget_destroy (GTK_WIDGET (fixture->window));
	fixture->editor = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_settings_set_boolean (settings, "prompt-on-composer-mode-switch", fixture->prompt_on_composer_mode_switch);
	g_clear_object (&settings);

	g_slist_free_full (fixture->undo_stack, undo_content_free);
	fixture->undo_stack = NULL;
}

static void
test_utils_flush_main_context (void)
{
	GMainContext *main_context;

	main_context = g_main_context_default ();

	while (g_main_context_pending (main_context)) {
		g_main_context_iteration (main_context, FALSE);
	}
}

gpointer
test_utils_async_call_prepare (void)
{
	return g_main_loop_new (NULL, FALSE);
}

typedef struct _AsynCallData {
	GMainLoop *loop;
	gboolean timeout_reached;
} AsyncCallData;

static gboolean
test_utils_async_call_timeout_reached_cb (gpointer user_data)
{
	AsyncCallData *async_call_data = user_data;

	g_return_val_if_fail (async_call_data != NULL, FALSE);
	g_return_val_if_fail (async_call_data->loop != NULL, FALSE);
	g_return_val_if_fail (!async_call_data->timeout_reached, FALSE);

	if (!g_source_is_destroyed (g_main_current_source ())) {
		async_call_data->timeout_reached = TRUE;
		g_main_loop_quit (async_call_data->loop);
	}

	return FALSE;
}

gboolean
test_utils_async_call_wait (gpointer async_data,
			    guint timeout_seconds)
{
	GMainLoop *loop = async_data;
	AsyncCallData async_call_data;
	GSource *source = NULL;

	g_return_val_if_fail (loop != NULL, FALSE);

	async_call_data.loop = loop;
	async_call_data.timeout_reached = FALSE;

	/* 0 is to wait forever */
	if (timeout_seconds > 0) {
		source = g_timeout_source_new_seconds (timeout_seconds);
		g_source_set_callback (source, test_utils_async_call_timeout_reached_cb, &async_call_data, NULL);
		g_source_attach (source, NULL);
	}

	g_main_loop_run (loop);

	if (source) {
		g_source_destroy (source);
		g_source_unref (source);
	}

	test_utils_flush_main_context ();

	g_main_loop_unref (loop);

	return !async_call_data.timeout_reached;
}

gboolean
test_utils_async_call_finish (gpointer async_data)
{
	GMainLoop *loop = async_data;

	g_return_val_if_fail (loop != NULL, FALSE);

	g_main_loop_quit (loop);

	return FALSE;
}

gboolean
test_utils_wait_milliseconds (guint milliseconds)
{
	gpointer async_data;

	async_data = test_utils_async_call_prepare ();
	g_timeout_add (milliseconds, test_utils_async_call_finish, async_data);

	return test_utils_async_call_wait (async_data, milliseconds / 1000 + 1);
}

static void
test_utils_send_key_event (GtkWidget *widget,
			   GdkEventType type,
			   guint keyval,
			   guint state)
{
	GdkKeymap *keymap;
	GdkKeymapKey *keys = NULL;
	gint n_keys;
	GdkEvent *event;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	event = gdk_event_new (type);
	event->key.is_modifier =
		keyval == GDK_KEY_Shift_L ||
		keyval == GDK_KEY_Shift_R ||
		keyval == GDK_KEY_Control_L ||
		keyval == GDK_KEY_Control_R ||
		keyval == GDK_KEY_Alt_L ||
		keyval == GDK_KEY_Alt_R;
	event->key.keyval = keyval;
	event->key.state = state;
	event->key.window = g_object_ref (gtk_widget_get_window (widget));
	event->key.send_event = TRUE;
	event->key.length = 0;
	event->key.string = NULL;
	event->key.hardware_keycode = 0;
	event->key.group = 0;
	event->key.time = GDK_CURRENT_TIME;

	gdk_event_set_device (event, gdk_seat_get_keyboard (gdk_display_get_default_seat (gtk_widget_get_display (widget))));

	keymap = gdk_keymap_get_for_display (gtk_widget_get_display (widget));
	if (gdk_keymap_get_entries_for_keyval (keymap, keyval, &keys, &n_keys)) {
		if (n_keys > 0) {
			event->key.hardware_keycode = keys[0].keycode;
			event->key.group = keys[0].group;
		}

		g_free (keys);
	}

	gtk_main_do_event (event);

	test_utils_wait_milliseconds (event_processing_delay_ms);

	gdk_event_free (event);
}

gboolean
test_utils_type_text (TestFixture *fixture,
		      const gchar *text)
{
	GtkWidget *widget;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);

	widget = GTK_WIDGET (e_html_editor_get_content_editor (fixture->editor));
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	g_return_val_if_fail (text != NULL, FALSE);
	g_return_val_if_fail (g_utf8_validate (text, -1, NULL), FALSE);

	while (*text) {
		guint keyval;
		gunichar unichar;

		unichar = g_utf8_get_char (text);
		text = g_utf8_next_char (text);

		keyval = gdk_unicode_to_keyval (unichar);

		test_utils_send_key_event (widget, GDK_KEY_PRESS, keyval, 0);
		test_utils_send_key_event (widget, GDK_KEY_RELEASE, keyval, 0);
	}

	test_utils_wait_milliseconds (event_processing_delay_ms);

	return TRUE;
}

gboolean
test_utils_html_equal (TestFixture *fixture,
		       const gchar *html1,
		       const gchar *html2)
{
	EContentEditor *cnt_editor;
	GDBusProxy *web_extension = NULL;
	GVariant *result;
	GError *error = NULL;
	gboolean html_equal = FALSE;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (html1 != NULL, FALSE);
	g_return_val_if_fail (html2 != NULL, FALSE);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	g_return_val_if_fail (cnt_editor != NULL, FALSE);

	g_object_get (cnt_editor, "web-extension", &web_extension, NULL);

	g_return_val_if_fail (G_IS_DBUS_PROXY (web_extension), FALSE);

	result = g_dbus_proxy_call_sync (
		web_extension,
		"TestHtmlEqual",
		g_variant_new ("(tss)", webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (cnt_editor)), html1, html2),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&error);
	g_assert_no_error (error);

	g_clear_error (&error);

	g_return_val_if_fail (result != NULL, FALSE);

	g_variant_get (result, "(b)", &html_equal);
	g_variant_unref (result);

	return html_equal;
}

static gboolean
test_utils_process_sequence (TestFixture *fixture,
			     const gchar *sequence)
{
	GtkWidget *widget;
	const gchar *seq;
	guint keyval, state;
	gboolean success = TRUE;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (sequence != NULL, FALSE);

	widget = GTK_WIDGET (e_html_editor_get_content_editor (fixture->editor));
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	state = 0;

	for (seq = sequence; *seq && success; seq++) {
		gboolean call_press = TRUE, call_release = TRUE;
		guint change_state = state;

		switch (*seq) {
		case 'S': /* Shift key press */
			keyval = GDK_KEY_Shift_L;

			if ((state & GDK_SHIFT_MASK) != 0) {
				success = FALSE;
				g_warning ("%s: Shift is already pressed", G_STRFUNC);
			} else {
				change_state |= GDK_SHIFT_MASK;
			}
			call_release = FALSE;
			break;
		case 's': /* Shift key release */
			keyval = GDK_KEY_Shift_L;

			if ((state & GDK_SHIFT_MASK) == 0) {
				success = FALSE;
				g_warning ("%s: Shift is already released", G_STRFUNC);
			} else {
				change_state &= ~GDK_SHIFT_MASK;
			}
			call_press = FALSE;
			break;
		case 'C': /* Ctrl key press */
			keyval = GDK_KEY_Control_L;

			if ((state & GDK_CONTROL_MASK) != 0) {
				success = FALSE;
				g_warning ("%s: Control is already pressed", G_STRFUNC);
			} else {
				change_state |= GDK_CONTROL_MASK;
			}
			call_release = FALSE;
			break;
		case 'c': /* Ctrl key release */
			keyval = GDK_KEY_Control_L;

			if ((state & GDK_CONTROL_MASK) == 0) {
				success = FALSE;
				g_warning ("%s: Control is already released", G_STRFUNC);
			} else {
				change_state &= ~GDK_CONTROL_MASK;
			}
			call_press = FALSE;
			break;
		case 'h': /* Home key press + release */
			keyval = GDK_KEY_Home;
			break;
		case 'e': /* End key press + release */
			keyval = GDK_KEY_End;
			break;
		case 'P': /* Page-Up key press + release */
			keyval = GDK_KEY_Page_Up;
			break;
		case 'p': /* Page-Down key press + release */
			keyval = GDK_KEY_Page_Down;
			break;
		case 'l': /* Arrow-Left key press + release */
			keyval = GDK_KEY_Left;
			break;
		case 'r': /* Arrow-Right key press + release */
			keyval = GDK_KEY_Right;
			break;
		case 'u': /* Arrow-Up key press + release */
			keyval = GDK_KEY_Up;
			break;
		case 'd': /* Arrow-Down key press + release */
			keyval = GDK_KEY_Down;
			break;
		case 'D': /* Delete key press + release */
			keyval = GDK_KEY_Delete;
			break;
		case 'b': /* Backspace key press + release */
			keyval = GDK_KEY_BackSpace;
			break;
		case 't': /* Tab key press + release */
			keyval = GDK_KEY_Tab;
			break;
		case 'n': /* Return key press + release */
			keyval = GDK_KEY_Return;
			break;
		default:
			success = FALSE;
			g_warning ("%s: Unknown sequence command '%c' in sequence '%s'", G_STRFUNC, *seq, sequence);
			break;
		}

		if (success) {
			if (call_press)
				test_utils_send_key_event (widget, GDK_KEY_PRESS, keyval, state);

			if (call_release)
				test_utils_send_key_event (widget, GDK_KEY_RELEASE, keyval, state);
		}

		state = change_state;
	}

	test_utils_wait_milliseconds (event_processing_delay_ms);

	return success;
}

static gboolean
test_utils_execute_action (TestFixture *fixture,
			   const gchar *action_name)
{
	GtkAction *action;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (action_name != NULL, FALSE);

	action = e_html_editor_get_action (fixture->editor, action_name);
	if (action) {
		gtk_action_activate (action);
	} else {
		g_warning ("%s: Failed to find action '%s'", G_STRFUNC, action_name);
		return FALSE;
	}

	return TRUE;
}

/* Expects only the part like "undo" [ ":" number ] */
static gint
test_utils_maybe_extract_undo_number (const gchar *command)
{
	const gchar *ptr;
	gint number;

	g_return_val_if_fail (command != NULL, -1);

	ptr = strchr (command, ':');
	if (!ptr)
		return 1;

	number = atoi (ptr + 1);
	g_return_val_if_fail (number > 0, -1);

	return number;
}

static const UndoContent *
test_utils_pick_undo_content (const GSList *undo_stack,
			      gint number)
{
	const GSList *link;

	g_return_val_if_fail (undo_stack != NULL, NULL);

	for (link = undo_stack; link && number > 0; link = g_slist_next (link)) {
		number--;
	}

	g_return_val_if_fail (link != NULL, NULL);
	g_return_val_if_fail (link->data != NULL, NULL);

	return link->data;
}

/* Each line of 'commands' contains one command.

   commands  = command *("\n" command)

   command   = actioncmd ; Execute an action
             / modecmd   ; Change editor mode to HTML or Plain Text
             / seqcmd    ; Sequence of special key strokes
             / typecmd   ; Type a text
             / undocmd   ; Undo/redo commands

   actioncmd = "action:" name

   actioncmd = "mode:" ("html" / "plain")

   seqcmd    = "seq:" sequence

   sequence  = "S" ; Shift key press
             / "s" ; Shift key release
             / "C" ; Ctrl key press
             / "c" ; Ctrl key release
             / "h" ; Home key press + release
             / "e" ; End key press + release
             / "P" ; Page-Up key press + release
             / "p" ; Page-Down key press + release
             / "l" ; Arrow-Left key press + release
             / "r" ; Arrow-Right key press + release
             / "u" ; Arrow-Up key press + release
             / "d" ; Arrow-Down key press + release
             / "D" ; Delete key press + release
             / "b" ; Backspace key press + release
             / "t" ; Tab key press + release
             / "n" ; Return key press + release

   typecmd   = "type:" text ; the 'text' can contain escaped letters with a backslash, like "\\n" transforms into "\n"

   undocmd   = "undo:" undotype

   undotype  = "undo" [ ":" number ] ; Call 'undo', number-times; if 'number' is not provided, then call it exactly once
             / "redo" [ ":" number ] ; Call 'redo', number-times; if 'number' is not provided, then call it exactly once
	     / "save"                ; Save current content of the editor for later tests
	     / "drop" [ ":" number ] ; Forgets saved content, if 'number' is provided, then top number saves are forgotten
	     / "test" [ ":" number ] ; Tests current editor content against any previously saved state; the optional
                                     ; 'number' argument can be used to specify which exact previous state to use
 */
gboolean
test_utils_process_commands (TestFixture *fixture,
			     const gchar *commands)
{
	gchar **cmds;
	gint cc;
	gboolean success = TRUE;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (commands != NULL, FALSE);

	cmds = g_strsplit (commands, "\n", -1);
	for (cc = 0; cmds && cmds[cc] && success; cc++) {
		const gchar *command = cmds[cc];

		if (g_str_has_prefix (command, "action:")) {
			test_utils_execute_action (fixture, command + 7);
		} else if (g_str_has_prefix (command, "mode:")) {
			const gchar *mode_change = command + 5;

			if (g_str_equal (mode_change, "html")) {
				test_utils_execute_action (fixture, "mode-html");
			} else if (g_str_equal (mode_change, "plain")) {
				test_utils_execute_action (fixture, "mode-plain");
			} else {
				success = FALSE;
				g_warning ("%s: Unknown mode '%s'", G_STRFUNC, mode_change);
			}
		} else if (g_str_has_prefix (command, "seq:")) {
			success = test_utils_process_sequence (fixture, command + 4);
		} else if (g_str_has_prefix (command, "type:")) {
			gchar *text;

			text = g_strcompress (command + 5);
			success = test_utils_type_text (fixture, text);
			if (!success)
				g_warning ("%s: Failed to type text '%s'", G_STRFUNC, text);
			g_free (text);
		} else if (g_str_has_prefix (command, "undo:")) {
			gint number;

			command += 5;

			if (g_str_equal (command, "undo") || g_str_has_prefix (command, "undo:")) {
				number = test_utils_maybe_extract_undo_number (command);
				while (number > 0 && success) {
					success = test_utils_execute_action (fixture, "undo");
					number--;
				}
			} else if (g_str_has_prefix (command, "redo") || g_str_has_prefix (command, "redo:")) {
				number = test_utils_maybe_extract_undo_number (command);
				while (number > 0 && success) {
					success = test_utils_execute_action (fixture, "redo");
					number--;
				}
			} else if (g_str_equal (command, "save")) {
				UndoContent *uc;

				uc = undo_content_new (fixture);
				fixture->undo_stack = g_slist_prepend (fixture->undo_stack, uc);
			} else if (g_str_equal (command, "drop") || g_str_has_prefix (command, "drop:")) {
				number = test_utils_maybe_extract_undo_number (command);
				g_warn_if_fail (number <= g_slist_length (fixture->undo_stack));

				while (number > 0 && fixture->undo_stack) {
					UndoContent *uc = fixture->undo_stack->data;

					fixture->undo_stack = g_slist_remove (fixture->undo_stack, uc);
					undo_content_free (uc);
					number--;
				}
			} else if (g_str_equal (command, "test") || g_str_has_prefix (command, "test:")) {
				const UndoContent *uc;

				number = test_utils_maybe_extract_undo_number (command);
				uc = test_utils_pick_undo_content (fixture->undo_stack, number);
				success = uc && undo_content_test (fixture, uc);
			} else {
				g_warning ("%s: Unknown command 'undo:%s'", G_STRFUNC, command);
				success = FALSE;
			}

			test_utils_wait_milliseconds (event_processing_delay_ms);
		} else if (*command) {
			g_warning ("%s: Unknown command '%s'", G_STRFUNC, command);
			success = FALSE;
		}

		test_utils_wait_milliseconds (event_processing_delay_ms);
	}

	g_strfreev (cmds);

	if (success) {
		/* Give the editor some time to finish any ongoing async operations */
		test_utils_wait_milliseconds (MAX (event_processing_delay_ms, 100));
	}

	return success;
}

gboolean
test_utils_run_simple_test (TestFixture *fixture,
			    const gchar *commands,
			    const gchar *expected_html,
			    const gchar *expected_plain)
{
	EContentEditor *cnt_editor;
	gchar *text;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (commands != NULL, FALSE);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);

	if (!test_utils_process_commands (fixture, commands))
		return FALSE;

	if (expected_html) {
		text = e_content_editor_get_content (cnt_editor, E_CONTENT_EDITOR_GET_PROCESSED | E_CONTENT_EDITOR_GET_TEXT_HTML, NULL, NULL);
		g_return_val_if_fail (text != NULL, FALSE);

		if (!test_utils_html_equal (fixture, text, expected_html)) {
			g_warning ("%s: returned HTML\n---%s---\n and expected HTML\n---%s---\n do not match", G_STRFUNC, text, expected_html);
			g_free (text);
			return FALSE;
		}

		g_free (text);
	}

	if (expected_plain) {
		text = e_content_editor_get_content (cnt_editor, E_CONTENT_EDITOR_GET_PROCESSED | E_CONTENT_EDITOR_GET_TEXT_PLAIN, NULL, NULL);
		g_return_val_if_fail (text != NULL, FALSE);

		if (!test_utils_html_equal (fixture, text, expected_plain)) {
			g_warning ("%s: returned Plain\n---%s---\n and expected Plain\n---%s---\n do not match", G_STRFUNC, text, expected_plain);
			g_free (text);
			return FALSE;
		}

		g_free (text);
	}

	return TRUE;
}
