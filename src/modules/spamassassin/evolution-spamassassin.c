/*
 * evolution-spamassassin.c
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

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>

#include <shell/e-shell.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define E_TYPE_SPAM_ASSASSIN \
	(e_spam_assassin_get_type ())
#define E_SPAM_ASSASSIN(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SPAM_ASSASSIN, ESpamAssassin))

#define SPAM_ASSASSIN_EXIT_STATUS_SUCCESS	0
#define SPAM_ASSASSIN_EXIT_STATUS_ERROR		-1

typedef struct _ESpamAssassin ESpamAssassin;
typedef struct _ESpamAssassinClass ESpamAssassinClass;

struct _ESpamAssassin {
	EMailJunkFilter parent;

	gboolean local_only;
	gchar *command;
	gchar *learn_command;

	gboolean version_set;
	gint version;
};

struct _ESpamAssassinClass {
	EMailJunkFilterClass parent_class;
};

enum {
	PROP_0,
	PROP_LOCAL_ONLY,
	PROP_COMMAND,
	PROP_LEARN_COMMAND
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_spam_assassin_get_type (void);
static void e_spam_assassin_interface_init (CamelJunkFilterInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	ESpamAssassin,
	e_spam_assassin,
	E_TYPE_MAIL_JUNK_FILTER, 0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		CAMEL_TYPE_JUNK_FILTER,
		e_spam_assassin_interface_init))

#ifndef SPAMASSASSIN_COMMAND
#define SPAMASSASSIN_COMMAND "/usr/bin/spamassassin"
#endif

#ifndef SA_LEARN_COMMAND
#define SA_LEARN_COMMAND "/usr/bin/sa-learn"
#endif

static const gchar *
spam_assassin_get_command_path (ESpamAssassin *extension)
{
	g_return_val_if_fail (extension != NULL, NULL);

	if (extension->command && *extension->command)
		return extension->command;

	return SPAMASSASSIN_COMMAND;
}

static const gchar *
spam_assassin_get_learn_command_path (ESpamAssassin *extension)
{
	g_return_val_if_fail (extension != NULL, NULL);

	if (extension->learn_command && *extension->learn_command)
		return extension->learn_command;

	return SA_LEARN_COMMAND;
}

#ifdef G_OS_UNIX
static void
spam_assassin_cancelled_cb (GCancellable *cancellable,
                            GPid *pid)
{
	/* XXX On UNIX-like systems we can safely assume a GPid is the
	 *     process ID and use it to terminate the process via signal. */
	kill (*pid, SIGTERM);
}
#endif

static void
spam_assassin_exited_cb (GPid *pid,
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
		source_data->exit_code = SPAM_ASSASSIN_EXIT_STATUS_ERROR;

	g_main_loop_quit (source_data->loop);
}

