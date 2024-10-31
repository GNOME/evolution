/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "shell/e-shell.h"

#include "e-rss-preferences.h"

#include "module-rss.h"

#define E_TYPE_RSS_SHELL_EXTENSION (e_rss_shell_extension_get_type ())

GType e_rss_shell_extension_get_type (void);

typedef struct _ERssShellExtension {
	EExtension parent;
} ERssShellExtension;

typedef struct _ERssShellExtensionClass {
	EExtensionClass parent_class;
} ERssShellExtensionClass;

G_DEFINE_DYNAMIC_TYPE (ERssShellExtension, e_rss_shell_extension, E_TYPE_EXTENSION)

static void
e_rss_ensure_esource (EShell *shell)
{
	ESourceRegistry *registry;
	ESource *rss_source;

	registry = e_shell_get_registry (shell);
	rss_source = e_source_registry_ref_source (registry, "rss");

	if (!rss_source) {
		GError *error = NULL;

		rss_source = e_source_new_with_uid ("rss", NULL, &error);

		if (rss_source) {
			ESourceMailAccount *mail_account;

			mail_account = e_source_get_extension (rss_source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
			e_source_mail_account_set_builtin (mail_account, TRUE);
			e_source_backend_set_backend_name (E_SOURCE_BACKEND (mail_account), "rss");
		} else {
			g_warning ("Failed to create RSS source: %s", error ? error->message : "Unknown error");
		}

		g_clear_error (&error);
	}

	if (rss_source) {
		GError *error = NULL;

		e_source_set_display_name (rss_source, _("News and Blogs"));

		if (!e_source_registry_commit_source_sync (registry, rss_source, NULL, &error))
			g_warning ("Failed to commit RSS source: %s", error ? error->message : "Unknown error");

		g_clear_error (&error);
	}

	g_clear_object (&rss_source);
}

static gboolean
init_preferences_idle_cb (gpointer user_data)
{
	EShell *shell = g_weak_ref_get (user_data);

	if (shell)
		e_rss_preferences_init (shell);

	g_clear_object (&shell);

	return G_SOURCE_REMOVE;
}

static void
e_rss_shell_ready_to_start_cb (EShell *shell)
{
	e_rss_ensure_esource (shell);

	g_idle_add_full (G_PRIORITY_LOW, init_preferences_idle_cb,
		e_weak_ref_new (shell), (GDestroyNotify) e_weak_ref_free);
}

static void
e_rss_shell_extension_constructed (GObject *object)
{
	/* Chain up to parent's method */
	G_OBJECT_CLASS (e_rss_shell_extension_parent_class)->constructed (object);

	g_signal_connect_object (e_extension_get_extensible (E_EXTENSION (object)), "event::ready-to-start",
		G_CALLBACK (e_rss_shell_ready_to_start_cb), NULL, 0);
}

static void
e_rss_shell_extension_class_init (ERssShellExtensionClass *klass)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_rss_shell_extension_constructed;

	extension_class = E_EXTENSION_CLASS (klass);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_rss_shell_extension_class_finalize (ERssShellExtensionClass *klass)
{
}

static void
e_rss_shell_extension_init (ERssShellExtension *extension)
{
}

void
e_rss_shell_extension_type_register (GTypeModule *type_module)
{
	e_rss_shell_extension_register_type (type_module);
}
