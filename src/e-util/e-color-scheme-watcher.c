/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

/* This is partly based on:
 * https://gitlab.gnome.org/GNOME/Initiatives/-/wikis/Dark-Style-Preference
 * and partly on the libhandy code.
 */

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "e-color-scheme-watcher.h"

typedef enum {
	E_COLOR_SCHEME_UNKNOWN = -1,
	E_COLOR_SCHEME_DEFAULT = 0,
	E_COLOR_SCHEME_PREFER_DARK,
	E_COLOR_SCHEME_PREFER_LIGHT
} EColorScheme;

struct _EColorSchemeWatcher
{
	GObject parent_instance;

	GCancellable *cancellable;
	GDBusProxy *settings_portal;
	gchar *last_theme_name;
	EColorScheme color_scheme;
	gboolean use_fdo_setting;
};

G_DEFINE_TYPE (EColorSchemeWatcher, e_color_scheme_watcher, G_TYPE_OBJECT);

#if (GTK_MINOR_VERSION % 2)
#define MINOR (GTK_MINOR_VERSION + 1)
#else
#define MINOR GTK_MINOR_VERSION
#endif

static gboolean
e_color_scheme_watcher_theme_dir_exists (const gchar *dir,
					 const gchar *subdir,
					 const gchar *name,
					 const gchar *variant)
{
	gchar *file;
	gchar *base;
	gboolean exists = FALSE;
	gint ii;

	if (variant)
		file = g_strconcat ("gtk-", variant, ".css", NULL);
	else
		file = g_strdup ("gtk.css");

	if (subdir)
		base = g_build_filename (dir, subdir, name, NULL);
	else
		base = g_build_filename (dir, name, NULL);

	for (ii = MINOR; ii >= 0 && !exists; ii = ii - 2) {
		gchar *subsubdir = NULL;
		gchar *path = NULL;

		if (ii < 14)
			ii = 0;

		subsubdir = g_strdup_printf ("gtk-3.%d", ii);
		path = g_build_filename (base, subsubdir, file, NULL);

		exists = g_file_test (path, G_FILE_TEST_EXISTS);

		g_clear_pointer (&path, g_free);
		g_clear_pointer (&subsubdir, g_free);
	}

	g_clear_pointer (&file, g_free);
	g_clear_pointer (&base, g_free);

	return exists;
}

#undef MINOR

static gboolean
e_color_scheme_watcher_theme_exists (const gchar *name,
				     const gchar *variant)
{
	gchar *dir = NULL;
	const gchar *const *dirs;
	const gchar *var;
	gint ii;

	/* First look in the user's data directory */
	if (e_color_scheme_watcher_theme_dir_exists (g_get_user_data_dir (), "themes", name, variant))
		return TRUE;

	/* Next look in the user's home directory */
	if (e_color_scheme_watcher_theme_dir_exists (g_get_home_dir (), ".themes", name, variant))
		return TRUE;

	/* Look in system data directories */
	dirs = g_get_system_data_dirs ();
	for (ii = 0; dirs[ii]; ii++) {
		if (e_color_scheme_watcher_theme_dir_exists (dirs[ii], "themes", name, variant))
			return TRUE;
	}

	/* Finally, try in the default theme directory */

	var = g_getenv ("GTK_DATA_PREFIX");
	if (var)
		dir = g_build_filename (var, "share", "themes", NULL);
	else
		dir = g_build_filename (EVOLUTION_DATADIR, "themes", NULL);

	if (e_color_scheme_watcher_theme_dir_exists (dir, NULL, name, variant)) {
		g_free (dir);
		return TRUE;
	}

	g_free (dir);

	return FALSE;
}

