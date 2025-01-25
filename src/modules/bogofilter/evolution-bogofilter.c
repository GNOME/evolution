/*
 * evolution-bogofilter.c
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
 */

#include "evolution-config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>

#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define E_TYPE_BOGOFILTER \
	(e_bogofilter_get_type ())
#define E_BOGOFILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BOGOFILTER, EBogofilter))

#define BOGOFILTER_EXIT_STATUS_SPAM		0
#define BOGOFILTER_EXIT_STATUS_HAM		1
#define BOGOFILTER_EXIT_STATUS_UNSURE		2
#define BOGOFILTER_EXIT_STATUS_ERROR		3

typedef struct _EBogofilter EBogofilter;
typedef struct _EBogofilterClass EBogofilterClass;

struct _EBogofilter {
	EMailJunkFilter parent;
	gboolean convert_to_unicode;
	gchar *command;
};

struct _EBogofilterClass {
	EMailJunkFilterClass parent_class;
};

enum {
	PROP_0,
	PROP_CONVERT_TO_UNICODE,
	PROP_COMMAND
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_bogofilter_get_type (void);
static void e_bogofilter_interface_init (CamelJunkFilterInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EBogofilter,
	e_bogofilter,
	E_TYPE_MAIL_JUNK_FILTER, 0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		CAMEL_TYPE_JUNK_FILTER,
		e_bogofilter_interface_init))

#ifndef BOGOFILTER_COMMAND
#define BOGOFILTER_COMMAND "/usr/bin/bogofilter"
#endif

static const gchar *
bogofilter_get_command_path (EBogofilter *extension)
{
	g_return_val_if_fail (extension != NULL, NULL);

	if (extension->command && *extension->command)
		return extension->command;

	return BOGOFILTER_COMMAND;
}

#ifdef G_OS_UNIX
static void
bogofilter_cancelled_cb (GCancellable *cancellable,
                         GPid *pid)
{
	/* XXX On UNIX-like systems we can safely assume a GPid is the
	 *     process ID and use it to terminate the process via signal. */
	kill (*pid, SIGTERM);
}
#endif

static void
bogofilter_exited_cb (GPid *pid,
                      gint status,
                      gpointer user_data)
{
	struct {
		GMainLoop *loop;
		gint exit_code;
	} *source_data = user_data;

	if (WIFEXITED (status))
		source_data->exit_code = WEXITSTATUS (status);
	else
		source_data->exit_code = BOGOFILTER_EXIT_STATUS_ERROR;

	g_main_loop_quit (source_data->loop);
}

static gint
bogofilter_command (const gchar **argv,
                    CamelMimeMessage *message,
                    GCancellable *cancellable,
                    GError **error)
{
	CamelStream *stream;
	GMainContext *context;
	GSource *source;
	GPid child_pid;
	gssize bytes_written;
	gint standard_input;
	gulong handler_id = 0;
	gboolean success;

	struct {
		GMainLoop *loop;
		gint exit_code;
	} source_data;

	/* Spawn Bogofilter with an open stdin pipe. */
	success = g_spawn_async_with_pipes (
		NULL,
		(gchar **) argv,
		NULL,
		G_SPAWN_DO_NOT_REAP_CHILD |
		G_SPAWN_STDOUT_TO_DEV_NULL,
		NULL, NULL,
		&child_pid,
		&standard_input,
		NULL,
		NULL,
		error);

	if (!success) {
		gchar *command_line;

		command_line = g_strjoinv (" ", (gchar **) argv);
		g_prefix_error (
			error, _("Failed to spawn Bogofilter (%s): "),
			command_line);
		g_free (command_line);

		return BOGOFILTER_EXIT_STATUS_ERROR;
	}

	/* Stream the CamelMimeMessage to Bogofilter. */
	stream = camel_stream_fs_new_with_fd (standard_input);
	bytes_written = camel_data_wrapper_write_to_stream_sync (
		CAMEL_DATA_WRAPPER (message), stream, cancellable, error);
	success = (bytes_written >= 0) &&
		(camel_stream_close (stream, cancellable, error) == 0);
	g_object_unref (stream);

	if (!success) {
		g_spawn_close_pid (child_pid);
		g_prefix_error (
			error, _("Failed to stream mail "
			"message content to Bogofilter: "));
		return BOGOFILTER_EXIT_STATUS_ERROR;
	}

	/* Wait for the Bogofilter process to terminate
	 * using GLib's main loop for better portability. */

	context = g_main_context_new ();

	source = g_child_watch_source_new (child_pid);
	g_source_set_callback (
		source, (GSourceFunc)
		bogofilter_exited_cb,
		&source_data, NULL);
	g_source_attach (source, context);
	g_source_unref (source);

	source_data.loop = g_main_loop_new (context, TRUE);
	source_data.exit_code = 0;

#ifdef G_OS_UNIX
	if (G_IS_CANCELLABLE (cancellable))
		handler_id = g_cancellable_connect (
			cancellable,
			G_CALLBACK (bogofilter_cancelled_cb),
			&child_pid, (GDestroyNotify) NULL);
#endif

	g_main_loop_run (source_data.loop);

	if (handler_id > 0)
		g_cancellable_disconnect (cancellable, handler_id);

	g_main_loop_unref (source_data.loop);
	source_data.loop = NULL;

	g_main_context_unref (context);

	/* Clean up. */

	g_spawn_close_pid (child_pid);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		source_data.exit_code = BOGOFILTER_EXIT_STATUS_ERROR;

	else if (source_data.exit_code == BOGOFILTER_EXIT_STATUS_ERROR)
		g_set_error_literal (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("Bogofilter either crashed or "
			"failed to process a mail message"));

	return source_data.exit_code;
}

