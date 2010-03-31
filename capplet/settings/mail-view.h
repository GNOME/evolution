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

#ifndef _MAIL_VIEW_H_
#define _MAIL_VIEW_H_

#include <shell/e-shell-view.h>

#include "anjal-mail-view.h"

#define MAIL_VIEW_TYPE        (mail_view_get_type ())
#define MAIL_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIL_VIEW_TYPE, MailView))
#define MAIL_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), MAIL_VIEW_TYPE, MailViewClass))
#define IS_MAIL_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIL_VIEW_TYPE))
#define IS_MAIL_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MAIL_VIEW_TYPE))
#define MAIL_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), MAIL_VIEW_TYPE, MailViewClass))

enum {
	MAIL_VIEW_FOLDER=1,
	MAIL_VIEW_COMPOSER,
	MAIL_VIEW_MESSAGE,
	MAIL_VIEW_ACCOUNT,
	MAIL_VIEW_SETTINGS,
	MAIL_VIEW_PEOPLE
};

typedef struct _MailViewPrivate MailViewPrivate;

typedef struct _MailView {
	AnjalMailView parent;

	GtkWidget *tree; /* Actual tree */
	GtkWidget *folder_tree;
	GtkWidget *slider;
	GtkWidget *check_mail;
	GtkWidget *sort_by;
	MailViewPrivate *priv;
	EShellView *shell_view;
} MailView;

typedef struct _MailViewClass {
	AnjalMailViewClass parent_class;
	void (* view_new) (MailView*);

} MailViewClass;

typedef enum {
	 MAIL_VIEW_HOLD_FOCUS=1,
} MailViewFlags;

typedef struct _MailViewChild {
	GtkVBox parent;
	gint type;
	gchar *uri;
	MailViewFlags flags;
}MailViewChild;

GType mail_view_get_type (void);
MailView * mail_view_new (void);
void  mail_view_set_folder_uri (MailView *mv, const gchar *uri);
void mail_view_show_sort_popup (MailView *mv, GtkWidget *);
void mail_view_show_list (MailView *mv);
void mail_view_close_view (MailView *mv);
void mail_view_set_check_email (MailView *mv, GtkWidget *button);
void mail_view_set_sort_by  (MailView *mv, GtkWidget *button);
void mail_view_check_mail(MailView *mv, gboolean deep);
void mail_view_set_folder_tree_widget (MailView *mv, GtkWidget *tree);
void mail_view_set_folder_tree (MailView *mv, GtkWidget *tree);
void mail_view_save (MailView *mv);
MailViewChild * mail_view_add_page (MailView *mv, guint16 type, gpointer data);
void mail_view_set_search (MailView *view, const gchar *search);
void mail_view_set_slider (MailView *mv, GtkWidget *slider);
void mail_view_init_search (MailView *mv, GtkWidget *search);
void mail_view_switch_to_people (MailView* mv, MailViewChild *mpv);
void mail_view_switch_to_settings (MailView* mv, MailViewChild *mpv);
void mail_view_set_search_entry (MailView *mv, GtkWidget *entry);
void mail_view_set_shell_view (MailView *mv, EShellView *shell);

#endif
