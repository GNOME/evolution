/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Peter Williams <peterw@ximian.com>
 */

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef MAIL_TOOLS_H
#define MAIL_TOOLS_H

#include <camel/camel.h>

/* Does a camel_movemail into the local movemail folder
 * and returns the path to the new movemail folder that was created. which shoudl be freed later */
gchar *		mail_tool_do_movemail		(CamelStore *store,
						 GError **error);

CamelNameValueArray *
		mail_tool_remove_xevolution_headers
						(CamelMimeMessage *message);
void		mail_tool_restore_xevolution_headers
						(CamelMimeMessage *message,
						 CamelNameValueArray *headers);

/* Generates the subject for a message forwarding @msg */
gchar *		mail_tool_generate_forward_subject
						(CamelMimeMessage *msg,
						 const gchar *orig_subject);

/* Make a message into an attachment */
CamelMimePart *	mail_tool_make_message_attachment
						(CamelMimeMessage *message);

#endif
