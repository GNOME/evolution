/*
 * em-junk.h
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
 *		Vivek Jain <jvivek@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_JUNK_H
#define EM_JUNK_H

#include <camel/camel.h>
#include <e-util/e-plugin.h>

#define EM_JUNK_ERROR (em_junk_error_quark ())

G_BEGIN_DECLS

typedef struct _EMJunkTarget EMJunkTarget;
typedef struct _EMJunkInterface EMJunkInterface;

typedef void (*EMJunkHookFunc) (EPlugin *plugin, EMJunkTarget *data);

struct _EMJunkTarget {
	CamelMimeMessage *m;
	GError *error;
};

struct _EMJunkInterface {
	CamelJunkPlugin camel;

	/* The hook forwards calls from Camel to the EPlugin. */
	EPluginHook *hook;

	/* These are symbol names in the EPlugin. */
	gchar *check_junk;
	gchar *report_junk;
	gchar *report_notjunk;
	gchar *commit_reports;
	gchar *validate_binary;

	gchar *plugin_name;
};

GQuark em_junk_error_quark (void);

G_END_DECLS

#endif /* EM_JUNK_H */
