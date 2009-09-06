/*
 * e-mail-display.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_DISPLAY_H
#define E_MAIL_DISPLAY_H

#include <mail/em-format-html.h>
#include <misc/e-web-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_DISPLAY \
	(e_mail_display_get_type ())
#define E_MAIL_DISPLAY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_DISPLAY, EMailDisplay))
#define E_MAIL_DISPLAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_DISPLAY, EMailDisplayClass))
#define E_IS_MAIL_DISPLAY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_DISPLAY))
#define E_IS_MAIL_DISPLAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_DISPLAY))
#define E_MAIL_DISPLAY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_DISPLAY, EMailDisplayClass))

G_BEGIN_DECLS

typedef struct _EMailDisplay EMailDisplay;
typedef struct _EMailDisplayClass EMailDisplayClass;
typedef struct _EMailDisplayPrivate EMailDisplayPrivate;

struct _EMailDisplay {
	EWebView parent;
	EMailDisplayPrivate *priv;
};

struct _EMailDisplayClass {
	EWebViewClass parent_class;
};

GType		e_mail_display_get_type		(void);
EMFormatHTML *	e_mail_display_get_formatter	(EMailDisplay *display);
void		e_mail_display_set_formatter	(EMailDisplay *display,
						 EMFormatHTML *formatter);

G_END_DECLS

#endif /* E_MAIL_DISPLAY_H */
