/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Sivaiah Nallagatla <snallagatla@novell.com>
 *  Copyright (C) 2004 Novell, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */


#include "camel-gw-listener.h"
#include <gtk/gtk.h>
#include "mail/em-config.h"

static CamelGwListener *config_listener = NULL;

int e_plugin_lib_enable (EPluginLib *ep, int enable);
GtkWidget* org_gnome_gw_account_setup(struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);

static void 
free_groupwise_listener ( void )
{
	g_object_unref (config_listener);
}

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	if (!config_listener) {
		config_listener = camel_gw_listener_new ();	
	 	g_atexit ( free_groupwise_listener );
	}

	return 0;
}


GtkWidget *
org_gnome_gw_account_setup(struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data)
{
	GtkWidget *w;
  
	if (data->old)
		return data->old;
        /* FIXME, with new soap camel provider we don't need extra settings in receiving options page, Remove them
	   from camel-groupwise-provider.c once soap provider is ready and add any groupwise sepcific settings like "add contacts automatically to Frequent contacts folder" here*/
       
	w = gtk_invisible_new ();
	return w;
}