static gboolean
e_color_scheme_watcher_check_theme_exists (const gchar *name,
					   const gchar *variant)
{
	gchar *resource_path = NULL;

	/* try loading the resource for the theme. This is mostly meant for built-in themes. */
	if (variant)
		resource_path = g_strdup_printf ("/org/gtk/libgtk/theme/%s/gtk-%s.css", name, variant);
	else
		resource_path = g_strdup_printf ("/org/gtk/libgtk/theme/%s/gtk.css", name);

	if (g_resources_get_info (resource_path, 0, NULL, NULL, NULL)) {
		g_free (resource_path);
		return TRUE;
	}

	g_free (resource_path);

	return e_color_scheme_watcher_theme_exists (name, variant);
}

static void
e_color_scheme_watcher_sync_theme (EColorSchemeWatcher *self)
{
	GtkSettings *gtk_settings = gtk_settings_get_default ();
	gchar *theme_name = NULL;

	g_object_get (gtk_settings,
		"gtk-theme-name", &theme_name,
		NULL);

	if (theme_name) {
		gboolean update_theme = FALSE;

		if (self->color_scheme == E_COLOR_SCHEME_PREFER_DARK) {
			g_object_set (gtk_settings,
				"gtk-application-prefer-dark-theme", TRUE,
				NULL);

			if (g_strcmp0 (theme_name, "HighContrast") == 0) {
				g_object_set (gtk_settings,
					"gtk-theme-name", "HighContrastInverse",
					NULL);
			} else if (g_strcmp0 (theme_name, "Breeze") == 0) {
				g_object_set (gtk_settings,
					"gtk-theme-name", "Breeze-Dark",
					NULL);
			} else {
				update_theme = g_strcmp0 (theme_name, "HighContrastInverse") != 0 &&
					       g_strcmp0 (theme_name, "Breeze-Dark") != 0;
			}
		} else if (self->color_scheme == E_COLOR_SCHEME_PREFER_LIGHT) {
			g_object_set (gtk_settings,
				"gtk-application-prefer-dark-theme", FALSE,
				NULL);

			if (g_strcmp0 (theme_name, "HighContrastInverse") == 0) {
				g_object_set (gtk_settings,
					"gtk-theme-name", "HighContrast",
					NULL);
			} else if (g_strcmp0 (theme_name, "Breeze-Dark") == 0) {
				g_object_set (gtk_settings,
					"gtk-theme-name", "Breeze",
					NULL);
			} else {
				update_theme = g_strcmp0 (theme_name, "HighContrast") != 0 &&
					       g_strcmp0 (theme_name, "Breeze") != 0;
			}
		} else {
			gtk_settings_reset_property (gtk_settings, "gtk-theme-name");
			gtk_settings_reset_property (gtk_settings, "gtk-application-prefer-dark-theme");
		}

		if (update_theme) {
			gchar *new_theme_name = NULL;
			gboolean suffix_cut = FALSE;

			if (g_str_has_suffix (theme_name, "-dark")) {
				theme_name[strlen (theme_name) - 5] = '\0';
				suffix_cut = TRUE;
			}

			if (self->color_scheme == E_COLOR_SCHEME_PREFER_DARK &&
			    e_color_scheme_watcher_check_theme_exists (theme_name, "dark")) {
				new_theme_name = g_strconcat (theme_name, "-dark", NULL);
				/* Verify whether the newly constructed name can be used; otherwise the theme
				   supports the dark variant with the original name. */
				if (!e_color_scheme_watcher_check_theme_exists (new_theme_name, NULL)) {
					g_free (new_theme_name);
					new_theme_name = theme_name;
					theme_name = NULL;
				}
			} else if (suffix_cut && e_color_scheme_watcher_check_theme_exists (theme_name, NULL)) {
				new_theme_name = theme_name;
				theme_name = NULL;
			}

			if (new_theme_name) {
				g_object_set (gtk_settings,
					"gtk-theme-name", new_theme_name,
					NULL);
			}

			g_free (new_theme_name);
		}

		g_free (theme_name);
	}
}

