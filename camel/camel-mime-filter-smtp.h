/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#ifndef _CAMEL_MIME_FILTER_SMTP_H
#define _CAMEL_MIME_FILTER_SMTP_H

#include <gtk/gtk.h>
#include <camel/camel-mime-filter.h>

#define CAMEL_MIME_FILTER_SMTP(obj)         GTK_CHECK_CAST (obj, camel_mime_filter_smtp_get_type (), CamelMimeFilterSmtp)
#define CAMEL_MIME_FILTER_SMTP_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_mime_filter_smtp_get_type (), CamelMimeFilterSmtpClass)
#define IS_CAMEL_MIME_FILTER_SMTP(obj)      GTK_CHECK_TYPE (obj, camel_mime_filter_smtp_get_type ())

typedef struct _CamelMimeFilterSmtp      CamelMimeFilterSmtp;
typedef struct _CamelMimeFilterSmtpClass CamelMimeFilterSmtpClass;

struct _CamelMimeFilterSmtp {
	CamelMimeFilter parent;

	struct _CamelMimeFilterSmtpPrivate *priv;

	int midline;		/* are we between lines? */
};

struct _CamelMimeFilterSmtpClass {
	CamelMimeFilterClass parent_class;
};

guint		camel_mime_filter_smtp_get_type (void);
CamelMimeFilterFrom      *camel_mime_filter_smtp_new (void);

#endif /* ! _CAMEL_MIME_FILTER_SMTP_H */
