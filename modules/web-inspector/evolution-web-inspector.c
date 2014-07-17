/*
 * evolution-web-inspector.c
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

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_WEB_INSPECTOR \
	(e_web_inspector_get_type ())
#define E_WEB_INSPECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEB_INSPECTOR, EWebInspector))

/* <Control>+<Shift>+I */
#define WEB_INSPECTOR_MOD  (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define WEB_INSPECTOR_KEY  (GDK_KEY_I)

#define WEB_INSPECTOR_SHORTCUT_SHOW(event) \
	((((event)->state & WEB_INSPECTOR_MOD) == WEB_INSPECTOR_MOD) && \
	((event)->keyval == WEB_INSPECTOR_KEY))

typedef struct _EWebInspector EWebInspector;
typedef struct _EWebInspectorClass EWebInspectorClass;

struct _EWebInspector {
	EExtension parent;
};

struct _EWebInspectorClass {
	EExtensionClass parent_class;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_web_inspector_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EWebInspector, e_web_inspector, E_TYPE_EXTENSION)

static WebKitWebView *
web_inspector_get_web_view (EWebInspector *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return WEBKIT_WEB_VIEW (extensible);
}

static gboolean
web_inspector_key_press_event_cb (WebKitWebView *web_view,
                                  GdkEventKey *event)
{
	WebKitWebInspector *inspector;
	gboolean handled = FALSE;

	inspector = webkit_web_view_get_inspector (web_view);

	if (WEB_INSPECTOR_SHORTCUT_SHOW (event)) {
		webkit_web_inspector_show (inspector);
		handled = TRUE;
	}

	return handled;
}

static WebKitWebView *
web_inspector_inspect_web_view_cb (WebKitWebInspector *inspector)
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
	WebKitWebView *web_view;
	WebKitWebSettings *settings;
	WebKitWebInspector *inspector;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_web_inspector_parent_class)->constructed (object);

	extension = E_WEB_INSPECTOR (object);
	web_view = web_inspector_get_web_view (extension);
	settings = webkit_web_view_get_settings (web_view);
	inspector = webkit_web_view_get_inspector (web_view);

	g_object_set (settings, "enable-developer-extras", TRUE, NULL);

	g_signal_connect (
		web_view, "key-press-event",
		G_CALLBACK (web_inspector_key_press_event_cb), NULL);

	g_signal_connect (
		inspector, "inspect-web-view",
		G_CALLBACK (web_inspector_inspect_web_view_cb), NULL);
}

static void
e_web_inspector_class_init (EWebInspectorClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = web_inspector_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = WEBKIT_TYPE_WEB_VIEW;
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