static void
e_color_scheme_watcher_set_color_scheme (EColorSchemeWatcher *self,
					 EColorScheme color_scheme)
{
	if (color_scheme > E_COLOR_SCHEME_PREFER_LIGHT || color_scheme == E_COLOR_SCHEME_UNKNOWN)
		color_scheme = E_COLOR_SCHEME_DEFAULT;

	if (color_scheme != self->color_scheme) {
		self->color_scheme = color_scheme;

		e_color_scheme_watcher_sync_theme (self);
	}
}

static EColorScheme
e_color_scheme_watcher_read_dgo (GVariant *value)
{
	const gchar *str;

	if (!value || !g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
		return E_COLOR_SCHEME_UNKNOWN;

	str = g_variant_get_string (value, NULL);

	if (!g_strcmp0 (str, "default"))
		return E_COLOR_SCHEME_DEFAULT;

	if (!g_strcmp0 (str, "prefer-dark"))
		return E_COLOR_SCHEME_PREFER_DARK;

	if (!g_strcmp0 (str, "prefer-light"))
		return E_COLOR_SCHEME_PREFER_LIGHT;

	g_debug ("Invalid/unknown GNOME color scheme: '%s'", str);

	return E_COLOR_SCHEME_UNKNOWN;
}

static void
e_color_scheme_watcher_settings_portal_changed_cb (GDBusProxy *proxy,
						   const gchar *sender_name,
						   const gchar *signal_name,
						   GVariant *parameters,
						   EColorSchemeWatcher *self)
{
	const gchar *namespc;
	const gchar *name;
	GVariant *value = NULL;

	if (g_strcmp0 (signal_name, "SettingChanged"))
		return;

	g_variant_get (parameters, "(&s&sv)", &namespc, &name, &value);

	if (self->use_fdo_setting &&
	    g_strcmp0 (namespc, "org.freedesktop.appearance") == 0 &&
	    g_strcmp0 (name, "color-scheme") == 0) {
		e_color_scheme_watcher_set_color_scheme (self, g_variant_get_uint32 (value));
	} else if (g_strcmp0 (namespc, "org.gnome.desktop.interface") == 0) {
		if (g_strcmp0 (name, "gtk-theme") == 0) {
			const gchar *theme_name = g_variant_get_string (value, NULL);
			if (g_strcmp0 (theme_name, self->last_theme_name) != 0) {
				GtkSettings *gtk_settings = gtk_settings_get_default ();

				g_clear_pointer (&self->last_theme_name, g_free);
				self->last_theme_name = g_strdup (theme_name);

				gtk_settings_reset_property (gtk_settings, "gtk-theme-name");
				gtk_settings_reset_property (gtk_settings, "gtk-application-prefer-dark-theme");
				e_color_scheme_watcher_sync_theme (self);
			}
		} else if (!self->use_fdo_setting && g_strcmp0 (name, "color-scheme") == 0) {
			EColorScheme color_scheme = e_color_scheme_watcher_read_dgo (value);

			if (color_scheme != E_COLOR_SCHEME_UNKNOWN)
				e_color_scheme_watcher_set_color_scheme (self, color_scheme);
		}
	}

	g_clear_pointer (&value, g_variant_unref);
}

static void
e_color_scheme_watcher_read_dgo_cb (GObject *source_object,
				    GAsyncResult *async_result,
				    gpointer user_data)
{
	EColorSchemeWatcher *self = user_data;
	GVariant *result;
	GVariant *child = NULL;
	GVariant *value = NULL;
	GError *error = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), async_result, &error);

	if (result) {
		g_variant_get (result, "(v)", &child);

		if (child) {
			g_variant_get (child, "v", &value);

			if (value) {
				EColorScheme color_scheme = e_color_scheme_watcher_read_dgo (value);

				if (color_scheme != E_COLOR_SCHEME_UNKNOWN) {
					e_color_scheme_watcher_set_color_scheme (self, color_scheme);

					g_signal_connect_object (self->settings_portal, "g-signal",
						G_CALLBACK (e_color_scheme_watcher_settings_portal_changed_cb), self, 0);
				}
			}
		}
	} else {
		g_debug ("Failed to read color scheme from GNOME: %s", error ? error->message : "Unknown error");
		g_clear_error (&error);
	}

	g_clear_pointer (&result, g_variant_unref);
	g_clear_pointer (&child, g_variant_unref);
	g_clear_pointer (&value, g_variant_unref);
}

