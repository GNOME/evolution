/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* msg-composer-attachment-bar.h
 *
 * Copyright (C) 2004  Novell, Inc.
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
 * Author: Harish Krishnaswamy <kharish@novell.com>
 */

#ifndef __CAL_ATTACHMENT_BAR_H__
#define __CAL_ATTACHMENT_BAR_H__

#include <libgnomeui/gnome-icon-list.h>
#include <camel/camel-multipart.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_CAL_ATTACHMENT_BAR \
	(cal_attachment_bar_get_type ())
#define CAL_ATTACHMENT_BAR(obj)   \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_ATTACHMENT_BAR, CalAttachmentBar))
#define CAL_ATTACHMENT_BAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CAL_ATTACHMENT_BAR, CalAttachmentBarClass))
#define E_IS_CAL_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_ATTACHMENT_BAR))
#define E_IS_CAL_ATTACHMENT_BAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CAL_ATTACHMENT_BAR))

typedef struct _CalAttachmentBar CalAttachmentBar;
typedef struct _CalAttachmentBarClass CalAttachmentBarClass;
typedef struct _CalAttachmentBarPrivate CalAttachmentBarPrivate;

struct _CalAttachmentBar {
	GnomeIconList parent;
	
	CalAttachmentBarPrivate *priv;
};

struct _CalAttachmentBarClass {
	GnomeIconListClass parent_class;
	
	void (* changed) (CalAttachmentBar *bar);
};


GtkType cal_attachment_bar_get_type (void);

GtkWidget *cal_attachment_bar_new (GtkAdjustment *adj);
void cal_attachment_bar_to_multipart (CalAttachmentBar *bar, CamelMultipart *multipart,
						 const char *default_charset);
guint cal_attachment_bar_get_num_attachments (CalAttachmentBar *bar);
void cal_attachment_bar_attach (CalAttachmentBar *bar, const char *file_name);
void cal_attachment_bar_attach_mime_part (CalAttachmentBar *bar, CamelMimePart *part);
GSList *cal_attachment_bar_get_attachment_list (CalAttachmentBar *bar);
char * cal_attachment_bar_get_nth_attachment_filename (CalAttachmentBar *bar, int n);
GSList *cal_attachment_bar_get_mime_attach_list (CalAttachmentBar *bar);
void cal_attachment_bar_set_attachment_list (CalAttachmentBar *bar, GSList *attach_list);
void cal_attachment_bar_set_local_attachment_store (CalAttachmentBar *bar, 
		const char *attachment_store);
void cal_attachment_bar_set_comp_uid (CalAttachmentBar *bar, char *comp_uid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAL_ATTACHMENT_BAR_H__ */
