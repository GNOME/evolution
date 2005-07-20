/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-attachment-bar.h
 *
 * Copyright (C) 2005  Novell, Inc.
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
 * 	   Srinivasa Ragavan
 */

#ifndef __E_ATTACHMENT_BAR_H__
#define __E_ATTACHMENT_BAR_H__

#include <libgnomeui/gnome-icon-list.h>

#include <camel/camel-multipart.h>
#include "e-attachment.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_ATTACHMENT_BAR \
	(e_attachment_bar_get_type ())
#define E_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_ATTACHMENT_BAR, EAttachmentBar))
#define E_ATTACHMENT_BAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_ATTACHMENT_BAR, EAttachmentBarClass))
#define E_IS_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_ATTACHMENT_BAR))
#define E_IS_ATTACHMENT_BAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_ATTACHMENT_BAR))

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
	
	void (* changed) (EAttachmentBar *bar);
};


GtkType e_attachment_bar_get_type (void);

GtkWidget *e_attachment_bar_new (GtkAdjustment *adj);
void e_attachment_bar_to_multipart (EAttachmentBar *bar, CamelMultipart *multipart,
						 const char *default_charset);
guint e_attachment_bar_get_num_attachments (EAttachmentBar *bar);
void e_attachment_bar_attach (EAttachmentBar *bar, const char *file_name, char *disposition);
void e_attachment_bar_attach_mime_part (EAttachmentBar *bar, CamelMimePart *part);
int e_attachment_bar_get_download_count (EAttachmentBar *bar);
void e_attachment_bar_attach_remote_file (EAttachmentBar *bar,const gchar *url);
GSList *e_attachment_bar_get_attachment (EAttachmentBar *bar, int id);
void e_attachment_bar_add_attachment (EAttachmentBar *bar, EAttachment *attachment);
void e_attachment_bar_edit_selected (EAttachmentBar *bar);
void e_attachment_bar_remove_selected (EAttachmentBar *bar);
GtkWidget ** e_attachment_bar_get_selector(EAttachmentBar *bar);
GSList *e_attachment_bar_get_parts (EAttachmentBar *bar);
GSList *e_attachment_bar_get_selected (EAttachmentBar *bar);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_ATTACHMENT_BAR_H__ */