static gint
spam_assassin_command_full (const gchar **argv,
                            CamelMimeMessage *message,
                            const gchar *input_data,
                            GByteArray *output_buffer,
                            gboolean wait_for_termination,
                            GCancellable *cancellable,
                            GError **error)
{
	GMainContext *context;
	GSpawnFlags flags = 0;
	GSource *source;
	GPid child_pid;
	gint standard_input;
	gint standard_output;
	gulong handler_id = 0;
	gboolean success;

	struct {
		GMainLoop *loop;
		gint exit_code;
	} source_data;

	if (wait_for_termination)
		flags |= G_SPAWN_DO_NOT_REAP_CHILD;
	if (output_buffer == NULL)
		flags |= G_SPAWN_STDOUT_TO_DEV_NULL;
	flags |= G_SPAWN_STDERR_TO_DEV_NULL;

	/* Spawn SpamAssassin with an open stdin pipe. */
	success = g_spawn_async_with_pipes (
		NULL,
		(gchar **) argv,
		NULL,
		flags,
		NULL, NULL,
		&child_pid,
		&standard_input,
		(output_buffer != NULL) ? &standard_output : NULL,
		NULL,
		error);

	if (!success) {
		gchar *command_line;

		command_line = g_strjoinv (" ", (gchar **) argv);
		g_prefix_error (
			error, _("Failed to spawn SpamAssassin (%s): "),
			command_line);
		g_free (command_line);

		return SPAM_ASSASSIN_EXIT_STATUS_ERROR;
	}

	if (message != NULL) {
		CamelStream *stream;
		gssize bytes_written;

		/* Stream the CamelMimeMessage to SpamAssassin. */
		stream = camel_stream_fs_new_with_fd (standard_input);
		bytes_written = camel_data_wrapper_write_to_stream_sync (
			CAMEL_DATA_WRAPPER (message),
			stream, cancellable, error);
		success = (bytes_written >= 0) &&
			(camel_stream_close (stream, cancellable, error) == 0);
		g_object_unref (stream);

		if (!success) {
			g_spawn_close_pid (child_pid);
			g_prefix_error (
				error, _("Failed to stream mail "
				"message content to SpamAssassin: "));
			return SPAM_ASSASSIN_EXIT_STATUS_ERROR;
		}

	} else if (input_data != NULL) {
		gssize bytes_written;

		/* Write raw data directly to SpamAssassin. */
		bytes_written = camel_write (
			standard_input, input_data,
			strlen (input_data), cancellable, error);
		success = (bytes_written >= 0);

		close (standard_input);

		if (!success) {
			g_spawn_close_pid (child_pid);
			g_prefix_error (
				error, _("Failed to write “%s” "
				"to SpamAssassin: "), input_data);
			return SPAM_ASSASSIN_EXIT_STATUS_ERROR;
		}
	}

	if (output_buffer != NULL) {
		CamelStream *input_stream;
		CamelStream *output_stream;
		gssize bytes_written;

		input_stream = camel_stream_fs_new_with_fd (standard_output);

		output_stream = camel_stream_mem_new ();
		camel_stream_mem_set_byte_array (
			CAMEL_STREAM_MEM (output_stream), output_buffer);

		bytes_written = camel_stream_write_to_stream (
			input_stream, output_stream, cancellable, error);
		g_byte_array_append (output_buffer, (guint8 *) "", 1);
		success = (bytes_written >= 0);

		g_object_unref (input_stream);
		g_object_unref (output_stream);

		if (!success) {
			g_spawn_close_pid (child_pid);
			g_prefix_error (
				error, _("Failed to read "
				"output from SpamAssassin: "));
			return SPAM_ASSASSIN_EXIT_STATUS_ERROR;
		}
	}

	/* XXX I'm not sure if we should call g_spawn_close_pid()
	 *     here or not.  Only really matters on Windows anyway. */
	if (!wait_for_termination)
		return 0;

	/* Wait for the SpamAssassin process to terminate
	 * using GLib's main loop for better portability. */

	context = g_main_context_new ();

	source = g_child_watch_source_new (child_pid);
	g_source_set_callback (
		source, (GSourceFunc)
		spam_assassin_exited_cb,
		&source_data, NULL);
	g_source_attach (source, context);
	g_source_unref (source);

	source_data.loop = g_main_loop_new (context, TRUE);
	source_data.exit_code = 0;

#ifdef G_OS_UNIX
	if (G_IS_CANCELLABLE (cancellable))
		handler_id = g_cancellable_connect (
			cancellable,
			G_CALLBACK (spam_assassin_cancelled_cb),
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
		source_data.exit_code = SPAM_ASSASSIN_EXIT_STATUS_ERROR;

	else if (source_data.exit_code == SPAM_ASSASSIN_EXIT_STATUS_ERROR)
		g_set_error_literal (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("SpamAssassin either crashed or "
			"failed to process a mail message"));

	return source_data.exit_code;
}

static gint
spam_assassin_command (const gchar **argv,
                       CamelMimeMessage *message,
                       const gchar *input_data,
                       GCancellable *cancellable,
                       GError **error)
{
	return spam_assassin_command_full (
		argv, message, input_data, NULL, TRUE, cancellable, error);
}

static gboolean
spam_assassin_get_local_only (ESpamAssassin *extension)
{
	return extension->local_only;
}

static void
spam_assassin_set_local_only (ESpamAssassin *extension,
                              gboolean local_only)
{
	if (extension->local_only == local_only)
		return;

	extension->local_only = local_only;

	g_object_notify (G_OBJECT (extension), "local-only");
}

static const gchar *
spam_assassin_get_command (ESpamAssassin *extension)
{
	return extension->command;
}

static void
spam_assassin_set_command (ESpamAssassin *extension,
			   const gchar *command)
{
	if (g_strcmp0 (extension->command, command) == 0)
		return;

	g_free (extension->command);
	extension->command = g_strdup (command);

	g_object_notify (G_OBJECT (extension), "command");
}

static const gchar *
spam_assassin_get_learn_command (ESpamAssassin *extension)
{
	return extension->learn_command;
}

static void
spam_assassin_set_learn_command (ESpamAssassin *extension,
				 const gchar *learn_command)
{
	if (g_strcmp0 (extension->learn_command, learn_command) == 0)
		return;

	g_free (extension->learn_command);
	extension->learn_command = g_strdup (learn_command);

	g_object_notify (G_OBJECT (extension), "learn-command");
}

static void
spam_assassin_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_LOCAL_ONLY:
			spam_assassin_set_local_only (
				E_SPAM_ASSASSIN (object),
				g_value_get_boolean (value));
			return;

		case PROP_COMMAND:
			spam_assassin_set_command (
				E_SPAM_ASSASSIN (object),
				g_value_get_string (value));
			return;

		case PROP_LEARN_COMMAND:
			spam_assassin_set_learn_command (
				E_SPAM_ASSASSIN (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spam_assassin_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_LOCAL_ONLY:
			g_value_set_boolean (
				value, spam_assassin_get_local_only (
				E_SPAM_ASSASSIN (object)));
			return;

		case PROP_COMMAND:
			g_value_set_string (
				value, spam_assassin_get_command (
				E_SPAM_ASSASSIN (object)));
			return;

		case PROP_LEARN_COMMAND:
			g_value_set_string (
				value, spam_assassin_get_learn_command (
				E_SPAM_ASSASSIN (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spam_assassin_finalize (GObject *object)
{
	ESpamAssassin *extension = E_SPAM_ASSASSIN (object);

	g_free (extension->command);
	extension->command = NULL;

	g_free (extension->learn_command);
	extension->learn_command = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_spam_assassin_parent_class)->finalize (object);
}

static gboolean
spam_assassin_get_version (ESpamAssassin *extension,
                           gint *spam_assassin_version,
                           GCancellable *cancellable,
                           GError **error)
{
	GByteArray *output_buffer;
	gint exit_code;
	guint ii;

	const gchar *argv[] = {
		spam_assassin_get_learn_command_path (extension),
		"--version",
		NULL
	};

	if (extension->version_set) {
		if (spam_assassin_version != NULL)
			*spam_assassin_version = extension->version;
		return TRUE;
	}

	output_buffer = g_byte_array_new ();

	exit_code = spam_assassin_command_full (
		argv, NULL, NULL, output_buffer, TRUE, cancellable, error);

	if (exit_code != 0) {
		g_byte_array_free (output_buffer, TRUE);
		return FALSE;
	}

	for (ii = 0; ii < output_buffer->len; ii++) {
		if (g_ascii_isdigit (output_buffer->data[ii])) {
			guint8 ch = output_buffer->data[ii];
			extension->version = (ch - '0');
			extension->version_set = TRUE;
			break;
		}
	}

	if (spam_assassin_version != NULL)
		*spam_assassin_version = extension->version;

	g_byte_array_free (output_buffer, TRUE);

	return TRUE;
}

static gboolean
spam_assassin_available (EMailJunkFilter *junk_filter)
{
	ESpamAssassin *extension = E_SPAM_ASSASSIN (junk_filter);
	gboolean available;
	GError *error = NULL;

	available = spam_assassin_get_version (extension, NULL, NULL, &error);

	if (error != NULL) {
		g_debug ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	return available;
}

static GtkWidget *
spam_assassin_new_config_widget (EMailJunkFilter *junk_filter)
{
	GtkWidget *box;
	GtkWidget *widget;
	GtkWidget *container;
	gchar *markup;

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

	markup = g_markup_printf_escaped (
		"<b>%s</b>", _("SpamAssassin Options"));
	widget = gtk_label_new (markup);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_check_button_new_with_mnemonic (
		_("I_nclude remote tests"));
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		junk_filter, "local-only",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	markup = g_markup_printf_escaped (
		"<small>%s</small>",
		_("This will make SpamAssassin more reliable, but slower."));
	widget = gtk_label_new (markup);
	gtk_widget_set_margin_start (widget, 36);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	return box;
}

static CamelJunkStatus
spam_assassin_classify (CamelJunkFilter *junk_filter,
                        CamelMimeMessage *message,
                        GCancellable *cancellable,
                        GError **error)
{
	ESpamAssassin *extension = E_SPAM_ASSASSIN (junk_filter);
	CamelJunkStatus status;
	const gchar *argv[7];
	gint exit_code;
	gint ii = 0;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return CAMEL_JUNK_STATUS_ERROR;

	argv[ii++] = spam_assassin_get_command_path (extension);
	argv[ii++] = "--exit-code";
	if (extension->local_only)
		argv[ii++] = "--local";
	argv[ii] = NULL;

	g_return_val_if_fail (ii < G_N_ELEMENTS (argv), CAMEL_JUNK_STATUS_ERROR);

	exit_code = spam_assassin_command (
		argv, message, NULL, cancellable, error);

	/* Check for an error while spawning the program. */
	if (exit_code == SPAM_ASSASSIN_EXIT_STATUS_ERROR)
		status = CAMEL_JUNK_STATUS_ERROR;

	/* Zero exit code means the message is ham. */
	else if (exit_code == 0)
		status = CAMEL_JUNK_STATUS_MESSAGE_IS_NOT_JUNK;

	/* Non-zero exit code means the message is spam. */
	else
		status = CAMEL_JUNK_STATUS_MESSAGE_IS_JUNK;

	/* Check that the return value and GError agree. */
	if (status != CAMEL_JUNK_STATUS_ERROR)
		g_warn_if_fail (error == NULL || *error == NULL);
	else
		g_warn_if_fail (error == NULL || *error != NULL);

	return status;
}

static gboolean
spam_assassin_learn_junk (CamelJunkFilter *junk_filter,
                          CamelMimeMessage *message,
                          GCancellable *cancellable,
                          GError **error)
{
	ESpamAssassin *extension = E_SPAM_ASSASSIN (junk_filter);
	const gchar *argv[5];
	gint exit_code;
	gint ii = 0;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	argv[ii++] = spam_assassin_get_learn_command_path (extension);
	argv[ii++] = "--spam";
	argv[ii++] = "--no-sync";
	if (extension->local_only)
		argv[ii++] = "--local";
	argv[ii] = NULL;

	g_return_val_if_fail (ii < G_N_ELEMENTS (argv), FALSE);

	exit_code = spam_assassin_command (
		argv, message, NULL, cancellable, error);

	/* Check that the return value and GError agree. */
	if (exit_code == SPAM_ASSASSIN_EXIT_STATUS_SUCCESS)
		g_warn_if_fail (error == NULL || *error == NULL);
	else
		g_warn_if_fail (error == NULL || *error != NULL);

	return (exit_code == SPAM_ASSASSIN_EXIT_STATUS_SUCCESS);
}

static gboolean
spam_assassin_learn_not_junk (CamelJunkFilter *junk_filter,
                              CamelMimeMessage *message,
                              GCancellable *cancellable,
                              GError **error)
{
	ESpamAssassin *extension = E_SPAM_ASSASSIN (junk_filter);
	const gchar *argv[5];
	gint exit_code;
	gint ii = 0;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	argv[ii++] = spam_assassin_get_learn_command_path (extension);
	argv[ii++] = "--ham";
	argv[ii++] = "--no-sync";
	if (extension->local_only)
		argv[ii++] = "--local";
	argv[ii] = NULL;

	g_return_val_if_fail (ii < G_N_ELEMENTS (argv), FALSE);

	exit_code = spam_assassin_command (
		argv, message, NULL, cancellable, error);

	/* Check that the return value and GError agree. */
	if (exit_code == SPAM_ASSASSIN_EXIT_STATUS_SUCCESS)
		g_warn_if_fail (error == NULL || *error == NULL);
	else
		g_warn_if_fail (error == NULL || *error != NULL);

	return (exit_code == SPAM_ASSASSIN_EXIT_STATUS_SUCCESS);
}

static gboolean
spam_assassin_synchronize (CamelJunkFilter *junk_filter,
                           GCancellable *cancellable,
                           GError **error)
{
	ESpamAssassin *extension = E_SPAM_ASSASSIN (junk_filter);
	const gchar *argv[4];
	gint exit_code;
	gint ii = 0;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	argv[ii++] = spam_assassin_get_learn_command_path (extension);
	argv[ii++] = "--sync";
	if (extension->local_only)
		argv[ii++] = "--local";
	argv[ii] = NULL;

	g_return_val_if_fail (ii < G_N_ELEMENTS (argv), FALSE);

	exit_code = spam_assassin_command (
		argv, NULL, NULL, cancellable, error);

	/* Check that the return value and GError agree. */
	if (exit_code == SPAM_ASSASSIN_EXIT_STATUS_SUCCESS)
		g_warn_if_fail (error == NULL || *error == NULL);
	else
		g_warn_if_fail (error == NULL || *error != NULL);

	return (exit_code == SPAM_ASSASSIN_EXIT_STATUS_SUCCESS);
}

static void
e_spam_assassin_class_init (ESpamAssassinClass *class)
{
	GObjectClass *object_class;
	EMailJunkFilterClass *junk_filter_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = spam_assassin_set_property;
	object_class->get_property = spam_assassin_get_property;
	object_class->finalize = spam_assassin_finalize;

	junk_filter_class = E_MAIL_JUNK_FILTER_CLASS (class);
	junk_filter_class->filter_name = "SpamAssassin";
	junk_filter_class->display_name = _("SpamAssassin");
	junk_filter_class->available = spam_assassin_available;
	junk_filter_class->new_config_widget = spam_assassin_new_config_widget;

	g_object_class_install_property (
		object_class,
		PROP_LOCAL_ONLY,
		g_param_spec_boolean (
			"local-only",
			"Local Only",
			"Do not use tests requiring DNS lookups",
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_COMMAND,
		g_param_spec_string (
			"command",
			"Full Path Command",
			"Full path command to use to run spamassassin",
			"",
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_LEARN_COMMAND,
		g_param_spec_string (
			"learn-command",
			"Full Path Command",
			"Full path command to use to run sa-learn",
			"",
			G_PARAM_READWRITE));
}

static void
e_spam_assassin_class_finalize (ESpamAssassinClass *class)
{
}

static void
e_spam_assassin_interface_init (CamelJunkFilterInterface *iface)
{
	iface->classify = spam_assassin_classify;
	iface->learn_junk = spam_assassin_learn_junk;
	iface->learn_not_junk = spam_assassin_learn_not_junk;
	iface->synchronize = spam_assassin_synchronize;
}

static void
e_spam_assassin_init (ESpamAssassin *extension)
{
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.spamassassin");

	g_settings_bind (
		settings, "local-only",
		extension, "local-only",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "command",
		G_OBJECT (extension), "command",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "learn-command",
		G_OBJECT (extension), "learn-command",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_spam_assassin_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
