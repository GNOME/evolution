/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-mlist-magic.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
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

/* Procmail-style magic mail rules for mailing lists: (from Joakim's own
   `.procmailrc'.)

   :0: 
   * ^Sender: owner-\/[^@]+
   lists/$MATCH
   
   :0:
   * ^X-BeenThere: \/[^@]+
   lists/$MATCH
   
   :0:
   * ^Delivered-To: mailing list \/[^@]+
   lists/$MATCH
   
   :0:
   * X-Mailing-List: <\/[^@]+
   lists/$MATCH
   
   :0:
   * X-Loop: \/[^@]+
   lists/$MATCH

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ctype.h>

#include "camel.h"

#include "mail-mlist-magic.h"


/* Utility functions.  */

static char *
extract_until_at_sign (const char *s)
{
	const char *at_sign;

	at_sign = strchr (s, '@');
	if (at_sign == NULL)
		return g_strdup (s);

	if (at_sign == s)
		return NULL;

	return g_strndup (s, at_sign - s);
}

static const char *
get_header (CamelMimeMessage *message,
	    const char *header_name)
{
	const char *value;

	value = camel_medium_get_header (CAMEL_MEDIUM (message), "Sender");
	if (value == NULL)
		return NULL;

	/* FIXME: Correct?  */
	while (isspace ((int) *value))
		value++;

	return value;
}


/* The checks.  */

/* ^Sender: owner-\/[^@]+ */
static char *
check_sender (CamelMimeMessage *message,
	      const char **header_name_return,
	      char **header_value_return)
{
	const char *value;

	value = get_header (message, "Sender");
	if (value == NULL)
		return NULL;

	if (strncmp (value, "owner-", 6) != 0)
		return NULL;

	if (value[6] == '\0' || value[6] == '@')
		return NULL;

	*header_name_return = "Sender";
	*header_value_return = g_strdup (value);
	return extract_until_at_sign (value + 6);
}
   
/* ^X-BeenThere: \/[^@]+ */
static char *
check_x_been_there (CamelMimeMessage *message,
		    const char **header_name_return,
		    char **header_value_return)
{
	const char *value;

	value = get_header (message, "X-BeenThere");
	if (value == NULL || *value == '@')
		return NULL;

	*header_name_return = "X-BeenThere";
	*header_value_return = g_strdup (value);

	return extract_until_at_sign (value);
}
   
/* ^Delivered-To: mailing list \/[^@]+ */
static char *
check_delivered_to (CamelMimeMessage *message,
		    const char **header_name_return,
		    char **header_value_return)
{
	const char *value;

	value = get_header (message, "Delivered-To");
	if (value == NULL)
		return NULL;

	/* FIXME uh? */
	if (strncmp (value, "mailing list ", 13) != 0)
		return NULL;

	if (value[13] == '\0' || value[13] == '@')
		return NULL;

	*header_name_return = "Delivered-To";
	*header_value_return = g_strdup (value);
	return extract_until_at_sign (value + 13);
}
   
/* X-Mailing-List: <\/[^@]+ */
static char *
check_x_mailing_list (CamelMimeMessage *message,
		      const char **header_name_return,
		      char **header_value_return)
{
	const char *value;
	int value_length;

	value = get_header (message, "X-Mailing-List");
	if (value == NULL)
		return NULL;

	if (value[0] != '<' || value[1] == '\0' || value[1] == '@')
		return NULL;

	value_length = strlen (value);
	if (value[value_length - 1] != '>')
		return NULL;

	*header_name_return = "X-Mailing-List";
	*header_value_return = g_strdup (value);
	return extract_until_at_sign (value + 1);
}
   
/* X-Loop: \/[^@]+ */
static char *
check_x_loop (CamelMimeMessage *message,
	      const char **header_name_return,
	      char **header_value_return)
{
	const char *value;

	value = get_header (message, "X-Loop");
	if (value == NULL)
		return NULL;

	if (*value == '\0' || *value == '@')
		return NULL;

	*header_name_return = "X-Loop";
	*header_value_return = g_strdup (value);

	return extract_until_at_sign (value);
}


/**
 * mail_mlist_magic_detect_list:
 * @message:
 * @header_name_return:
 * @header_value_return:
 * 
 * Detect if message was delivered by a mailing list.
 * 
 * Return value: The name of the mailing list, if the message appears to be
 * sent from a mailing list.  NULL otherwise.
 **/
char *
mail_mlist_magic_detect_list (CamelMimeMessage *message,
			      const char **header_name_return,
			      char **header_value_return)
{
	char *list_name;

	g_return_val_if_fail (message != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	list_name = check_sender (message, header_name_return, header_value_return);
	if (list_name != NULL)
		return list_name;

	list_name = check_x_been_there (message, header_name_return, header_value_return);
	if (list_name != NULL)
		return list_name;

	list_name = check_delivered_to (message, header_name_return, header_value_return);
	if (list_name != NULL)
		return list_name;

	list_name = check_x_mailing_list (message, header_name_return, header_value_return);
	if (list_name != NULL)
		return list_name;

	list_name = check_x_loop (message, header_name_return, header_value_return);
	if (list_name != NULL)
		return list_name;

	return NULL;
}
