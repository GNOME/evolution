/*
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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


#include <glib.h>
#include <glib/gi18n.h>
#include "e-mail-view.h"


G_DEFINE_TYPE (EMailView, e_mail_view, GTK_TYPE_VBOX)

enum {
	PANE_CLOSE,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_SHELL_VIEW
};

struct _EMailViewPrivate {
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
e_mail_view_init (EMailView  *shell)
{
	shell->priv = g_new0(EMailViewPrivate, 1);
}

static void
e_mail_view_finalize (GObject *object)
{
	/* EMailView *shell = (EMailView *)object; */

	G_OBJECT_CLASS (e_mail_view_parent_class)->finalize (object);
}

static void
mail_view_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
		        E_MAIL_VIEW(object)->content = g_value_get_object (value);
		
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_view_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			g_value_set_object (
			    value, E_MAIL_VIEW(object)->content);
			
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_view_class_init (EMailViewClass *klass)
{
	GObjectClass * object_class = G_OBJECT_CLASS (klass);

	e_mail_view_parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = e_mail_view_finalize;
	object_class->set_property = mail_view_set_property;
	object_class->get_property = mail_view_get_property;

	signals[PANE_CLOSE] =
		g_signal_new ("pane-close",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMailViewClass , pane_close),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	/**
	 * EMailView:shell-content
	 *
	 * The #EShellContent to which the widget belongs.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-content",
			NULL,
			NULL,
			E_TYPE_SHELL_CONTENT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

}


