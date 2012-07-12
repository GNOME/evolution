/*
 * evolution-spamassassin.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include <config.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>

#include <shell/e-shell.h>
#include <e-util/e-mktemp.h>
#include <libemail-engine/e-mail-junk-filter.h>

/* Standard GObject macros */
#define E_TYPE_SPAM_ASSASSIN \
	(e_spam_assassin_get_type ())
#define E_SPAM_ASSASSIN(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SPAM_ASSASSIN, ESpamAssassin))

#ifndef SPAMASSASSIN_BINARY
#define SPAMASSASSIN_BINARY "/usr/bin/spamassassin"
#endif

#ifndef SA_LEARN_BINARY
#define SA_LEARN_BINARY "/usr/bin/sa-learn"
#endif

#ifndef SPAMC_BINARY
#define SPAMC_BINARY "/usr/bin/spamc"
#endif

#ifndef SPAMD_BINARY
#define SPAMD_BINARY "/usr/bin/spamd"
#endif

/* For starting our own daemon. */
#define DAEMON_MAX_RETRIES 100
#define DAEMON_RETRY_DELAY 0.05  /* seconds */

#define SPAM_ASSASSIN_EXIT_STATUS_SUCCESS	0
#define SPAM_ASSASSIN_EXIT_STATUS_ERROR		-1

typedef struct _ESpamAssassin ESpamAssassin;
typedef struct _ESpamAssassinClass ESpamAssassinClass;

struct _ESpamAssassin {
	EMailJunkFilter parent;

	GMutex *socket_path_mutex;

	gchar *pid_file;
	gchar *socket_path;
	gchar *spamc_binary;
	gchar *spamd_binary;
	gint version;

	gboolean local_only;
	gboolean use_daemon;
	gboolean version_set;

	/* spamc/spamd state */
	gboolean spamd_tested;
	gboolean spamd_using_allow_tell;
	gboolean system_spamd_available;
	gboolean use_spamc;
};

struct _ESpamAssassinClass {
	EMailJunkFilterClass parent_class;
};

enum {
	PROP_0,
	PROP_LOCAL_ONLY,
	PROP_SPAMC_BINARY,
	PROP_SPAMD_BINARY,
	PROP_SOCKET_PATH,
	PROP_USE_DAEMON
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_spam_assassin_get_type (void);
static void e_spam_assassin_interface_init (CamelJunkFilterInterface *interface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	ESpamAssassin,
	e_spam_assassin,
	E_TYPE_MAIL_JUNK_FILTER, 0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		CAMEL_TYPE_JUNK_FILTER,
		e_spam_assassin_interface_init))

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
				error, _("Failed to write '%s' "
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
	if ((extension->local_only ? 1 : 0) == (local_only ? 1 : 0))
		return;

	extension->local_only = local_only;

	g_object_notify (G_OBJECT (extension), "local-only");
}

static const gchar *
spam_assassin_get_spamc_binary (ESpamAssassin *extension)
{
	return extension->spamc_binary;
}

static void
spam_assassin_set_spamc_binary (ESpamAssassin *extension,
                                const gchar *spamc_binary)
{
	if (g_strcmp0 (extension->spamc_binary, spamc_binary) == 0)
		return;

	g_free (extension->spamc_binary);
	extension->spamc_binary = g_strdup (spamc_binary);

	g_object_notify (G_OBJECT (extension), "spamc-binary");
}

static const gchar *
spam_assassin_get_spamd_binary (ESpamAssassin *extension)
{
	return extension->spamd_binary;
}

static void
spam_assassin_set_spamd_binary (ESpamAssassin *extension,
                                const gchar *spamd_binary)
{
	if (g_strcmp0 (extension->spamd_binary, spamd_binary) == 0)
		return;

	g_free (extension->spamd_binary);
	extension->spamd_binary = g_strdup (spamd_binary);

	g_object_notify (G_OBJECT (extension), "spamd-binary");
}

static const gchar *
spam_assassin_get_socket_path (ESpamAssassin *extension)
{
	return extension->socket_path;
}

static void
spam_assassin_set_socket_path (ESpamAssassin *extension,
                               const gchar *socket_path)
{
	if (g_strcmp0 (extension->socket_path, socket_path) == 0)
		return;

	g_free (extension->socket_path);
	extension->socket_path = g_strdup (socket_path);

	g_object_notify (G_OBJECT (extension), "socket-path");
}

