/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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


#ifndef __MAIL_FORMAT_H__
#define __MAIL_FORMAT_H__

#include <camel/camel.h>
#include <gtkhtml/gtkhtml.h>

#include "mail-display.h"
#include "mail-display-stream.h"

GByteArray *mail_format_get_data_wrapper_text (CamelDataWrapper *data,
					       MailDisplay *mail_display);

ssize_t mail_format_data_wrapper_write_to_stream (CamelDataWrapper *wrapper,
						  gboolean decode,
						  MailDisplay *mail_display,
						  CamelStream *stream);

void mail_format_mime_message (CamelMimeMessage *mime_message,
			       MailDisplay *md, MailDisplayStream *stream);
void mail_format_raw_message (CamelMimeMessage *mime_message,
			      MailDisplay *md, MailDisplayStream *stream);

gboolean mail_content_loaded (CamelDataWrapper *wrapper,
			      MailDisplay *display,
			      gboolean redisplay,
			      const char *url,
			      GtkHTML *html,
			      GtkHTMLStream *handle);

typedef gboolean (*MailMimeHandlerFn) (CamelMimePart *part, const char *mime_type,
				       MailDisplay *md, MailDisplayStream *stream);
typedef struct {
	Bonobo_ServerInfo *component;
	GList *applications;
	MailMimeHandlerFn builtin;
	guint generic : 1;
	guint is_bonobo : 1;
} MailMimeHandler;

MailMimeHandler *mail_lookup_handler (const char *mime_type);

gboolean mail_part_is_inline (CamelMimePart *part);
gboolean mail_part_is_displayed_inline (CamelMimePart *part, MailDisplay *md);
void     mail_part_toggle_displayed (CamelMimePart *part, MailDisplay *md);

char *mail_get_message_body (CamelDataWrapper *data, gboolean want_plain, gboolean cite);

#endif /* __MAIL_FORMAT_H__ */
