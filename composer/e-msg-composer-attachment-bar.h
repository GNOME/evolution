/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* msg-composer-attachment-bar.h
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

#ifndef __E_MSG_COMPOSER_ATTACHMENT_BAR_H__
#define __E_MSG_COMPOSER_ATTACHMENT_BAR_H__

#include <gnome.h>
#include <camel/camel-multipart.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR \
	(e_msg_composer_attachment_bar_get_type ())
#define E_MSG_COMPOSER_ATTACHMENT_BAR(obj) \
	(GTK_CHECK_CAST ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR, EMsgComposerAttachmentBar))
#define E_MSG_COMPOSER_ATTACHMENT_BAR_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR, EMsgComposerAttachmentBarClass))
#define E_IS_MSG_COMPOSER_ATTACHMENT_BAR(obj) \
	(GTK_CHECK_TYPE ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR))
#define E_IS_MSG_COMPOSER_ATTACHMENT_BAR_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_MSG_COMPOSER_ATTACHMENT_BAR))


typedef struct _EMsgComposerAttachmentBarPrivate EMsgComposerAttachmentBarPrivate;

struct _EMsgComposerAttachmentBar {
	GnomeIconList parent;

	EMsgComposerAttachmentBarPrivate *priv;
};
typedef struct _EMsgComposerAttachmentBar       EMsgComposerAttachmentBar;

struct _EMsgComposerAttachmentBarClass {
	GnomeIconListClass parent_class;

	void (* changed) (EMsgComposerAttachmentBar *bar);
};
typedef struct _EMsgComposerAttachmentBarClass  EMsgComposerAttachmentBarClass;


GtkType e_msg_composer_attachment_bar_get_type (void);
GtkWidget *e_msg_composer_attachment_bar_new (GtkAdjustment *adj);
void e_msg_composer_attachment_bar_to_multipart (EMsgComposerAttachmentBar *bar, CamelMultipart *multipart);
guint e_msg_composer_attachment_bar_get_num_attachments (EMsgComposerAttachmentBar *bar);
void e_msg_composer_attachment_bar_attach (EMsgComposerAttachmentBar *bar, const gchar *file_name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_MSG_COMPOSER_ATTACHMENT_BAR_H__ */
