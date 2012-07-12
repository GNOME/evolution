/*
 * e-mail-config-web-view.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "e-mail-config-web-view.h"

#include <libebackend/libebackend.h>

#include <shell/e-shell.h>
#include <misc/e-web-view.h>

typedef struct _EMailConfigWebView EMailConfigWebView;
typedef struct _EMailConfigWebViewClass EMailConfigWebViewClass;

struct _EMailConfigWebView {
	EExtension parent;
	GtkCssProvider *css_provider;
	EShellSettings *shell_settings;
};

struct _EMailConfigWebViewClass {
	EExtensionClass parent_class;
};

static gpointer parent_class;

/* replaces content of color string */
static void
fix_color_string (gchar *color_string)
{
	GdkColor color;

	if (!color_string || strlen (color_string) < 13)
		return;

	if (!gdk_color_parse (color_string, &color))
		return;

	sprintf (
		color_string, "#%02x%02x%02x",
		(gint) color.red * 256 / 65536,
		(gint) color.green * 256 / 65536,
		(gint) color.blue * 256 / 65536);
}

static void
mail_config_web_view_load_style (EMailConfigWebView *extension)
{
	GString *buffer;
	gchar *citation_color;
	gchar *monospace_font;
	gchar *spell_color;
	gchar *variable_font;
	gboolean custom_fonts;
	gboolean mark_citations;
	EExtensible *extensible;
	GtkStyleContext *style_context;
	GError *error = NULL;

	/* Some of our mail and composer preferences are passed down to
	 * GtkHtml through style properties, unfortunately.  This builds
	 * a style sheet for the EWebView using values from GSettings. */

	custom_fonts = e_shell_settings_get_boolean (
		extension->shell_settings, "mail-use-custom-fonts");

	monospace_font = e_shell_settings_get_string (
		extension->shell_settings, "mail-font-monospace");

	variable_font = e_shell_settings_get_string (
		extension->shell_settings, "mail-font-variable");

	mark_citations = e_shell_settings_get_boolean (
		extension->shell_settings, "mail-mark-citations");

	citation_color = e_shell_settings_get_string (
		extension->shell_settings, "mail-citation-color");

	spell_color = e_shell_settings_get_string (
		extension->shell_settings, "composer-spell-color");

	buffer = g_string_new ("EWebView {\n");

	fix_color_string (citation_color);
	fix_color_string (spell_color);

	if (custom_fonts && variable_font != NULL)
		g_string_append_printf (
			buffer, "  font: %s;\n", variable_font);

	if (custom_fonts && monospace_font != NULL)
		g_string_append_printf (
			buffer, "  -GtkHTML-fixed-font-name: '%s';\n",
			monospace_font);

	if (mark_citations && citation_color != NULL)
		g_string_append_printf (
			buffer, "  -GtkHTML-cite-color: %s;\n",
			citation_color);

	if (spell_color != NULL)
		g_string_append_printf (
			buffer, "  -GtkHTML-spell-error-color: %s;\n",
			spell_color);

	g_string_append (buffer, "}\n");

	gtk_css_provider_load_from_data (
		extension->css_provider, buffer->str, buffer->len, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_string_free (buffer, TRUE);

	g_free (monospace_font);
	g_free (variable_font);
	g_free (citation_color);
	g_free (spell_color);

	extensible = e_extension_get_extensible (E_EXTENSION (extension));
	style_context = gtk_widget_get_style_context (GTK_WIDGET (extensible));
	gtk_style_context_invalidate (style_context);
}

static void
mail_config_web_view_realize (GtkWidget *widget,
                              EMailConfigWebView *extension)
{
	g_object_bind_property (
		extension->shell_settings,
		"composer-inline-spelling",
		widget, "inline-spelling",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		extension->shell_settings,
		"composer-magic-links",
		widget, "magic-links",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		extension->shell_settings,
		"composer-magic-smileys",
		widget, "magic-smileys",
		G_BINDING_SYNC_CREATE);

	gtk_style_context_add_provider (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (extension->css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	mail_config_web_view_load_style (extension);

	/* Reload the style sheet when certain settings change. */

	g_signal_connect_swapped (
		extension->shell_settings,
		"notify::mail-use-custom-fonts",
		G_CALLBACK (mail_config_web_view_load_style),
		extension);

	g_signal_connect_swapped (
		extension->shell_settings,
		"notify::mail-font-monospace",
		G_CALLBACK (mail_config_web_view_load_style),
		extension);

	g_signal_connect_swapped (
		extension->shell_settings,
		"notify::mail-font-variable",
		G_CALLBACK (mail_config_web_view_load_style),
		extension);

	g_signal_connect_swapped (
		extension->shell_settings,
		"notify::mail-mark-citations",
		G_CALLBACK (mail_config_web_view_load_style),
		extension);

	g_signal_connect_swapped (
		extension->shell_settings,
		"notify::mail-citation-color",
		G_CALLBACK (mail_config_web_view_load_style),
		extension);

	g_signal_connect_swapped (
		extension->shell_settings,
		"notify::composer-spell-color",
		G_CALLBACK (mail_config_web_view_load_style),
		extension);
}

static void
mail_config_web_view_dispose (GObject *object)
{
	EMailConfigWebView *extension;

	extension = (EMailConfigWebView *) object;

	if (extension->css_provider != NULL) {
		g_object_unref (extension->css_provider);
		extension->css_provider = NULL;
	}

	if (extension->shell_settings != NULL) {
		g_signal_handlers_disconnect_by_func (
			extension->shell_settings,
			mail_config_web_view_load_style, object);
		g_object_unref (extension->shell_settings);
		extension->shell_settings = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_config_web_view_constructed (GObject *object)
{
	EShell *shell;
	EShellSettings *shell_settings;
	EMailConfigWebView *extension;
	EExtensible *extensible;

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	extension = (EMailConfigWebView *) object;
	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	extension->css_provider = gtk_css_provider_new ();
	extension->shell_settings = g_object_ref (shell_settings);

	/* Wait to bind shell settings until the EWebView is realized
	 * so GtkhtmlEditor has a chance to install a GtkHTMLEditorAPI.
	 * Otherwise our settings will have no effect. */

	g_signal_connect (
		extensible, "realize",
		G_CALLBACK (mail_config_web_view_realize), extension);

	/* Chain up to parent's consturcted() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
mail_config_web_view_class_init (EMailConfigWebViewClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_config_web_view_dispose;
	object_class->constructed = mail_config_web_view_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_WEB_VIEW;
}

void
e_mail_config_web_view_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EMailConfigWebViewClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_config_web_view_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMailConfigWebView),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, E_TYPE_EXTENSION,
		"EMailConfigWebView", &type_info, 0);
}
