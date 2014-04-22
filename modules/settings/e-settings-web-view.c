/*
 * e-settings-web-view.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "e-settings-web-view.h"

#include <e-util/e-util.h>

#define E_SETTINGS_WEB_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_WEB_VIEW, ESettingsWebViewPrivate))

struct _ESettingsWebViewPrivate {
	GtkCssProvider *css_provider;
	GSettings *settings;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsWebView,
	e_settings_web_view,
	E_TYPE_EXTENSION)

/* replaces content of color string */
static void
settings_web_view_fix_color_string (gchar *color_string)
{
	GdkColor color;

	if (color_string == NULL)
		return;

	if (strlen (color_string) < 13)
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
settings_web_view_load_style (ESettingsWebView *extension)
{
	GString *buffer;
	gchar *citation_color;
	gchar *monospace_font;
	gchar *variable_font;
	gboolean custom_fonts;
	gboolean mark_citations;
	EExtensible *extensible;
	GtkStyleContext *style_context;
	GSettings *settings;
	GError *error = NULL;

	/* Some of our mail and composer preferences are passed down to
	 * GtkHtml through style properties, unfortunately.  This builds
	 * a style sheet for the EWebView using values from GSettings. */

	settings = extension->priv->settings;

	custom_fonts =
		g_settings_get_boolean (settings, "use-custom-font");
	monospace_font =
		g_settings_get_string (settings, "monospace-font");
	variable_font =
		g_settings_get_string (settings, "variable-width-font");
	mark_citations =
		g_settings_get_boolean (settings, "mark-citations");
	citation_color =
		g_settings_get_string (settings, "citation-color");

	buffer = g_string_new ("EWebViewGtkHTML {\n");

	settings_web_view_fix_color_string (citation_color);

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

	g_string_append (buffer, "}\n");

	gtk_css_provider_load_from_data (
		extension->priv->css_provider,
		buffer->str, buffer->len, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_string_free (buffer, TRUE);

	g_free (monospace_font);
	g_free (variable_font);
	g_free (citation_color);

	extensible = e_extension_get_extensible (E_EXTENSION (extension));
	style_context = gtk_widget_get_style_context (GTK_WIDGET (extensible));
	gtk_style_context_invalidate (style_context);
}

static void
settings_web_view_changed_cb (GSettings *settings,
                              const gchar *key,
                              ESettingsWebView *extension)
{
	settings_web_view_load_style (extension);
}

static void
settings_web_view_realize (GtkWidget *widget,
                           ESettingsWebView *extension)
{
	GSettings *settings;

	settings = extension->priv->settings;

	g_settings_bind (
		settings, "composer-inline-spelling",
		widget, "inline-spelling",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-magic-links",
		widget, "magic-links",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-magic-smileys",
		widget, "magic-smileys",
		G_SETTINGS_BIND_GET);

	gtk_style_context_add_provider (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (extension->priv->css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	settings_web_view_load_style (extension);

	/* Reload the style sheet when certain settings change. */

	g_signal_connect (
		settings, "changed::use-custom-font",
		G_CALLBACK (settings_web_view_changed_cb), extension);

	g_signal_connect (
		settings, "changed::monospace-font",
		G_CALLBACK (settings_web_view_changed_cb), extension);

	g_signal_connect (
		settings, "changed::variable-width-font",
		G_CALLBACK (settings_web_view_changed_cb), extension);

	g_signal_connect (
		settings, "changed::mark-citations",
		G_CALLBACK (settings_web_view_changed_cb), extension);

	g_signal_connect (
		settings, "changed::citation-color",
		G_CALLBACK (settings_web_view_changed_cb), extension);
}

static void
settings_web_view_dispose (GObject *object)
{
	ESettingsWebViewPrivate *priv;

	priv = E_SETTINGS_WEB_VIEW_GET_PRIVATE (object);

	if (priv->settings != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->settings,
			settings_web_view_changed_cb, object);
	}

	g_clear_object (&priv->css_provider);
	g_clear_object (&priv->settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_settings_web_view_parent_class)->dispose (object);
}

static void
settings_web_view_constructed (GObject *object)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	/* Wait to bind settings until the EWebView is realized so
	 * GtkhtmlEditor has a chance to install a GtkHTMLEditorAPI.
	 * Otherwise our settings will have no effect. */

	g_signal_connect (
		extensible, "realize",
		G_CALLBACK (settings_web_view_realize), object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_web_view_parent_class)->
		constructed (object);
}

static void
e_settings_web_view_class_init (ESettingsWebViewClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsWebViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = settings_web_view_dispose;
	object_class->constructed = settings_web_view_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_WEB_VIEW;
}

static void
e_settings_web_view_class_finalize (ESettingsWebViewClass *class)
{
}

static void
e_settings_web_view_init (ESettingsWebView *extension)
{
	GSettings *settings;

	extension->priv = E_SETTINGS_WEB_VIEW_GET_PRIVATE (extension);

	extension->priv->css_provider = gtk_css_provider_new ();

	settings = g_settings_new ("org.gnome.evolution.mail");
	extension->priv->settings = settings;
}

void
e_settings_web_view_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_web_view_register_type (type_module);
}