static void
bogofilter_init_wordlist (EBogofilter *extension)
{
	CamelStream *stream;
	CamelMimeParser *parser;
	CamelMimeMessage *message;

	/* Initialize the Bogofilter database with a welcome message. */

	parser = camel_mime_parser_new ();
	message = camel_mime_message_new ();

	stream = camel_stream_fs_new_with_name (
		WELCOME_MESSAGE, O_RDONLY, 0, NULL);
	camel_mime_parser_init_with_stream (parser, stream, NULL);
	camel_mime_parser_scan_from (parser, FALSE);
	g_object_unref (stream);

	camel_mime_part_construct_from_parser_sync (
		CAMEL_MIME_PART (message), parser, NULL, NULL);

	camel_junk_filter_learn_not_junk (
		CAMEL_JUNK_FILTER (extension), message, NULL, NULL);

	g_object_unref (message);
	g_object_unref (parser);
}

static gboolean
bogofilter_get_convert_to_unicode (EBogofilter *extension)
{
	return extension->convert_to_unicode;
}

static void
bogofilter_set_convert_to_unicode (EBogofilter *extension,
                                   gboolean convert_to_unicode)
{
	if (extension->convert_to_unicode == convert_to_unicode)
		return;

	extension->convert_to_unicode = convert_to_unicode;

	g_object_notify (G_OBJECT (extension), "convert-to-unicode");
}

static const gchar *
bogofilter_get_command (EBogofilter *extension)
{
	return extension->command ? extension->command : "";
}

static void
bogofilter_set_command (EBogofilter *extension,
			const gchar *command)
{
	if (g_strcmp0 (extension->command, command) == 0)
		return;

	g_free (extension->command);
	extension->command = g_strdup (command);

	g_object_notify (G_OBJECT (extension), "command");
}

