/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-attachment.h
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
#ifndef __E_MSG_COMPOSER_ATTACHMENT_H__
#define __E_MSG_COMPOSER_ATTACHMENT_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glade/glade-xml.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-exception.h>
#include <libgnomevfs/gnome-vfs.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_MSG_COMPOSER_ATTACHMENT			(e_msg_composer_attachment_get_type ())
#define E_MSG_COMPOSER_ATTACHMENT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT, EMsgComposerAttachment))
#define E_MSG_COMPOSER_ATTACHMENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MSG_COMPOSER_ATTACHMENT, EMsgComposerAttachmentClass))
#define E_IS_MSG_COMPOSER_ATTACHMENT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT))
#define E_IS_MSG_COMPOSER_ATTACHMENT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT))


typedef struct _EMsgComposerAttachment       EMsgComposerAttachment;
typedef struct _EMsgComposerAttachmentClass  EMsgComposerAttachmentClass;

struct _EMsgComposerAttachment {
	GObject parent;

	GladeXML *editor_gui;

	CamelMimePart *body;
	gboolean guessed_type;
	gulong size;

	GdkPixbuf *pixbuf_cache;
	
	GnomeVFSAsyncHandle *handle;
	gboolean is_available_local;
	char *file_name;
	char *description;
	gboolean disposition;
	int index;
};

struct _EMsgComposerAttachmentClass {
	GObjectClass parent_class;

	void (*changed)	(EMsgComposerAttachment *msg_composer_attachment);
};


GType e_msg_composer_attachment_get_type (void);
EMsgComposerAttachment *e_msg_composer_attachment_new (const char *file_name,
						       const char *disposition,
						       CamelException *ex);
EMsgComposerAttachment * e_msg_composer_attachment_new_remote_file (const char *file_name,
						       		    const char *disposition,
			       		   			    CamelException *ex);
void e_msg_composer_attachment_build_remote_file (const char *filename,
						  EMsgComposerAttachment *attachment,
					   	  const char *disposition,
					   	  CamelException *ex);
EMsgComposerAttachment *e_msg_composer_attachment_new_from_mime_part (CamelMimePart *part);
void e_msg_composer_attachment_edit (EMsgComposerAttachment *attachment,
				     GtkWidget *parent);
				     

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_MSG_COMPOSER_ATTACHMENT_H__ */
