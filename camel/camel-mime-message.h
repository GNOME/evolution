/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camelMimeMessage.h : class for a mime message
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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
#include <camel/camel-mime-filter-bestenc.h>

#define CAMEL_RECIPIENT_TYPE_TO "To"
#define CAMEL_RECIPIENT_TYPE_CC "Cc"
#define CAMEL_RECIPIENT_TYPE_BCC "Bcc"

#define CAMEL_RECIPIENT_TYPE_RESENT_TO "Resent-To"
#define CAMEL_RECIPIENT_TYPE_RESENT_CC "Resent-Cc"
#define CAMEL_RECIPIENT_TYPE_RESENT_BCC "Resent-Bcc"

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

	/* cached internal copy */
	time_t date_received;
	int date_received_offset;	/* GMT offset */

	char *subject;

	char *message_id;

	CamelInternetAddress *reply_to;
	CamelInternetAddress *from;

	GHashTable *recipients;	/* hash table of CamelInternetAddress's */
};

typedef struct {
	CamelMimePartClass parent_class;

	/* Virtual methods */	

} CamelMimeMessageClass;



/* Standard Camel function */
CamelType                   camel_mime_message_get_type           (void);


/* public methods */
CamelMimeMessage           *camel_mime_message_new                (void);
void                        camel_mime_message_set_date           (CamelMimeMessage           *mime_message,
								   time_t                      date,
								   int                         offset);
time_t                      camel_mime_message_get_date           (CamelMimeMessage           *mime_message,
								   int                        *offset);
time_t                      camel_mime_message_get_date_received  (CamelMimeMessage           *mime_message,
								   int                        *offset);
void                        camel_mime_message_set_message_id     (CamelMimeMessage           *mime_message,
								   const char                 *message_id);
const char                 *camel_mime_message_get_message_id     (CamelMimeMessage           *mime_message);
void                        camel_mime_message_set_reply_to       (CamelMimeMessage           *mime_message,
								   const CamelInternetAddress *reply_to);
const CamelInternetAddress *camel_mime_message_get_reply_to       (CamelMimeMessage           *mime_message);

void                        camel_mime_message_set_subject        (CamelMimeMessage           *mime_message,
								   const char                 *subject);
const char                 *camel_mime_message_get_subject        (CamelMimeMessage           *mime_message);
void                        camel_mime_message_set_from           (CamelMimeMessage           *mime_message,
								   const CamelInternetAddress *from);
const CamelInternetAddress *camel_mime_message_get_from           (CamelMimeMessage           *mime_message);

const CamelInternetAddress *camel_mime_message_get_recipients     (CamelMimeMessage           *mime_message,
								   const char                 *type);
void                        camel_mime_message_set_recipients     (CamelMimeMessage           *mime_message,
								   const char                 *type,
								   const CamelInternetAddress *r);

void                        camel_mime_message_set_source         (CamelMimeMessage           *mime_message,
								   const char                 *identity);
const char                 *camel_mime_message_get_source         (CamelMimeMessage           *mime_message);
								   

/* utility functions */
gboolean                    camel_mime_message_has_8bit_parts     (CamelMimeMessage           *mime_message);
void                        camel_mime_message_set_best_encoding  (CamelMimeMessage           *msg,
								   CamelBestencRequired        required,
								   CamelBestencEncoding        enctype);
void                        camel_mime_message_encode_8bit_parts  (CamelMimeMessage           *mime_message);

CamelMimePart              *camel_mime_message_get_part_by_content_id (CamelMimeMessage *message, const char *content_id);

char                       *camel_mime_message_build_mbox_from    (CamelMimeMessage           *mime_message);

void camel_mime_message_dump(CamelMimeMessage *msg, int body);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_MESSAGE_H */
