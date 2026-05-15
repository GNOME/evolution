/*
 * SPDX-FileCopyrightText: (C) 2016 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef TEST_KEYFILE_SETTINGS_BACKEND_H
#define TEST_KEYFILE_SETTINGS_BACKEND_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* This is a private GSettingsBackend implementation,
   meant for testing purposes only.

   It requires set 'TEST_KEYFILE_SETTINGS_FILENAME' environment variable,
   with a file name to use for the settings backend.
*/

#define TEST_KEYFILE_SETTINGS_BACKEND_NAME "evolution-test-keyfile"
#define TEST_KEYFILE_SETTINGS_FILENAME_ENVVAR "TEST_KEYFILE_SETTINGS_FILENAME"

G_END_DECLS

#endif /* TEST_KEYFILE_SETTINGS_BACKEND_H */
