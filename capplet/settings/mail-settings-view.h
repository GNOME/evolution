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
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 2009 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _MAIL_SETTINGS_VIEW_H_
#define _MAIL_SETTINGS_VIEW_H_

#include <gtk/gtk.h>
#include "mail-view.h"

#define MAIL_SETTINGS_VIEW_TYPE        (mail_settings_view_get_type ())
#define MAIL_SETTINGS_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIL_SETTINGS_VIEW_TYPE, MailFolderView))
#define MAIL_SETTINGS_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), MAIL_SETTINGS_VIEW_TYPE, MailFolderViewClass))
#define IS_MAIL_SETTINGS_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIL_SETTINGS_VIEW_TYPE))
#define IS_MAIL_SETTINGS_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MAIL_SETTINGS_VIEW_TYPE))
#define MAIL_SETTINGS_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), MAIL_SETTINGS_VIEW_TYPE, MailFolderViewClass))

typedef struct _MailSettingsViewPrivate MailSettingsViewPrivate;

typedef struct _MailSettingsView {
	GtkVBox parent;
	gint type;
	const gchar *uri;
	MailViewFlags flags;
	/* Base class of MailChildView ends */

	MailSettingsViewPrivate *priv;
} MailSettingsView;

typedef struct _MailSettingsViewClass {
	GtkVBoxClass parent_class;

	void (* view_close) (MailSettingsView *);
	void (* show_account) (MailSettingsView *, gpointer);
} MailSettingsViewClass;

GType mail_settings_view_get_type (void);
MailSettingsView *mail_settings_view_new (void);
GtkWidget * mail_settings_view_get_tab_widget(MailSettingsView *mcv);
void mail_settings_view_activate (MailSettingsView *mcv, GtkWidget *tree, GtkWidget *folder_tree, GtkWidget *check_mail, GtkWidget *sort_by, GtkWidget *slider, gboolean act);
#endif