static gboolean
spam_assassin_get_use_daemon (ESpamAssassin *extension)
{
	return extension->use_daemon;
}

static void
spam_assassin_set_use_daemon (ESpamAssassin *extension,
                              gboolean use_daemon)
{
	if ((extension->use_daemon ? 1 : 0) == (use_daemon ? 1 : 0))
		return;

	extension->use_daemon = use_daemon;

	g_object_notify (G_OBJECT (extension), "use-daemon");
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
		SA_LEARN_BINARY,
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

static void
spam_assassin_test_spamd_allow_tell (ESpamAssassin *extension)
{
	gint exit_code;
	GError *error = NULL;

	const gchar *argv[] = {
		SPAMC_BINARY,
		"--learntype=forget",
		NULL
	};

	/* Check if spamd is running with --allow-tell. */

	exit_code = spam_assassin_command (argv, NULL, "\n", NULL, &error);
	extension->spamd_using_allow_tell = (exit_code == 0);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static gboolean
spam_assassin_test_spamd_running (ESpamAssassin *extension,
                                  gboolean system_spamd)
{
	const gchar *argv[5];
	gint exit_code;
	gint ii = 0;
	GError *error = NULL;

	g_mutex_lock (extension->socket_path_mutex);

	argv[ii++] = extension->spamc_binary;
	argv[ii++] = "--no-safe-fallback";
	if (!system_spamd) {
		argv[ii++] = "--socket";
		argv[ii++] = extension->socket_path;
	}
	argv[ii] = NULL;

	g_assert (ii < G_N_ELEMENTS (argv));

	exit_code = spam_assassin_command (
		argv, NULL, "From test@127.0.0.1", NULL, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_mutex_unlock (extension->socket_path_mutex);

	return (exit_code == 0);
}

static void
spam_assassin_kill_our_own_daemon (ESpamAssassin *extension)
{
	gint pid;
	gchar *contents = NULL;
	GError *error = NULL;

	g_mutex_lock (extension->socket_path_mutex);

	g_free (extension->socket_path);
	extension->socket_path = NULL;

	g_mutex_unlock (extension->socket_path_mutex);

	if (extension->pid_file == NULL)
		return;

	g_file_get_contents (extension->pid_file, &contents, NULL, &error);

	if (error != NULL) {
		g_warn_if_fail (contents == NULL);
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	g_return_if_fail (contents != NULL);

	pid = atoi (contents);
	g_free (contents);

	if (pid > 0 && kill (pid, SIGTERM) == 0)
		waitpid (pid, NULL, 0);
}

static void
spam_assassin_prepare_for_quit (EShell *shell,
                                EActivity *activity,
                                ESpamAssassin *extension)
{
	spam_assassin_kill_our_own_daemon (extension);
}

static gboolean
spam_assassin_start_our_own_daemon (ESpamAssassin *extension)
{
	const gchar *argv[8];
	const gchar *user_runtime_dir;
	gchar *pid_file;
	gchar *socket_path;
	gboolean started = FALSE;
	gint exit_code;
	gint ii = 0;
	gint fd;
	GError *error = NULL;

	g_mutex_lock (extension->socket_path_mutex);

	/* Don't put the PID files in Evolution's tmp directory
	 * (as defined in e-mktemp.c) because that gets cleaned
	 * every few hours, and these files need to persist. */
	user_runtime_dir = g_get_user_runtime_dir ();

	pid_file = g_build_filename (
		user_runtime_dir, "spamd-pid-file-XXXXXX", NULL);

	socket_path = g_build_filename (
		user_runtime_dir, "spamd-socket-path-XXXXXX", NULL);

	/* The template filename is modified in place. */
	fd = g_mkstemp (pid_file);
	if (fd >= 0) {
		close (fd);
		g_unlink (pid_file);
	} else {
		g_warning (
			"Failed to create spamd-pid-file: %s",
			g_strerror (errno));
		goto exit;
	}

	/* The template filename is modified in place. */
	fd = g_mkstemp (socket_path);
	if (fd >= 0) {
		close (fd);
		g_unlink (socket_path);
	} else {
		g_warning (
			"Failed to create spamd-socket-path: %s",
			g_strerror (errno));
		goto exit;
	}

	argv[ii++] = extension->spamd_binary;
	argv[ii++] = "--socketpath";
	argv[ii++] = socket_path;

	if (spam_assassin_get_local_only (extension))
		argv[ii++] = "--local";

	argv[ii++] = "--max-children=1";
	argv[ii++] = "--pidfile";
	argv[ii++] = pid_file;
	argv[ii] = NULL;

	g_assert (ii < G_N_ELEMENTS (argv));

	exit_code = spam_assassin_command_full (
		argv, NULL, NULL, NULL, FALSE, NULL, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto exit;
	}

	if (exit_code == SPAM_ASSASSIN_EXIT_STATUS_SUCCESS) {
		/* Wait for the socket path to appear. */
		for (ii = 0; ii < DAEMON_MAX_RETRIES; ii++) {
			if (g_file_test (socket_path, G_FILE_TEST_EXISTS)) {
				started = TRUE;
				break;
			}
			g_usleep (DAEMON_RETRY_DELAY * G_USEC_PER_SEC);
		}
	}

	/* Set these directly to avoid emitting "notify" signals. */
	if (started) {
		g_free (extension->pid_file);
		extension->pid_file = pid_file;
		pid_file = NULL;

		g_free (extension->socket_path);
		extension->socket_path = socket_path;
		socket_path = NULL;

		/* XXX EMailSession is too prone to reference leaks to leave
		 *     this for our finalize() method.  We want to be sure to
		 *     kill the spamd process we started when Evolution shuts
		 *     down, so connect to an EShell signal instead. */
		g_signal_connect (
			e_shell_get_default (), "prepare-for-quit",
			G_CALLBACK (spam_assassin_prepare_for_quit),
			extension);
	}

exit:
	g_free (pid_file);
	g_free (socket_path);

	g_mutex_unlock (extension->socket_path_mutex);

	return started;
}

static void
spam_assassin_test_spamd (ESpamAssassin *extension)
{
	const gchar *spamd_binary;
	gboolean try_system_spamd;

	/* XXX SpamAssassin could really benefit from a D-Bus interface
	 *     these days.  These tests are just needlessly painful for
	 *     clients trying to talk to an already-running spamd. */

	extension->use_spamc = FALSE;
	spamd_binary = extension->spamd_binary;
	try_system_spamd = (g_strcmp0 (spamd_binary, SPAMD_BINARY) == 0);

	if (extension->local_only && try_system_spamd) {
		gint exit_code;

		/* Run a shell command to check for a running
		 * spamd process with a -L/--local option or a
		 * -p/--port option. */

		const gchar *argv[] = {
			"/bin/sh",
			"-c",
			"ps ax | grep -v grep | "
			"grep -E 'spamd.*(\\-L|\\-\\-local)' | "
			"grep -E -v '\\ \\-p\\ |\\ \\-\\-port\\ '",
			NULL
		};

		exit_code = spam_assassin_command (
			argv, NULL, NULL, NULL, NULL);
		try_system_spamd = (exit_code == 0);
	}

	/* Try to use the system spamd first. */
	if (try_system_spamd) {
		if (spam_assassin_test_spamd_running (extension, TRUE)) {
			extension->use_spamc = TRUE;
			extension->system_spamd_available = TRUE;
		}
	}

	/* If there's no system spamd running, try
	 * to use one with a user specified socket. */
	if (!extension->use_spamc && extension->socket_path != NULL) {
		if (spam_assassin_test_spamd_running (extension, FALSE)) {
			extension->use_spamc = TRUE;
			extension->system_spamd_available = FALSE;
		}
	}

	/* Still unsuccessful?  Try to start our own spamd. */
	if (!extension->use_spamc) {
		extension->use_spamc =
			spam_assassin_start_our_own_daemon (extension) &&
			spam_assassin_test_spamd_running (extension, FALSE);
	}
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

		case PROP_SPAMC_BINARY:
			spam_assassin_set_spamc_binary (
				E_SPAM_ASSASSIN (object),
				g_value_get_string (value));
			return;

		case PROP_SPAMD_BINARY:
			spam_assassin_set_spamd_binary (
				E_SPAM_ASSASSIN (object),
				g_value_get_string (value));
			return;

		case PROP_SOCKET_PATH:
			spam_assassin_set_socket_path (
				E_SPAM_ASSASSIN (object),
				g_value_get_string (value));
			return;

		case PROP_USE_DAEMON:
			spam_assassin_set_use_daemon (
				E_SPAM_ASSASSIN (object),
				g_value_get_boolean (value));
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

		case PROP_SPAMC_BINARY:
			g_value_set_string (
				value, spam_assassin_get_spamc_binary (
				E_SPAM_ASSASSIN (object)));
			return;

		case PROP_SPAMD_BINARY:
			g_value_set_string (
				value, spam_assassin_get_spamd_binary (
				E_SPAM_ASSASSIN (object)));
			return;

		case PROP_SOCKET_PATH:
			g_value_set_string (
				value, spam_assassin_get_socket_path (
				E_SPAM_ASSASSIN (object)));
			return;

		case PROP_USE_DAEMON:
			g_value_set_boolean (
				value, spam_assassin_get_use_daemon (
				E_SPAM_ASSASSIN (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
spam_assassin_finalize (GObject *object)
{
	ESpamAssassin *extension = E_SPAM_ASSASSIN (object);

	g_mutex_free (extension->socket_path_mutex);

	g_free (extension->pid_file);
	g_free (extension->socket_path);
	g_free (extension->spamc_binary);
	g_free (extension->spamd_binary);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_spam_assassin_parent_class)->finalize (object);
}

static gboolean
spam_assassin_available (EMailJunkFilter *junk_filter)
{
	ESpamAssassin *extension = E_SPAM_ASSASSIN (junk_filter);
	gboolean available;
	GError *error = NULL;

	available = spam_assassin_get_version (extension, NULL, NULL, &error);

	/* XXX These tests block like crazy so maybe this isn't the best
	 *     place to be doing this, but the first available() call is
	 *     done at startup before the UI is shown.  So hopefully the
	 *     delay will not be noticeable. */
	if (available && extension->use_daemon && !extension->spamd_tested) {
		extension->spamd_tested = TRUE;
		spam_assassin_test_spamd (extension);
		spam_assassin_test_spamd_allow_tell (extension);
	}

	if (error != NULL) {
		g_warning ("%s", error->message);
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
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
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
	gtk_widget_set_margin_left (widget, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_object_bind_property (
		junk_filter, "local-only",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	markup = g_markup_printf_escaped (
		"<small>%s</small>",
		_("This will make SpamAssassin more reliable, but slower."));
	widget = gtk_label_new (markup);
	gtk_widget_set_margin_left (widget, 36);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	return box;
}

static gboolean
spam_assassin_classify (CamelJunkFilter *junk_filter,
                        CamelMimeMessage *message,
                        CamelJunkStatus *status,
                        GCancellable *cancellable,
                        GError **error)
{
	ESpamAssassin *extension = E_SPAM_ASSASSIN (junk_filter);
	const gchar *argv[7];
	gboolean using_spamc;
	gint exit_code;
	gint ii = 0;

	g_mutex_lock (extension->socket_path_mutex);

	using_spamc = (extension->use_spamc && extension->use_daemon);

	if (using_spamc) {
		argv[ii++] = extension->spamc_binary;
		argv[ii++] = "--check";
		argv[ii++] = "--timeout=60";
		if (!extension->system_spamd_available) {
			argv[ii++] = "--socket";
			argv[ii++] = extension->socket_path;
		}
	} else {
		argv[ii++] = SPAMASSASSIN_BINARY;
		argv[ii++] = "--exit-code";
		if (extension->local_only)
			argv[ii++] = "--local";
	}
	argv[ii] = NULL;

	g_assert (ii < G_N_ELEMENTS (argv));

	exit_code = spam_assassin_command (
		argv, message, NULL, cancellable, error);

	/* For either program, exit code 0 means the message is ham. */
	if (exit_code == 0)
		*status = CAMEL_JUNK_STATUS_MESSAGE_IS_NOT_JUNK;

	/* spamassassin(1) only specifies zero and non-zero exit codes. */
	else if (!using_spamc)
		*status = CAMEL_JUNK_STATUS_MESSAGE_IS_JUNK;

	/* Whereas spamc(1) explicitly states exit code 1 means spam. */
	else if (exit_code == 1)
		*status = CAMEL_JUNK_STATUS_MESSAGE_IS_JUNK;

	/* Consider any other spamc(1) exit code to be inconclusive
	 * since it most likely failed to process the message. */
	else
		*status = CAMEL_JUNK_STATUS_INCONCLUSIVE;

	/* Check that the return value and GError agree. */
	if (exit_code != SPAM_ASSASSIN_EXIT_STATUS_ERROR)
		g_warn_if_fail (error == NULL || *error == NULL);
	else
		g_warn_if_fail (error == NULL || *error != NULL);

	g_mutex_unlock (extension->socket_path_mutex);

	return (exit_code != SPAM_ASSASSIN_EXIT_STATUS_ERROR);
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

	if (extension->spamd_using_allow_tell) {
		argv[ii++] = extension->spamc_binary;
		argv[ii++] = "--learntype=spam";
	} else {
		argv[ii++] = SA_LEARN_BINARY;
		argv[ii++] = "--spam";
		if (extension->version >= 3)
			argv[ii++] = "--no-sync";
		else
			argv[ii++] = "--no-rebuild";
		if (extension->local_only)
			argv[ii++] = "--local";
	}
	argv[ii] = NULL;

	g_assert (ii < G_N_ELEMENTS (argv));

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

	if (extension->spamd_using_allow_tell) {
		argv[ii++] = extension->spamc_binary;
		argv[ii++] = "--learntype=ham";
	} else {
		argv[ii++] = SA_LEARN_BINARY;
		argv[ii++] = "--ham";
		if (extension->version >= 3)
			argv[ii++] = "--no-sync";
		else
			argv[ii++] = "--no-rebuild";
		if (extension->local_only)
			argv[ii++] = "--local";
	}
	argv[ii] = NULL;

	g_assert (ii < G_N_ELEMENTS (argv));

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

	/* If we're using a spamd that allows learning,
	 * there's no need to synchronize anything. */
	if (extension->spamd_using_allow_tell)
		return TRUE;

	argv[ii++] = SA_LEARN_BINARY;
	if (extension->version >= 3)
		argv[ii++] = "--sync";
	else
		argv[ii++] = "--rebuild";
	if (extension->local_only)
		argv[ii++] = "--local";
	argv[ii] = NULL;

	g_assert (ii < G_N_ELEMENTS (argv));

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
		PROP_SPAMC_BINARY,
		g_param_spec_string (
			"spamc-binary",
			"spamc Binary",
			"File path for the spamc binary",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SPAMD_BINARY,
		g_param_spec_string (
			"spamd-binary",
			"spamd Binary",
			"File path for the spamd binary",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SOCKET_PATH,
		g_param_spec_string (
			"socket-path",
			"Socket Path",
			"Socket path for a SpamAssassin daemon",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_USE_DAEMON,
		g_param_spec_boolean (
			"use-daemon",
			"Use Daemon",
			"Whether to use a SpamAssassin daemon",
			FALSE,
			G_PARAM_READWRITE));
}

static void
e_spam_assassin_class_finalize (ESpamAssassinClass *class)
{
}

static void
e_spam_assassin_interface_init (CamelJunkFilterInterface *interface)
{
	interface->classify = spam_assassin_classify;
	interface->learn_junk = spam_assassin_learn_junk;
	interface->learn_not_junk = spam_assassin_learn_not_junk;
	interface->synchronize = spam_assassin_synchronize;
}

static void
e_spam_assassin_init (ESpamAssassin *extension)
{
	GSettings *settings;

	extension->socket_path_mutex = g_mutex_new ();

	settings = g_settings_new ("org.gnome.evolution.spamassassin");

	g_settings_bind (
		settings, "local-only",
		G_OBJECT (extension), "local-only",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "spamc-binary",
		G_OBJECT (extension), "spamc-binary",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "spamd-binary",
		G_OBJECT (extension), "spamd-binary",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "socket-path",
		G_OBJECT (extension), "socket-path",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "use-daemon",
		G_OBJECT (extension), "use-daemon",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	if (extension->spamc_binary == NULL)
		extension->spamc_binary = g_strdup (SPAMC_BINARY);

	if (extension->spamd_binary == NULL)
		extension->spamd_binary = g_strdup (SPAMD_BINARY);
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
