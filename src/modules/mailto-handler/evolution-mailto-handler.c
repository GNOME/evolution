/*
 * evolution-mailto-handler.c
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

#include <glib/gi18n-lib.h>
#include <libebackend/libebackend.h>

#include <shell/e-shell.h>

/* Standard GObject macros */
#define E_TYPE_MAILTO_HANDLER \
	(e_mailto_handler_get_type ())
#define E_MAILTO_HANDLER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAILTO_HANDLER, EMailtoHandler))

#define MAILTO_COMMAND \
	"evolution --component=mail"

#define MAILTO_HANDLER "x-scheme-handler/mailto"

typedef struct _EMailtoHandler EMailtoHandler;
typedef struct _EMailtoHandlerClass EMailtoHandlerClass;

struct _EMailtoHandler {
	EExtension parent;
};

struct _EMailtoHandlerClass {
	EExtensionClass parent_class;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_mailto_handler_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EMailtoHandler, e_mailto_handler, E_TYPE_EXTENSION)

static EShell *
mailto_handler_get_shell (EMailtoHandler *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_SHELL (extensible);
}

static gboolean
mailto_handler_is_evolution (GAppInfo *app_info)
{
	gint argc;
	gchar **argv;
	gchar *basename;
	gboolean is_evolution;
	const gchar *mailto_command;

	if (app_info == NULL)
		return FALSE;

	mailto_command = g_app_info_get_commandline (app_info);
	if (mailto_command == NULL)
		return FALSE;

	/* Tokenize the mailto command. */
	if (!g_shell_parse_argv (mailto_command, &argc, &argv, NULL))
		return FALSE;

	g_return_val_if_fail (argc > 0, FALSE);

	/* Check the basename of the first token. */
	basename = g_path_get_basename (argv[0]);
	is_evolution = g_str_has_prefix (basename, "evolution");
	g_free (basename);

	g_strfreev (argv);

	return is_evolution;
}

static gboolean
mailto_handler_prompt (EMailtoHandler *extension)
{
	GSettings *settings;
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *widget;
	const gchar *text;
	gchar *markup;
	gint response;

	dialog = gtk_dialog_new_with_buttons (
		"", NULL, 0,
		_("Do _not change settings"), GTK_RESPONSE_NO,
		_("_Set as default email client"), GTK_RESPONSE_YES,
		NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name ("dialog-question", GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Do you want to make Evolution your default email client?");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (widget), markup);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("_Do not show this message again");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 12);
	gtk_widget_show (widget);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "prompt-check-if-default-mailer",
		widget, "active",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_SET |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	g_object_unref (settings);

	/* Direct input focus away from the checkbox. */
	widget = gtk_dialog_get_widget_for_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_YES);
	gtk_widget_grab_focus (widget);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return (response == GTK_RESPONSE_YES);
}

static void
mailto_handler_check (EMailtoHandler *extension)
{
	GSettings *settings;
	gboolean check_mailto_handler = TRUE;
	GAppInfo *app_info = NULL;
	GError *error = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	check_mailto_handler = g_settings_get_boolean (
		settings, "prompt-check-if-default-mailer");

	g_object_unref (settings);

	/* Should we check the "mailto" URI handler? */
	if (!check_mailto_handler)
		goto exit;

	app_info = g_app_info_get_default_for_type (MAILTO_HANDLER, FALSE);

	/* Is Evolution already handling "mailto" URIs? */
	if (mailto_handler_is_evolution (app_info))
		goto exit;

	/* Does the user want Evolution to handle them? */
	if (!mailto_handler_prompt (extension))
		goto exit;

	g_clear_object (&app_info);

	/* Configure Evolution to be the "mailto" URI handler. */
	app_info = g_app_info_create_from_commandline (
		MAILTO_COMMAND,
		_("Evolution"),
		G_APP_INFO_CREATE_SUPPORTS_URIS,
		&error);

	/* Sanity check. */
	g_return_if_fail (
		((app_info != NULL) && (error == NULL)) ||
		((app_info == NULL) && (error != NULL)));

	if (app_info != NULL)
		g_app_info_set_as_default_for_type (
			app_info, MAILTO_HANDLER, &error);

exit:
	g_clear_object (&app_info);

	if (error != NULL) {
		g_warning (
			"Failed to register as default handler: %s",
			error->message);
		g_error_free (error);
	}
}

static void
mailto_handler_constructed (GObject *object)
{
	EShell *shell;
	EMailtoHandler *extension;

	extension = E_MAILTO_HANDLER (object);

	shell = mailto_handler_get_shell (extension);

	g_signal_connect_swapped (
		shell, "event::ready-to-start",
		G_CALLBACK (mailto_handler_check), extension);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mailto_handler_parent_class)->constructed (object);
}

static void
e_mailto_handler_class_init (EMailtoHandlerClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mailto_handler_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_mailto_handler_class_finalize (EMailtoHandlerClass *class)
{
}

static void
e_mailto_handler_init (EMailtoHandler *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_mailto_handler_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
