/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer.h
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

#ifndef ___E_MSG_COMPOSER_H__
#define ___E_MSG_COMPOSER_H__

#include <gnome.h>
#include <glade/glade.h>

#include "e-msg-composer-attachment-bar.h"
#include "e-msg-composer-hdrs.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define E_TYPE_MSG_COMPOSER	       (e_msg_composer_get_type ())
#define E_MSG_COMPOSER(obj)	       (GTK_CHECK_CAST ((obj), E_TYPE_MSG_COMPOSER, EMsgComposer))
#define E_MSG_COMPOSER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_MSG_COMPOSER, EMsgComposerClass))
#define E_IS_MSG_COMPOSER(obj)	       (GTK_CHECK_TYPE ((obj), E_TYPE_MSG_COMPOSER))
#define E_IS_MSG_COMPOSER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_MSG_COMPOSER))


typedef struct _EMsgComposer       EMsgComposer;
typedef struct _EMsgComposerClass  EMsgComposerClass;

struct _EMsgComposer {
	GnomeApp parent;

	GtkWidget *hdrs;

	GtkWidget *text;
	GtkWidget *text_scrolled_window;

	GtkWidget *attachment_bar;
	GtkWidget *attachment_scrolled_window;

	GtkWidget *address_dialog;

	gboolean attachment_bar_visible : 1;
};

struct _EMsgComposerClass {
	GnomeAppClass parent_class;

	void (* send) (EMsgComposer *composer);
	void (* postpone) (EMsgComposer *composer);
};


GtkType e_msg_composer_get_type (void);
void e_msg_composer_construct (EMsgComposer *composer);
GtkWidget *e_msg_composer_new (void);
void e_msg_composer_show_attachments (EMsgComposer *composer, gboolean show);
CamelMimeMessage *e_msg_composer_get_message (EMsgComposer *composer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ___E_MSG_COMPOSER_H__ */
