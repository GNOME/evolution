/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Matt Loper <matt@helixcode.com>
 *
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com)
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
 */

#ifndef MAIL_FORMAT_H
#define MAIL_FORMAT_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtkhtml/gtkhtml.h>
#include "camel/camel.h"
#include "composer/e-msg-composer.h"

void mail_format_mime_message (CamelMimeMessage *mime_message,
			       GtkHTMLStreamHandle *header_stream,
			       GtkHTMLStreamHandle *body_stream);

void mail_write_html (GtkHTMLStreamHandle *stream, const char *data);

EMsgComposer *mail_generate_reply (CamelMimeMessage *mime_message,
				   gboolean to_all);

EMsgComposer *mail_generate_forward (CamelMimeMessage *mime_message,
				     gboolean forward_as_attachment,
				     gboolean keep_attachments);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // CAMEL_FORMATTER_H

