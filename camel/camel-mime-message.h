/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimeMessage.h : class for a mime message */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org>
 *
 * Copyright 1999 International GNOME Support (http://www.gnome-support.com) .
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

typedef struct _CamelMimeMessage CamelMimeMessage;

#include <gtk/gtk.h>
#include "camel-mime-part.h"
#include "camel-folder.h"
#include "camel-session.h"
#include "camel-recipient.h"


#define CAMEL_RECIPIENT_TYPE_TO "To"
#define CAMEL_RECIPIENT_TYPE_CC "Cc"
#define CAMEL_RECIPIENT_TYPE_BCC "Bcc"


#define CAMEL_MIME_MESSAGE_TYPE     (camel_mime_message_get_type ())
#define CAMEL_MIME_MESSAGE(obj)     (GTK_CHECK_CAST((obj), CAMEL_MIME_MESSAGE_TYPE, CamelMimeMessage))
#define CAMEL_MIME_MESSAGE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MIME_MESSAGE_TYPE, CamelMimeMessageClass))
#define CAMEL_IS_MIME_MESSAGE(o)    (GTK_CHECK_TYPE((o), CAMEL_MIME_MESSAGE_TYPE))



struct _CamelMimeMessage
{
	CamelMimePart parent_object;

	/* header fields */
	gchar *received_date;
	gchar *sent_date;

	gchar *subject;
	gchar *reply_to;

	gchar *from;
	CamelRecipientTable *recipients;

	/* other fields */
	GHashTable *flags; /* boolean values */
	gboolean expunged;
	
	guint message_number; /* set by folder object when retrieving message */
	gchar *message_uid;

	CamelFolder *folder;
	CamelSession *session;

};



typedef struct {
	CamelMimePartClass parent_class;
	
	/* Virtual methods */	
	void     (*set_received_date) (CamelMimeMessage *mime_message, gchar *received_date);
	const gchar *  (*get_received_date) (CamelMimeMessage *mime_message);
	const gchar *  (*get_sent_date) (CamelMimeMessage *mime_message);
	void     (*set_reply_to) (CamelMimeMessage *mime_message, const gchar *reply_to);
	const gchar *  (*get_reply_to) (CamelMimeMessage *mime_message);
	void     (*set_subject) (CamelMimeMessage *mime_message, const gchar *subject);
	const gchar *  (*get_subject) (CamelMimeMessage *mime_message);
	void     (*set_from) (CamelMimeMessage *mime_message, const gchar *from);
	const gchar *  (*get_from) (CamelMimeMessage *mime_message);
	void     (*add_recipient) (CamelMimeMessage *mime_message, const gchar *recipient_type, const gchar *recipient); 
	void     (*remove_recipient) (CamelMimeMessage *mime_message, const gchar *recipient_type, const gchar *recipient);
	const GList *  (*get_recipients) (CamelMimeMessage *mime_message, const gchar *recipient_type);
	void     (*set_flag) (CamelMimeMessage *mime_message, const gchar *flag, gboolean value);
	gboolean (*get_flag) (CamelMimeMessage *mime_message, const gchar *flag);
	GList *  (*get_flag_list) (CamelMimeMessage *mime_message);
	void     (*set_message_number)(CamelMimeMessage *mime_message, guint number);
	guint    (*get_message_number)(CamelMimeMessage *mime_message);
} CamelMimeMessageClass;



/* Standard Gtk function */
GtkType camel_mime_message_get_type (void);


/* public methods */
CamelMimeMessage *camel_mime_message_new_with_session (CamelSession *session);


void camel_mime_message_set_received_date (CamelMimeMessage *mime_message, const gchar *received_date);
const gchar *camel_mime_message_get_received_date (CamelMimeMessage *mime_message);
const gchar *camel_mime_message_get_sent_date (CamelMimeMessage *mime_message);
void camel_mime_message_set_reply_to (CamelMimeMessage *mime_message, const gchar *reply_to);
const gchar *camel_mime_message_get_reply_to (CamelMimeMessage *mime_message);
void camel_mime_message_set_subject (CamelMimeMessage *mime_message, const gchar *subject);
const gchar *camel_mime_message_get_subject (CamelMimeMessage *mime_message);
void camel_mime_message_set_from (CamelMimeMessage *mime_message, const gchar *from);
const gchar *camel_mime_message_get_from (CamelMimeMessage *mime_message);

void camel_mime_message_add_recipient (CamelMimeMessage *mime_message, const gchar *recipient_type, const gchar *recipient);
void camel_mime_message_remove_recipient (CamelMimeMessage *mime_message, const gchar *recipient_type, const gchar *recipient);
const GList *camel_mime_message_get_recipients (CamelMimeMessage *mime_message, const gchar *recipient_type);

void camel_mime_message_set_flag (CamelMimeMessage *mime_message, const gchar *flag, gboolean value);
gboolean camel_mime_message_get_flag (CamelMimeMessage *mime_message, const gchar *flag);
GList *camel_mime_message_get_flag_list (CamelMimeMessage *mime_message);

guint camel_mime_message_get_message_number (CamelMimeMessage *mime_message);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_MESSAGE_H */
