/*
 * evolution-mailto-handler.c
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
#include <glib/gi18n-lib.h>

#include <shell/e-shell.h>
#include <e-util/e-binding.h>
#include <e-util/e-extension.h>

/* Standard GObject macros */
#define E_TYPE_MAILTO_HANDLER \
	(e_mailto_handler_get_type ())
#define E_MAILTO_HANDLER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAILTO_HANDLER, EMailtoHandler))

#define MAILTO_COMMAND \
	"evolution --component=mail %s"

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
mailto_handler_is_evolution (const gchar *mailto_command)
{
	gint argc;
	gchar **argv;
	gchar *basename;
	gboolean is_evolution;

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
	EShell *shell;
	EShellSettings *shell_settings;
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *widget;
	const gchar *text;
	gchar *markup;
	gint response;

	shell = mailto_handler_get_shell (extension);
	shell_settings = e_shell_get_shell_settings (shell);

	dialog = gtk_dialog_new_with_buttons (
		"", NULL, 0,
		GTK_STOCK_NO, GTK_RESPONSE_NO,
		GTK_STOCK_YES, GTK_RESPONSE_YES,
		NULL);

#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (dialog, "has-separator", FALSE, NULL);
#endif
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_stock (
		GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_vbox_new (FALSE, 12);
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

	e_mutual_binding_new_with_negation (
		shell_settings, "mailto-handler-check",
		widget, "active");

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
	EShell *shell;
	EShellSettings *shell_settings;
	gboolean check_mailto_handler = TRUE;
	gchar *mailto_command = NULL;

	shell = mailto_handler_get_shell (extension);
	shell_settings = e_shell_get_shell_settings (shell);

	g_object_get (
		shell_settings,
		"mailto-handler-command", &mailto_command,
		"mailto-handler-check", &check_mailto_handler,
		NULL);

	/* Should we check the "mailto" URI handler? */
	if (!check_mailto_handler)
		goto exit;

	/* Is Evolution already handling "mailto" URIs? */
	if (mailto_handler_is_evolution (mailto_command))
		goto exit;

	/* Does the user want Evolution to handle them? */
	if (!mailto_handler_prompt (extension))
		goto exit;

	/* Configure Evolution to be the "mailto" URI handler. */

	g_object_set (
		shell_settings,
		"mailto-handler-command", MAILTO_COMMAND,
		"mailto-handler-enabled", TRUE,
		"mailto-handler-needs-terminal", FALSE,
		NULL);

exit:
	g_free (mailto_command);
}

static void
mailto_handler_constructed (GObject *object)
{
	EShell *shell;
	EShellSettings *shell_settings;
	EMailtoHandler *extension;

	extension = E_MAILTO_HANDLER (object);

	shell = mailto_handler_get_shell (extension);
	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_settings_install_property_for_key (
		"mailto-handler-check",
		"/apps/evolution/mail/prompts/checkdefault");

	e_shell_settings_install_property_for_key (
		"mailto-handler-command",
		"/desktop/gnome/url-handlers/mailto/command");

	e_shell_settings_install_property_for_key (
		"mailto-handler-enabled",
		"/desktop/gnome/url-handlers/mailto/enabled");

	e_shell_settings_install_property_for_key (
		"mailto-handler-needs-terminal",
		"/desktop/gnome/url-handlers/mailto/needs_terminal");

	g_signal_connect_swapped (
		shell, "event::ready-to-start",
		G_CALLBACK (mailto_handler_check), extension);
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
