/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-attachment.h
 *
 * Copyright (C) 1999  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <gnome.h>
#include <glade/glade-xml.h>
#include <camel/camel-mime-part.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_MSG_COMPOSER_ATTACHMENT			(e_msg_composer_attachment_get_type ())
#define E_MSG_COMPOSER_ATTACHMENT(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT, EMsgComposerAttachment))
#define E_MSG_COMPOSER_ATTACHMENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_MSG_COMPOSER_ATTACHMENT, EMsgComposerAttachmentClass))
#define E_IS_MSG_COMPOSER_ATTACHMENT(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT))
#define E_IS_MSG_COMPOSER_ATTACHMENT_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT))


typedef struct _EMsgComposerAttachment       EMsgComposerAttachment;
typedef struct _EMsgComposerAttachmentClass  EMsgComposerAttachmentClass;

struct _EMsgComposerAttachment {
	GtkObject parent;

	GladeXML *editor_gui;

	CamelMimePart *body;
	gboolean guessed_type;
	gulong size;
};

struct _EMsgComposerAttachmentClass {
	GtkObjectClass parent_class;

	void (*changed)	(EMsgComposerAttachment *msg_composer_attachment);
};


GtkType e_msg_composer_attachment_get_type (void);
EMsgComposerAttachment *e_msg_composer_attachment_new (const gchar *file_name);
EMsgComposerAttachment *e_msg_composer_attachment_new_from_mime_part (CamelMimePart *part);
void e_msg_composer_attachment_edit (EMsgComposerAttachment *attachment,
				     GtkWidget *parent);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_MSG_COMPOSER_ATTACHMENT_H__ */
