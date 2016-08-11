/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
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
