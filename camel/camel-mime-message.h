/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camelMimeMessage.h : class for a mime message
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef CAMEL_MIME_MESSAGE_H
#define CAMEL_MIME_MESSAGE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-mime-part.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-internet-address.h>

#define CAMEL_RECIPIENT_TYPE_TO "To"
#define CAMEL_RECIPIENT_TYPE_CC "Cc"
#define CAMEL_RECIPIENT_TYPE_BCC "Bcc"


#define CAMEL_MIME_MESSAGE_TYPE     (camel_mime_message_get_type ())
#define CAMEL_MIME_MESSAGE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_MIME_MESSAGE_TYPE, CamelMimeMessage))
#define CAMEL_MIME_MESSAGE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_MIME_MESSAGE_TYPE, CamelMimeMessageClass))
#define CAMEL_IS_MIME_MESSAGE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_MIME_MESSAGE_TYPE))


/* specify local time */
#define CAMEL_MESSAGE_DATE_CURRENT (~0)

struct _CamelMimeMessage
{
	CamelMimePart parent_object;

	/* header fields */
	time_t date;
	int date_offset;	/* GMT offset */
	char *date_str;		/* cached copy of date string */

	gchar *subject;
	gchar *reply_to;

	gchar *from;

	GHashTable *recipients;	/* hash table of CamelInternetAddress's */
};

typedef struct {
	CamelMimePartClass parent_class;

	/* Virtual methods */	

} CamelMimeMessageClass;



/* Standard Camel function */
CamelType camel_mime_message_get_type (void);


/* public methods */
CamelMimeMessage * camel_mime_message_new                  (void);


void               camel_mime_message_set_date		   (CamelMimeMessage *mime_message,  time_t date, int offset);
void               camel_mime_message_get_date		   (CamelMimeMessage *mime_message,  time_t *date, int *offset);
char		  *camel_mime_message_get_date_string	   (CamelMimeMessage *mime_message);

const gchar *      camel_mime_message_get_received_date    (CamelMimeMessage *mime_message);
const gchar *      camel_mime_message_get_sent_date        (CamelMimeMessage *mime_message);
void               camel_mime_message_set_reply_to         (CamelMimeMessage *mime_message, 
							    const gchar *reply_to);
const gchar *      camel_mime_message_get_reply_to         (CamelMimeMessage *mime_message);
void               camel_mime_message_set_subject          (CamelMimeMessage *mime_message, 
							    const gchar *subject);
const gchar *      camel_mime_message_get_subject          (CamelMimeMessage *mime_message);
void               camel_mime_message_set_from             (CamelMimeMessage *mime_message, 
							    const gchar *from);
const gchar *      camel_mime_message_get_from             (CamelMimeMessage *mime_message);


void		camel_mime_message_add_recipient (CamelMimeMessage *mime_message,
						  const char *type, const char *name, const char *address);
void		camel_mime_message_remove_recipient_address (CamelMimeMessage *mime_message, 
							     const char *type, const char *address);
void		camel_mime_message_remove_recipient_name (CamelMimeMessage *mime_message, 
							  const char *type, const char *name);

const CamelInternetAddress *camel_mime_message_get_recipients (CamelMimeMessage *mime_message, 
							       const char *type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_MESSAGE_H */
