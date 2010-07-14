/*
 * e-mail-folder-pane.c
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

#include "e-mail-folder-pane.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-plugin-ui.h"
#include "e-util/gconf-bridge.h"
#include "shell/e-shell.h"
#include "shell/e-shell-utils.h"
#include "widgets/misc/e-popup-action.h"
#include "widgets/misc/e-preview-pane.h"

#include "mail/e-mail-reader.h"
#include "mail/e-mail-reader-utils.h"
#include "mail/em-folder-tree-model.h"
#include "mail/em-format-html-display.h"
#include "mail/message-list.h"

#define E_MAIL_FOLDER_PANE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_FOLDER_PANE, EMailFolderPanePrivate))

struct _EMailFolderPanePrivate {
	int fo;
};

enum {
	PROP_0,
	PROP_PREVIEW_VISIBLE,
};

static gpointer parent_class;


static void
mail_folder_pane_dispose (GObject *object)
{
	EMailFolderPanePrivate *priv;

	priv = E_MAIL_FOLDER_PANE_GET_PRIVATE (object);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_folder_pane_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	if (G_OBJECT_CLASS (parent_class)->constructed)
		G_OBJECT_CLASS (parent_class)->constructed (object);

}

static void
folder_pane_set_preview_visible (EMailPanedView *view,
                                          gboolean preview_visible)
{
	return;
}

static gboolean
folder_pane_get_preview_visible (EMailPanedView *view)
{

	return FALSE;
}

static void
mail_folder_pane_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value,
				FALSE);
			return;


	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_folder_pane_set_property (GObject *object,
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
mfp_open_selected_mail (EMailPanedView *view)
{
	GPtrArray *uids;
	int i;

	uids = e_mail_reader_get_selected_uids (E_MAIL_READER(view)); 
	for (i=0; i<uids->len; i++) {
		g_signal_emit_by_name (view, "open-mail", uids->pdata[i]);
	}
	
	printf("I WIN\n");
}

static void
mail_folder_pane_class_init (EMailPanedViewClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailFolderPanePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_folder_pane_dispose;
	object_class->constructed = mail_folder_pane_constructed;
	object_class->set_property = mail_folder_pane_set_property;
	object_class->get_property = mail_folder_pane_get_property;

	class->open_selected_mail = mfp_open_selected_mail;

	E_MAIL_VIEW_CLASS(g_type_class_peek_parent(class))->set_preview_visible = folder_pane_set_preview_visible;
	E_MAIL_VIEW_CLASS(g_type_class_peek_parent(class))->get_preview_visible = folder_pane_get_preview_visible;

	g_object_class_override_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		"preview-visible");

}


static void
mail_folder_pane_init (EMailFolderPane *browser)
{

	browser->priv = E_MAIL_FOLDER_PANE_GET_PRIVATE (browser);

}

GType
e_mail_folder_pane_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailFolderPaneClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_folder_pane_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailFolderPane),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_folder_pane_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_MAIL_PANED_VIEW_TYPE, "EMailFolderPane", &type_info, 0);

	}

	return type;
}

GtkWidget *
e_mail_folder_pane_new (EShellContent *content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (content), NULL);

	return g_object_new (
		E_TYPE_MAIL_FOLDER_PANE,
		"shell-content", content, 
		"preview-visible", FALSE,
		NULL);
}

