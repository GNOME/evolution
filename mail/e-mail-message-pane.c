/*
 * e-mail-browser.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-message-pane.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/gconf-bridge.h"

#include "mail/e-mail-reader.h"

#define e_mail_message_pane_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_MESSAGE_PANE, EMailMessagePanePrivate))


struct _EMailMessagePanePrivate {
	int fo;
};

enum {
	PROP_0,
	PROP_PREVIEW_VISIBLE
};

static gpointer parent_class;

/* This is too trivial to put in a file.
 * It gets merged with the EMailReader UI. */
static void
mail_message_pane_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			return;
		
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_message_pane_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value,
				TRUE);
		return;			
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_message_pane_dispose (GObject *object)
{
	EMailMessagePanePrivate *priv;

	priv = e_mail_message_pane_GET_PRIVATE (object);


	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_message_pane_constructed (GObject *object)
{
	EMailMessagePanePrivate *priv;
		
	/* Chain up to parent's constructed() method. */
	if (G_OBJECT_CLASS (parent_class)->constructed)
		G_OBJECT_CLASS (parent_class)->constructed (object);

	priv = e_mail_message_pane_GET_PRIVATE (object);

	gtk_widget_hide (e_mail_reader_get_message_list (E_MAIL_READER(object)));
	e_mail_paned_view_hide_message_list_pane (E_MAIL_PANED_VIEW(object), FALSE);
}

static void
message_pane_set_preview_visible (EMailMessagePane *view,
                                          gboolean preview_visible)
{
	e_mail_paned_view_set_preview_visible (E_MAIL_PANED_VIEW(view), TRUE);

	return;
}

static gboolean
message_pane_get_preview_visible (EMailMessagePane *view)
{

	return TRUE;
}

static void
mail_message_pane_class_init (EMailMessagePaneClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailMessagePanePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_message_pane_set_property;
	object_class->get_property = mail_message_pane_get_property;
	object_class->dispose = mail_message_pane_dispose;
	object_class->constructed = mail_message_pane_constructed;

	E_MAIL_VIEW_CLASS(g_type_class_peek_parent(class))->set_preview_visible = message_pane_set_preview_visible;
	E_MAIL_VIEW_CLASS(g_type_class_peek_parent(class))->get_preview_visible = message_pane_get_preview_visible;

	g_object_class_override_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		"preview-visible");
}

static void
mail_message_pane_init (EMailMessagePane *browser)
{

	browser->priv = e_mail_message_pane_GET_PRIVATE (browser);
}

GType
e_mail_message_pane_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailMessagePaneClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_message_pane_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailMessagePane),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_message_pane_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_MAIL_PANED_VIEW_TYPE , "EMailMessagePane", &type_info, 0);

	}

	return type;
}

GtkWidget *
e_mail_message_pane_new (EShellContent *content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (content), NULL);

	return g_object_new (
		E_TYPE_MAIL_MESSAGE_PANE,
		"shell-content", content, 
		"preview-visible", TRUE,
		NULL);
}
