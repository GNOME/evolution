/*--------------------------------*-C-*---------------------------------*
 *
 * Author :
 *  Matt Loper <matt@helixcode.com>
 *
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com) .
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 *
 *----------------------------------------------------------------------*/

#ifndef CAMEL_FORMATTER_H
#define CAMEL_FORMATTER_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel.h"

#define CAMEL_FORMATTER_TYPE     (camel_formatter_get_type ())
#define CAMEL_FORMATTER(obj)     (GTK_CHECK_CAST((obj), CAMEL_FORMATTER_TYPE, CamelFormatter))
#define CAMEL_FORMATTER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_FORMATTER_TYPE, CamelFormatterClass))
#define CAMEL_IS_CAMEL_FORMATTER(o)    (GTK_CHECK_TYPE((o), CAMEL_FORMATTER_TYPE))

typedef struct _CamelFormatterPrivate CamelFormatterPrivate;

typedef struct _CamelFormatter CamelFormatter;

struct _CamelFormatter
{
	GtkObject parent_object;
	CamelFormatterPrivate *priv;
};

typedef struct {
	GtkObjectClass parent_class;
} CamelFormatterClass;


/* Standard Gtk function */
GtkType  camel_formatter_get_type (void);

/* Public functions */
CamelFormatter* camel_formatter_new (void);

/* The main job of CamelFormatter is to take a mime message, and
   produce html from it. */
void camel_formatter_mime_message_to_html (CamelFormatter* formatter,
					   CamelMimeMessage* mime_message,
					   CamelStream* header_stream,
					   CamelStream* body_stream);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // CAMEL_FORMATTER_H

