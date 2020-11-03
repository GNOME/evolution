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

#ifndef TEST_HTML_EDITOR_UNITS_UTILS_H
#define TEST_HTML_EDITOR_UNITS_UTILS_H

#include <glib.h>
#include <e-util/e-util.h>

G_BEGIN_DECLS

typedef struct _TestSettings {
	gchar *schema;
	gchar *key;
	GVariant *old_value;
} TestSettings;

typedef struct _TestFixture {
	GtkWidget *window;
	EHTMLEditor *editor;
	EFocusTracker *focus_tracker;
	GSList *settings; /* TestSettings * */
	guint key_state;

	GSList *undo_stack; /* UndoContent * */
} TestFixture;

#define UNICODE_NBSP "\xc2\xa0"

typedef void (* ETestFixtureSimpleFunc) (TestFixture *fixture);

void		test_utils_set_event_processing_delay_ms
						(guint value);
guint		test_utils_get_event_processing_delay_ms
						(void);
void		test_utils_set_background	(gboolean background);
gboolean	test_utils_get_background	(void);
void		test_utils_set_multiple_web_processes
						(gboolean multiple_web_processes);
gboolean	test_utils_get_multiple_web_processes
						(void);
void		test_utils_set_keep_going	(gboolean keep_going);
gboolean	test_utils_get_keep_going	(void);
void		test_utils_free_global_memory	(void);
void		test_utils_add_test		(const gchar *name,
						 ETestFixtureSimpleFunc func);
void		test_utils_fixture_set_up	(TestFixture *fixture,
						 gconstpointer user_data);
void		test_utils_fixture_tear_down	(TestFixture *fixture,
						 gconstpointer user_data);
void		test_utils_fixture_change_setting
						(TestFixture *fixture,
						 const gchar *schema,
						 const gchar *key,
						 GVariant *value);
void		test_utils_fixture_change_setting_boolean
						(TestFixture *fixture,
						 const gchar *schema,
						 const gchar *key,
						 gboolean value);
void		test_utils_fixture_change_setting_int32
						(TestFixture *fixture,
						 const gchar *schema,
						 const gchar *key,
						 gint value);
void		test_utils_fixture_change_setting_string
						(TestFixture *fixture,
						 const gchar *schema,
						 const gchar *key,
						 const gchar *value);
gpointer	test_utils_async_call_prepare	(void);
gboolean	test_utils_async_call_wait	(gpointer async_data,
						 guint timeout_seconds);
gboolean	test_utils_async_call_finish	(gpointer async_data);
gboolean	test_utils_wait_milliseconds	(guint milliseconds);
gboolean	test_utils_type_text		(TestFixture *fixture,
						 const gchar *text);
gboolean	test_utils_html_equal		(TestFixture *fixture,
						 const gchar *html1,
						 const gchar *html2);
gboolean	test_utils_process_commands	(TestFixture *fixture,
						 const gchar *commands);
gboolean	test_utils_run_simple_test	(TestFixture *fixture,
						 const gchar *commands,
						 const gchar *expected_html,
						 const gchar *expected_plain);
void		test_utils_insert_content	(TestFixture *fixture,
						 const gchar *content,
						 EContentEditorInsertContentFlags flags);
void		test_utils_set_clipboard_text	(const gchar *text,
						 gboolean is_html);
gchar *		test_utils_get_clipboard_text	(gboolean request_html);
EContentEditor *
		test_utils_get_content_editor	(TestFixture *fixture);
gchar *		test_utils_dup_image_uri	(const gchar *path);
void		test_utils_insert_signature	(TestFixture *fixture,
						 const gchar *content,
						 gboolean is_html,
						 const gchar *uid,
						 gboolean start_bottom,
						 gboolean top_signature,
						 gboolean add_delimiter);

G_END_DECLS

#endif /* TEST_HTML_EDITOR_UNITS_UTILS_H */
