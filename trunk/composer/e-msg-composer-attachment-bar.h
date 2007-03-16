/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* msg-composer-attachment-bar.h
 *
 * Copyright (C) 1999  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef __E_MSG_COMPOSER_ATTACHMENT_BAR_H__
#define __E_MSG_COMPOSER_ATTACHMENT_BAR_H__

#include <libgnomeui/gnome-icon-list.h>

#include <camel/camel-multipart.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR \
	(e_msg_composer_attachment_bar_get_type ())
#define E_MSG_COMPOSER_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR, EMsgComposerAttachmentBar))
#define E_MSG_COMPOSER_ATTACHMENT_BAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR, EMsgComposerAttachmentBarClass))
#define E_IS_MSG_COMPOSER_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR))
#define E_IS_MSG_COMPOSER_ATTACHMENT_BAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR))

typedef struct _EMsgComposerAttachmentBar EMsgComposerAttachmentBar;
typedef struct _EMsgComposerAttachmentBarClass EMsgComposerAttachmentBarClass;
typedef struct _EMsgComposerAttachmentBarPrivate EMsgComposerAttachmentBarPrivate;

struct _EMsgComposerAttachmentBar {
	GnomeIconList parent;
	
	EMsgComposerAttachmentBarPrivate *priv;
};

struct _EMsgComposerAttachmentBarClass {
	GnomeIconListClass parent_class;
	
	void (* changed) (EMsgComposerAttachmentBar *bar);
};


GtkType e_msg_composer_attachment_bar_get_type (void);

GtkWidget *e_msg_composer_attachment_bar_new (GtkAdjustment *adj);
void e_msg_composer_attachment_bar_to_multipart (EMsgComposerAttachmentBar *bar, CamelMultipart *multipart,
						 const char *default_charset);
guint e_msg_composer_attachment_bar_get_num_attachments (EMsgComposerAttachmentBar *bar);
void e_msg_composer_attachment_bar_attach (EMsgComposerAttachmentBar *bar, const char *file_name);
void e_msg_composer_attachment_bar_attach_mime_part (EMsgComposerAttachmentBar *bar, CamelMimePart *part);
int e_msg_composer_attachment_bar_get_download_count (EMsgComposerAttachmentBar *bar);
void e_msg_composer_attachment_bar_attach_remote_file (EMsgComposerAttachmentBar *bar,const gchar *url);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_MSG_COMPOSER_ATTACHMENT_BAR_H__ */