static void
bogofilter_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONVERT_TO_UNICODE:
			bogofilter_set_convert_to_unicode (
				E_BOGOFILTER (object),
				g_value_get_boolean (value));
			return;

		case PROP_COMMAND:
			bogofilter_set_command (
				E_BOGOFILTER (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
bogofilter_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CONVERT_TO_UNICODE:
			g_value_set_boolean (
				value, bogofilter_get_convert_to_unicode (
				E_BOGOFILTER (object)));
			return;

		case PROP_COMMAND:
			g_value_set_string (
				value, bogofilter_get_command (
				E_BOGOFILTER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
bogofilter_finalize (GObject *object)
{
	EBogofilter *extension = E_BOGOFILTER (object);

	g_free (extension->command);
	extension->command = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_bogofilter_parent_class)->finalize (object);
}

static gboolean
bogofilter_available (EMailJunkFilter *junk_filter)
{
	return g_file_test (bogofilter_get_command_path (E_BOGOFILTER (junk_filter)), G_FILE_TEST_IS_EXECUTABLE);
}

static GtkWidget *
bogofilter_new_config_widget (EMailJunkFilter *junk_filter)
{
	GtkWidget *box;
	GtkWidget *widget;
	gchar *markup;

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

	markup = g_markup_printf_escaped (
		"<b>%s</b>", _("Bogofilter Options"));
	widget = gtk_label_new (markup);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_check_button_new_with_mnemonic (
		_("Convert message text to _Unicode"));
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		junk_filter, "convert-to-unicode",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	return box;
}

static CamelJunkStatus
bogofilter_classify (CamelJunkFilter *junk_filter,
                     CamelMimeMessage *message,
                     GCancellable *cancellable,
                     GError **error)
{
	EBogofilter *extension = E_BOGOFILTER (junk_filter);
	static gboolean wordlist_initialized = FALSE;
	CamelJunkStatus status = CAMEL_JUNK_STATUS_ERROR;
	gint exit_code;

	const gchar *argv[] = {
		bogofilter_get_command_path (extension),
		NULL,  /* leave room for unicode option */
		NULL
	};

	if (bogofilter_get_convert_to_unicode (extension))
		argv[1] = "--unicode=yes";

retry:
	exit_code = bogofilter_command (argv, message, cancellable, error);

	switch (exit_code) {
		case BOGOFILTER_EXIT_STATUS_SPAM:
			status = CAMEL_JUNK_STATUS_MESSAGE_IS_JUNK;
			break;

		case BOGOFILTER_EXIT_STATUS_HAM:
			status = CAMEL_JUNK_STATUS_MESSAGE_IS_NOT_JUNK;
			break;

		case BOGOFILTER_EXIT_STATUS_UNSURE:
			status = CAMEL_JUNK_STATUS_INCONCLUSIVE;
			break;

		case BOGOFILTER_EXIT_STATUS_ERROR:
			status = CAMEL_JUNK_STATUS_ERROR;
			if (!wordlist_initialized) {
				wordlist_initialized = TRUE;
				bogofilter_init_wordlist (extension);
				goto retry;
			}
			break;

		default:
			g_warning (
				"Bogofilter: Unexpected exit code (%d) "
				"while classifying message", exit_code);
			break;
	}

	/* Check that the return value and GError agree. */
	if (status != CAMEL_JUNK_STATUS_ERROR)
		g_warn_if_fail (error == NULL || *error == NULL);
	else
		g_warn_if_fail (error == NULL || *error != NULL);

	return status;
}

static gboolean
bogofilter_learn_junk (CamelJunkFilter *junk_filter,
                       CamelMimeMessage *message,
                       GCancellable *cancellable,
                       GError **error)
{
	EBogofilter *extension = E_BOGOFILTER (junk_filter);
	gint exit_code;

	const gchar *argv[] = {
		bogofilter_get_command_path (extension),
		"--register-spam",
		NULL,  /* leave room for unicode option */
		NULL
	};

	if (bogofilter_get_convert_to_unicode (extension))
		argv[2] = "--unicode=yes";

	exit_code = bogofilter_command (argv, message, cancellable, error);

	if (exit_code != 0)
		g_warning (
			"Bogofilter: Unexpected exit code (%d) "
			"while registering spam", exit_code);

	/* Check that the return value and GError agree. */
	if (exit_code != BOGOFILTER_EXIT_STATUS_ERROR)
		g_warn_if_fail (error == NULL || *error == NULL);
	else
		g_warn_if_fail (error == NULL || *error != NULL);

	return (exit_code != BOGOFILTER_EXIT_STATUS_ERROR);
}

static gboolean
bogofilter_learn_not_junk (CamelJunkFilter *junk_filter,
                           CamelMimeMessage *message,
                           GCancellable *cancellable,
                           GError **error)
{
	EBogofilter *extension = E_BOGOFILTER (junk_filter);
	gint exit_code;

	const gchar *argv[] = {
		bogofilter_get_command_path (extension),
		"--register-ham",
		NULL,  /* leave room for unicode option */
		NULL
	};

	if (bogofilter_get_convert_to_unicode (extension))
		argv[2] = "--unicode=yes";

	exit_code = bogofilter_command (argv, message, cancellable, error);

	if (exit_code != 0)
		g_warning (
			"Bogofilter: Unexpected exit code (%d) "
			"while registering ham", exit_code);

	/* Check that the return value and GError agree. */
	if (exit_code != BOGOFILTER_EXIT_STATUS_ERROR)
		g_warn_if_fail (error == NULL || *error == NULL);
	else
		g_warn_if_fail (error == NULL || *error != NULL);

	return (exit_code != BOGOFILTER_EXIT_STATUS_ERROR);
}

static void
e_bogofilter_class_init (EBogofilterClass *class)
{
	GObjectClass *object_class;
	EMailJunkFilterClass *junk_filter_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = bogofilter_set_property;
	object_class->get_property = bogofilter_get_property;
	object_class->finalize = bogofilter_finalize;

	junk_filter_class = E_MAIL_JUNK_FILTER_CLASS (class);
	junk_filter_class->filter_name = "Bogofilter";
	junk_filter_class->display_name = _("Bogofilter");
	junk_filter_class->available = bogofilter_available;
	junk_filter_class->new_config_widget = bogofilter_new_config_widget;

	g_object_class_install_property (
		object_class,
		PROP_CONVERT_TO_UNICODE,
		g_param_spec_boolean (
			"convert-to-unicode",
			"Convert to Unicode",
			"Convert message text to Unicode",
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_COMMAND,
		g_param_spec_string (
			"command",
			"Full Path Command",
			"Full path command to use to run bogofilter",
			"",
			G_PARAM_READWRITE));
}

static void
e_bogofilter_class_finalize (EBogofilterClass *class)
{
}

static void
e_bogofilter_interface_init (CamelJunkFilterInterface *iface)
{
	iface->classify = bogofilter_classify;
	iface->learn_junk = bogofilter_learn_junk;
	iface->learn_not_junk = bogofilter_learn_not_junk;
}

static void
e_bogofilter_init (EBogofilter *extension)
{
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.bogofilter");
	g_settings_bind (
		settings, "utf8-for-spam-filter",
		G_OBJECT (extension), "convert-to-unicode",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "command",
		G_OBJECT (extension), "command",
		G_SETTINGS_BIND_DEFAULT);
	g_object_unref (settings);
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_bogofilter_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
