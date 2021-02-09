/*
 * evolution-webkit-inspector.c
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
#include <gdk/gdkkeysyms.h>

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_WEBKIT_INSPECTOR \
	(e_webkit_inspector_get_type ())
#define E_WEBKIT_INSPECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEBKIT_INSPECTOR, EWebKitInspector))

/* <Control>+<Shift>+I or <Control>+<Shift>+D */
#define WEBKIT_INSPECTOR_MOD  (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define WEBKIT_INSPECTOR_KEY1  (GDK_KEY_I)
#define WEBKIT_INSPECTOR_KEY2  (GDK_KEY_D)

#define WEBKIT_INSPECTOR_SHORTCUT_SHOW(event) \
	((((event)->state & WEBKIT_INSPECTOR_MOD) == WEBKIT_INSPECTOR_MOD) && \
	 (((event)->keyval == WEBKIT_INSPECTOR_KEY1) || ((event)->keyval == WEBKIT_INSPECTOR_KEY2)))

typedef struct _EWebKitInspector EWebKitInspector;
typedef struct _EWebKitInspectorClass EWebKitInspectorClass;

struct _EWebKitInspector {
	EExtension parent;
};

struct _EWebKitInspectorClass {
	EExtensionClass parent_class;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_webkit_inspector_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EWebKitInspector, e_webkit_inspector, E_TYPE_EXTENSION)

static WebKitWebView *
webkit_inspector_get_web_view (EWebKitInspector *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return WEBKIT_WEB_VIEW (extensible);
}

static gboolean
webkit_inspector_key_press_event_cb (WebKitWebView *web_view,
                                  GdkEventKey *event)
{
	WebKitWebInspector *inspector;
	gboolean handled = FALSE;

	inspector = webkit_web_view_get_inspector (web_view);

	if (WEBKIT_INSPECTOR_SHORTCUT_SHOW (event)) {
		webkit_web_inspector_show (inspector);
		handled = TRUE;
	}

	return handled;
}

static void
webkit_inspector_constructed (GObject *object)
{
	EWebKitInspector *extension;
	WebKitWebView *web_view;
	WebKitSettings *settings;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_webkit_inspector_parent_class)->constructed (object);

	extension = E_WEBKIT_INSPECTOR (object);
	web_view = webkit_inspector_get_web_view (extension);
	settings = webkit_web_view_get_settings (web_view);
	webkit_settings_set_enable_developer_extras (settings, TRUE);

	g_signal_connect (
		web_view, "key-press-event",
		G_CALLBACK (webkit_inspector_key_press_event_cb), NULL);
}

static void
e_webkit_inspector_class_init (EWebKitInspectorClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = webkit_inspector_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = WEBKIT_TYPE_WEB_VIEW;
}

static void
e_webkit_inspector_class_finalize (EWebKitInspectorClass *class)
{
}

static void
e_webkit_inspector_init (EWebKitInspector *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	if (e_util_get_webkit_developer_mode_enabled ())
		e_webkit_inspector_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
