/*
 * evolution-web-inspector.c
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

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_WEB_INSPECTOR \
	(e_web_inspector_get_type ())
#define E_WEB_INSPECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEB_INSPECTOR, EWebInspector))

typedef struct _EWebInspector EWebInspector;
typedef struct _EWebInspectorClass EWebInspectorClass;

struct _EWebInspector {
	EExtension parent;
};

struct _EWebInspectorClass {
	EExtensionClass parent_class;
};

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='inspect-menu' >"
"      <menuitem action='inspect'/>"
"    </placeholder>"
"  </popup>"
"</ui>";

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_web_inspector_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EWebInspector, e_web_inspector, E_TYPE_EXTENSION)

static EWebView *
web_inspector_get_web_view (EWebInspector *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_WEB_VIEW (extensible);
}

static void
web_inspector_action_inspect_cb (GtkAction *action,
                                 EWebInspector *extension)
{
	WebKitWebInspector *inspector;
	EWebView *web_view;

	web_view = web_inspector_get_web_view (extension);
	inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (web_view));

	webkit_web_inspector_show (inspector);
}

static GtkActionEntry inspect_entries[] = {

	{ "inspect",
	  NULL,
	  N_("_Inspect..."),
	  NULL,
	  N_("Inspect the HTML content (debugging feature)"),
	  G_CALLBACK (web_inspector_action_inspect_cb) }
};

static WebKitWebView *
web_inspector_inspect_web_view_cb (WebKitWebInspector *inspector,
                                   EWebInspector *extension)
{
	GtkWidget *web_view;
	GtkWidget *window;
	const gchar *title;

	title = _("Evolution Web Inspector");
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), title);
	gtk_widget_set_size_request (window, 600, 400);
	gtk_widget_show (window);

	web_view = webkit_web_view_new ();
	gtk_container_add (GTK_CONTAINER (window), web_view);
	gtk_widget_show (web_view);

	return WEBKIT_WEB_VIEW (web_view);
}

static void
web_inspector_constructed (GObject *object)
{
	EWebInspector *extension;
	WebKitWebSettings *settings;
	WebKitWebInspector *inspector;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	EWebView *web_view;
	GError *error = NULL;

	extension = E_WEB_INSPECTOR (object);
	web_view = web_inspector_get_web_view (extension);

	ui_manager = e_web_view_get_ui_manager (web_view);
	action_group = e_web_view_get_action_group (web_view, "standard");

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (web_view));
	g_object_set (settings, "enable-developer-extras", TRUE, NULL);

	inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (web_view));

	g_signal_connect (
		inspector, "inspect-web-view",
		G_CALLBACK (web_inspector_inspect_web_view_cb), extension);

	gtk_action_group_add_actions (
		action_group, inspect_entries,
		G_N_ELEMENTS (inspect_entries), extension);

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here implies a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);
}

static void
e_web_inspector_class_init (EWebInspectorClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = web_inspector_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_WEB_VIEW;
}

static void
e_web_inspector_class_finalize (EWebInspectorClass *class)
{
}

static void
e_web_inspector_init (EWebInspector *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_web_inspector_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
