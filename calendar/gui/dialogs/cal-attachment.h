/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* cal-attachment.h
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
 * Author: Harish Krishnaswamy
 */
#ifndef __CAL_ATTACHMENT_H__
#define __CAL_ATTACHMENT_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glade/glade-xml.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-exception.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_CAL_ATTACHMENT			(cal_attachment_get_type ())
#define CAL_ATTACHMENT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_ATTACHMENT, CalAttachment))
#define CAL_ATTACHMENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CAL_ATTACHMENT, CalAttachmentClass))
#define E_IS_CAL_ATTACHMENT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_ATTACHMENT))
#define E_IS_CAL_ATTACHMENT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CAL_ATTACHMENT))

typedef struct _CalAttachment       CalAttachment;
typedef struct _CalAttachmentClass  CalAttachmentClass;

struct _CalAttachment {
	GObject parent;

	GladeXML *editor_gui;

	CamelMimePart *body;
	gboolean guessed_type;
	gulong size;

	GdkPixbuf *pixbuf_cache;
};

struct _CalAttachmentClass {
	GObjectClass parent_class;

	void (*changed)	(CalAttachment *cal_attachment);
};

struct CalMimeAttach {
	char *filename;
	char *content_type;
	char *description;
	char *encoded_data;
	guint length;
};

GType cal_attachment_get_type (void);
CalAttachment *cal_attachment_new (const char *file_name, const char *disposition, CamelException *ex);
CalAttachment *cal_attachment_new_from_mime_part (CamelMimePart *part);
void cal_attachment_edit (CalAttachment *attachment, GtkWidget *parent);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAL_ATTACHMENT_H__ */
