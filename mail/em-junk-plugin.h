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

#ifndef _EM_JUNK_PLUGIN_H
#define _EM_JUNK_PLUGIN_H

#include <camel/camel-junk-plugin.h>
#include <gtk/gtkwidget.h>

#define EM_JUNK_PLUGIN(x) ((EMJunkPlugin *) x)

typedef struct _EMJunkPlugin EMJunkPlugin;

struct _EMJunkPlugin
{
	CamelJunkPlugin csp;

	/* when called, it should insert own GUI configuration into supplied.
	   container. returns data pointer which is later passed to apply,
	   plugin has to call (*changed_cb) (); whenever configuration
	   is changed to notify settings dialog about a change.
	   if setup_config_ui is NULL, it means there are no options */

	gpointer (*setup_config_ui) (GtkWidget *container, void (*changed_cb) ());
	void     (*apply)           (gpointer data);
};

#endif
