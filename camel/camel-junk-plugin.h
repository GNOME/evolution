/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *  Radek Doulik <rodo@ximian.com>
 *
 * Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef _CAMEL_JUNK_PLUGIN_H
#define _CAMEL_JUNK_PLUGIN_H

#define CAMEL_JUNK_PLUGIN(x) ((CamelJunkPlugin *) x)

typedef struct _CamelJunkPlugin CamelJunkPlugin;
struct _CamelMimeMessage;

struct _CamelJunkPlugin
{
	/* junk filter human readable name, translated */
	const char * (*get_name) (void);

	/* should be set to 1 */
	int api_version;

	/* when called, it should return TRUE if message is identified as junk,
	   FALSE otherwise */
	int (*check_junk) (struct _CamelMimeMessage *message);

	/* called when user identified a message to be junk */
	void (*report_junk) (struct _CamelMimeMessage *message);

	/* called when user identified a message not to be junk */
	void (*report_notjunk) (struct _CamelMimeMessage *message);

	/* called after one or more junk/ham(s) reported */
	void (*commit_reports) (void);

	/* called before all other calls to junk plugin for initialization */
	void (*init) (void);
};

const char * camel_junk_plugin_get_name (CamelJunkPlugin *csp);
int camel_junk_plugin_check_junk (CamelJunkPlugin *csp, struct _CamelMimeMessage *message);
void camel_junk_plugin_report_junk (CamelJunkPlugin *csp, struct _CamelMimeMessage *message);
void camel_junk_plugin_report_notjunk (CamelJunkPlugin *csp, struct _CamelMimeMessage *message);
void camel_junk_plugin_commit_reports (CamelJunkPlugin *csp);
void camel_junk_plugin_init (CamelJunkPlugin *csp);

#endif
