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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _MAIL_DECORATION_
#define _MAIL_DECORATION_ 

#include <gtk/gtk.h>

#define MAIL_DECORATION_TYPE \
	(mail_decoration_get_type ())
#define MAIL_DECORATION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), MAIL_DECORATION_TYPE, MailDecoration))
#define MAIL_DECORATION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), MAIL_DECORATION_TYPE, MailDecorationClass))
#define IS_MAIL_DECORATION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), MAIL_DECORATION_TYPE))
#define IS_MAIL_DECORATION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), MAIL_DECORATION_TYPE))
#define MAIL_DECORATION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), MAIL_DECORATION_TYPE, MailDecorationClass))

typedef struct _MailDecoration MailDecoration;
typedef struct _MailDecorationClass MailDecorationClass;
typedef struct _MailDecorationPrivate MailDecorationPrivate;

struct _MailDecoration {
        GObject parent;

	GtkWindow *window;
	MailDecorationPrivate *priv;
};

struct _MailDecorationClass {
	GObjectClass parent_class;
};

GType		mail_decoration_get_type	(void);
MailDecoration *mail_decoration_new		(GtkWindow *);

#endif
