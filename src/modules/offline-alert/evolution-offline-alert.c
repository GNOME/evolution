/*
 * evolution-offline-alert.c
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

#include <libebackend/libebackend.h>

#include <shell/e-shell-view.h>
#include <shell/e-shell-window-actions.h>

/* Standard GObject macros */
#define E_TYPE_OFFLINE_ALERT \
	(e_offline_alert_get_type ())
#define E_OFFLINE_ALERT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_OFFLINE_ALERT, EOfflineAlert))

typedef struct _EOfflineAlert EOfflineAlert;
typedef struct _EOfflineAlertClass EOfflineAlertClass;

struct _EOfflineAlert {
	EExtension parent;
	gpointer alert;  /* weak pointer */
};

struct _EOfflineAlertClass {
	EExtensionClass parent_class;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_offline_alert_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EOfflineAlert, e_offline_alert, E_TYPE_EXTENSION)

static EShell *
offline_alert_get_shell (EOfflineAlert *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_SHELL (extensible);
}

static void
offline_alert_online_cb (EShell *shell,
                         GParamSpec *pspec,
                         EOfflineAlert *extension)
{
	if (!e_shell_get_online (shell))
		return;

	if (extension->alert != NULL)
		e_alert_response (extension->alert, GTK_RESPONSE_OK);
}

static void
offline_alert_network_available_cb (EShell *shell,
                                    GParamSpec *pspec,
                                    EOfflineAlert *extension)
{
	if (e_shell_get_network_available (shell)) {
		if (extension->alert != NULL)
			e_alert_response (extension->alert, GTK_RESPONSE_OK);
		return;
	}

	if (!e_shell_get_online (shell))
		return;

	g_return_if_fail (extension->alert == NULL);

	extension->alert = e_alert_new ("offline-alert:no-network", NULL);

	g_object_add_weak_pointer (
		G_OBJECT (extension->alert), &extension->alert);

	e_shell_submit_alert (shell, extension->alert);

	g_object_unref (extension->alert);
}

static void
offline_alert_window_added_cb (GtkApplication *application,
                               GtkWindow *window,
                               EOfflineAlert *extension)
{
	EShell *shell = E_SHELL (application);
	EUIAction *action;
	const gchar *alert_id;

	if (!E_IS_SHELL_WINDOW (window))
		return;

	/* Connect these signals after we have the first EShellWindow
	 * to avoid false-positive signals during EShell initialization. */

	e_signal_connect_notify (
		shell, "notify::online",
		G_CALLBACK (offline_alert_online_cb), extension);

	e_signal_connect_notify (
		shell, "notify::network-available",
		G_CALLBACK (offline_alert_network_available_cb), extension);

	g_signal_handlers_disconnect_by_func (
		shell, offline_alert_window_added_cb, extension);

	if (e_shell_get_online (shell))
		return;

	g_return_if_fail (extension->alert == NULL);

	/* This alert only shows at startup, not when the user
	 * chooses to work offline.  That's why we only wanted
	 * the first EShellWindow. */

	action = E_SHELL_WINDOW_ACTION_WORK_ONLINE (window);

	if (e_shell_get_network_available (shell))
		alert_id = "offline-alert:offline";
	else
		alert_id = "offline-alert:no-network";
	extension->alert = e_alert_new (alert_id, NULL);
	e_alert_add_action (extension->alert, action, GTK_RESPONSE_NONE, FALSE);

	g_object_add_weak_pointer (
		G_OBJECT (extension->alert), &extension->alert);

	e_shell_submit_alert (shell, extension->alert);

	g_object_unref (extension->alert);
}

static void
offline_alert_dispose (GObject *object)
{
	EOfflineAlert *extension;

	extension = E_OFFLINE_ALERT (object);

	if (extension->alert != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (extension->alert), &extension->alert);
		extension->alert = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_offline_alert_parent_class)->dispose (object);
}

static void
offline_alert_constructed (GObject *object)
{
	EShell *shell;
	EOfflineAlert *extension;

	extension = E_OFFLINE_ALERT (object);
	shell = offline_alert_get_shell (extension);

	/* Watch for the first EShellWindow. */
	g_signal_connect (
		shell, "window-added",
		G_CALLBACK (offline_alert_window_added_cb), extension);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_offline_alert_parent_class)->constructed (object);
}

static void
e_offline_alert_class_init (EOfflineAlertClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = offline_alert_dispose;
	object_class->constructed = offline_alert_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_offline_alert_class_finalize (EOfflineAlertClass *class)
{
}

static void
e_offline_alert_init (EOfflineAlert *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_offline_alert_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