static void
e_color_scheme_watcher_read_fdo_cb (GObject *source_object,
				    GAsyncResult *async_result,
				    gpointer user_data)
{
	EColorSchemeWatcher *self = user_data;
	GVariant *result;
	GVariant *child = NULL;
	GVariant *value = NULL;
	GError *error = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), async_result, &error);

	if (result) {
		g_variant_get (result, "(v)", &child);

		if (child) {
			g_variant_get (child, "v", &value);

			if (value) {
				EColorScheme color_scheme = g_variant_get_uint32 (value);

				self->use_fdo_setting = TRUE;

				e_color_scheme_watcher_set_color_scheme (self, color_scheme);

				g_signal_connect_object (self->settings_portal, "g-signal",
					G_CALLBACK (e_color_scheme_watcher_settings_portal_changed_cb), self, 0);
			}
		}
	} else {
		g_debug ("Failed to read color scheme from freedesktop.org: %s", error ? error->message : "Unknown error");
		g_clear_error (&error);
	}

	g_clear_pointer (&result, g_variant_unref);
	g_clear_pointer (&child, g_variant_unref);
	g_clear_pointer (&value, g_variant_unref);

	if (!self->use_fdo_setting) {
		g_dbus_proxy_call (self->settings_portal,
				   "Read",
				   g_variant_new ("(ss)",
						  "org.gnome.desktop.interface",
						  "color-scheme"),
				   G_DBUS_CALL_FLAGS_NONE,
				   5000,
				   self->cancellable,
				   e_color_scheme_watcher_read_dgo_cb,
				   self);
	}
}

static void
e_color_scheme_watcher_got_proxy_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	EColorSchemeWatcher *self = user_data;
	GDBusProxy *proxy;
	GError *error = NULL;

	proxy = g_dbus_proxy_new_for_bus_finish (result, &error);

	if (!proxy) {
		g_debug ("Failed to get color scheme proxy: %s\n", error ? error->message : "Unknown error");
		g_clear_error (&error);
		return;
	}

	g_return_if_fail (E_IS_COLOR_SCHEME_WATCHER (self));

	self->settings_portal = proxy;

	g_dbus_proxy_call (self->settings_portal,
			   "Read",
			   g_variant_new ("(ss)",
					  "org.freedesktop.appearance",
					  "color-scheme"),
			   G_DBUS_CALL_FLAGS_NONE,
			   5000,
			   self->cancellable,
			   e_color_scheme_watcher_read_fdo_cb,
			   self);
}

static void
e_color_scheme_watcher_dispose (GObject *object)
{
	EColorSchemeWatcher *self = E_COLOR_SCHEME_WATCHER (object);

	if (self->cancellable) {
		g_cancellable_cancel (self->cancellable);
		g_clear_object (&self->cancellable);
	}

	g_clear_object (&self->settings_portal);
	g_clear_pointer (&self->last_theme_name, g_free);

	G_OBJECT_CLASS (e_color_scheme_watcher_parent_class)->dispose (object);
}

static void
e_color_scheme_watcher_class_init (EColorSchemeWatcherClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = e_color_scheme_watcher_dispose;
}

static void
e_color_scheme_watcher_init (EColorSchemeWatcher *self)
{
	self->color_scheme = E_COLOR_SCHEME_UNKNOWN;
	self->cancellable = g_cancellable_new ();

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  "org.freedesktop.portal.Desktop",
				  "/org/freedesktop/portal/desktop",
				  "org.freedesktop.portal.Settings",
				  self->cancellable,
				  e_color_scheme_watcher_got_proxy_cb,
				  self);
}

EColorSchemeWatcher *
e_color_scheme_watcher_new (void)
{
	return g_object_new (E_TYPE_COLOR_SCHEME_WATCHER, NULL);
}
