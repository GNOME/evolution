/*
 * ipod.c - Find an iPod mount point using HAL
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
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2004 Justin Wake <jwake@iinet.net.au>
 *
 */

#include "config.h"
#include "evolution-ipod-sync.h"
#include <unistd.h>
#include <string.h>

/**
 * Ensure that HAL is running before we try to use it.
 * From gnome-volume-manager's src/properties.c
 */
gboolean
check_hal (void)
{
	LibHalContext *ctx;
	gchar **devices;
	gint num;
	DBusConnection *conn;

	conn = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);

	ctx = libhal_ctx_new ();
	libhal_ctx_set_dbus_connection (ctx, conn);
	if (!libhal_ctx_init(ctx, NULL))
		return FALSE;
	devices = libhal_get_all_devices (ctx, &num, NULL);
	if (!devices)
	{
		libhal_ctx_shutdown (ctx, NULL);
		return FALSE;
	}
	libhal_free_string_array (devices);

	libhal_ctx_shutdown (ctx, NULL);
	return TRUE;
}

#define MOUNT "/bin/mount"
#define UMOUNT "/bin/umount"

/**
 * Try to mount a given device.
 */
static gboolean
try_mount (gchar *device)
{
	gchar *argv[3];
	GError *err = NULL;
	gint exit_status;

	argv[0] = (gchar *)MOUNT;
	argv[1] = device;
	argv[2] = NULL;

	if (!g_spawn_sync (g_get_home_dir (), argv, NULL, 0, NULL, NULL, NULL,
							 NULL, &exit_status, &err))
	{
		g_warning ("try_mount failed: %s", err->message);
		return FALSE;
	}

	return (exit_status == 0);
}

/**
 * Try to unmount a given device.
 */
gboolean
try_umount (gchar *device)
{
	gchar *argv[3];
	GError *err = NULL;
	gint exit_status;

	argv[0] = (gchar *)UMOUNT;
	argv[1] = device;
	argv[2] = NULL;

	sync ();

	if (!g_spawn_sync (g_get_home_dir (), argv, NULL, 0, NULL, NULL, NULL,
							 NULL, &exit_status, &err))
	{
		g_warning ("try_umount failed: %s", err->message);
		return FALSE;
	}

	return (exit_status == 0);
}

/**
 * See if a given mount point contains an iPod.
 *
 * Do this by checking for the presence of an iTunes
 * database at <mount_point>/iPod_Control/iTunes/.
 */
static gboolean
is_ipod (gchar *mount_point)
{
	gboolean ret = FALSE;

	gchar *itunes_path;

	itunes_path = g_build_path (G_DIR_SEPARATOR_S, mount_point,
										 "iPod_Control", "iTunes",
										 NULL);

	if (!g_file_test (itunes_path, G_FILE_TEST_IS_DIR))
		goto out;

	ret = TRUE;

out:
	g_free (itunes_path);
	return ret;
}

/**
 * Try to find a mount point for an iPod.
 */
gchar *
find_ipod_mount_point (LibHalContext *ctx)
{
	gchar **apple_devices = NULL;
	gchar **volumes = NULL;
	gchar *udi, *udi2, *device, *fsusage, *mount_point = NULL;
	gchar *retval = NULL;
	gint apple_count = 0;
	gint volume_count = 0;
	gint has_fs = 0;
	gint i, j;

	/* First, we look for things made by Apple. */
	apple_devices = libhal_manager_find_device_string_match (ctx,
							"info.vendor",
							"Apple",
							&apple_count,
							NULL);

	for (i = 0; i < apple_count; i++)
	{
		udi = apple_devices[i];

		volumes = NULL;
		volumes = libhal_manager_find_device_string_match (ctx,
							"info.parent",
							udi,
							&volume_count,
							NULL);

		for (j = 0; j < volume_count; j++)
		{
			udi2 = volumes[j];

			/* Only interested if it has a filesystem. */
			has_fs = 0;

			if (!libhal_device_property_exists (ctx, udi2,
								"volume.is_filesystem", NULL) ||
				  !libhal_device_get_property_bool (ctx, udi2,
								"volume.is_filesystem", NULL))
			{
				has_fs = 1;
			}

			fsusage = libhal_device_get_property_string (ctx, udi2,
								"volume.fsusage", NULL);

			if (fsusage == NULL)
				continue;

			if (strncmp (fsusage, "filesystem", 10) == 0)
			{
				has_fs = 1;
			}

			libhal_free_string (fsusage);

			if (has_fs == 0)
				continue;

			device = libhal_device_get_property_string (ctx, udi2,
									"block.device", NULL);

			/* Let's see if it's mounted. */
			if (!libhal_device_property_exists (ctx, udi2,
								"volume.is_mounted", NULL) ||
				 !libhal_device_get_property_bool (ctx, udi2,
								"volume.is_mounted", NULL))
			{
				/* It isn't, so let's attempt to mount it */
				if (device != NULL)
				{
					try_mount (device);
				}
			}

			if (!libhal_device_property_exists (ctx, udi2,
								"volume.mount_point", NULL))
			{
				libhal_free_string (device);
				continue;
			}

			libhal_free_string (device);

			mount_point = libhal_device_get_property_string (ctx, udi2,
											"volume.mount_point", NULL);

			if (is_ipod (mount_point))
			{
				goto out;
			}

			libhal_free_string (mount_point);
			mount_point = NULL;
		}
	}

out:
	if (volumes != NULL)
		libhal_free_string_array (volumes);

	if (apple_devices != NULL)
		libhal_free_string_array (apple_devices);

	if (mount_point != NULL)
	{
		retval = g_strdup (mount_point);
		libhal_free_string (mount_point);
	}

	return (retval);
}
