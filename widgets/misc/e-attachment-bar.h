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
 *		Ettore Perazzoli <ettore@ximian.com>
 * 	   	Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_ATTACHMENT_BAR_H
#define E_ATTACHMENT_BAR_H

#include <gtk/gtk.h>
#include <libgnomeui/gnome-icon-list.h>

#include <camel/camel-multipart.h>
#include "e-attachment.h"

#define E_TYPE_ATTACHMENT_BAR \
	(e_attachment_bar_get_type ())
#define E_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_BAR, EAttachmentBar))
#define E_ATTACHMENT_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_BAR, EAttachmentBarClass))
#define E_IS_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_BAR))
#define E_IS_ATTACHMENT_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_BAR))
#define E_ATTACHMENT_BAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_BAR, EAttachmentBarClass))

G_BEGIN_DECLS

typedef struct _EAttachmentBar EAttachmentBar;
typedef struct _EAttachmentBarClass EAttachmentBarClass;
typedef struct _EAttachmentBarPrivate EAttachmentBarPrivate;

struct _EAttachmentBar {
	GnomeIconList parent;
	gboolean expand;
	EAttachmentBarPrivate *priv;
};

struct _EAttachmentBarClass {
	GnomeIconListClass parent_class;

	/* Signals */
	void		(*changed)		(EAttachmentBar *bar);
	void		(*update_actions)	(EAttachmentBar *bar);
};

GType		e_attachment_bar_get_type	(void);
GtkWidget *	e_attachment_bar_new		(void);
void		e_attachment_bar_to_multipart	(EAttachmentBar *bar,
						 CamelMultipart *multipart,
						 const gchar *default_charset);
guint		e_attachment_bar_get_num_attachments
						(EAttachmentBar *bar);
void		e_attachment_bar_attach		(EAttachmentBar *bar,
						 const gchar *filename,
						 const gchar *disposition);
void		e_attachment_bar_attach_mime_part
						(EAttachmentBar *bar,
						 CamelMimePart *part);
gint		e_attachment_bar_get_download_count
						(EAttachmentBar *bar);
void		e_attachment_bar_attach_remote_file
						(EAttachmentBar *bar,
						 const gchar *url,
						 const gchar *disposition);
GSList *	e_attachment_bar_get_attachment	(EAttachmentBar *bar,
						 gint id);
void		e_attachment_bar_add_attachment	(EAttachmentBar *bar,
						 EAttachment *attachment);
GSList *	e_attachment_bar_get_parts	(EAttachmentBar *bar);
GSList *	e_attachment_bar_get_selected	(EAttachmentBar *bar);
void		e_attachment_bar_set_width	(EAttachmentBar *bar,
						 gint bar_width);
GSList *	e_attachment_bar_get_all_attachments
						(EAttachmentBar *bar);
void		e_attachment_bar_create_attachment_cache
						(EAttachment *attachment);
GtkAction *	e_attachment_bar_recent_action_new
						(EAttachmentBar *bar, 
						 const gchar *action_name,
						 const gchar *action_label);
void		e_attachment_bar_add_attachment_silent
						(EAttachmentBar *bar,
						 EAttachment *attachment);
void		e_attachment_bar_refresh	(EAttachmentBar *bar);
gint		e_attachment_bar_file_chooser_dialog_run
						(EAttachmentBar *attachment_bar,
						 GtkWidget *dialog);
void		e_attachment_bar_update_actions	(EAttachmentBar *attachment_bar);
const gchar *	e_attachment_bar_get_background_filename
						(EAttachmentBar *attachment_bar);
void		e_attachment_bar_set_background_filename
						(EAttachmentBar *attachment_bar,
						 const gchar *background_filename);
const gchar *	e_attachment_bar_get_background_options
						(EAttachmentBar *attachment_bar);
void		e_attachment_bar_set_background_options
						(EAttachmentBar *attachment_bar,
						 const gchar *background_options);
const gchar *	e_attachment_bar_get_current_folder
						(EAttachmentBar *attachment_bar);
void		e_attachment_bar_set_current_folder
						(EAttachmentBar *attachment_bar,
						 const gchar *current_folder);
gboolean	e_attachment_bar_get_editable	(EAttachmentBar *attachment_bar);
void		e_attachment_bar_set_editable	(EAttachmentBar *attachment_bar,
						 gboolean editable);
GtkUIManager *	e_attachment_bar_get_ui_manager	(EAttachmentBar *attachment_bar);
GtkAction *	e_attachment_bar_get_action	(EAttachmentBar *attachment_bar,
						 const gchar *action_name);
GtkActionGroup *e_attachment_bar_get_action_group
						(EAttachmentBar *attachment_bar,
						 const gchar *group_name);

G_END_DECLS

#endif /* E_ATTACHMENT_BAR_H */
