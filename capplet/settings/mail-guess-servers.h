/*
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
 * Authors:
 *		Srinivasa Ragavan <srini@linux.intel.com>
 *
 * Copyright (C) 2009 Intel Corporation (www.intel.com)
 *
 */

#ifndef _MAIL_GUESS_SERVERS_
#define _MAIL_GUESS_SERVERS_

typedef struct _EmailProvider {
	/* Input */
	gchar *domain;
	gchar *email;
	gchar *username;

	/* Receiving server*/
	gchar *recv_type;
	gchar *recv_hostname;
	gchar *recv_port;
	gchar *recv_socket_type;
	gchar *recv_username;
	gchar *recv_auth;

	/* Sending server */
	gchar *send_type;
	gchar *send_hostname;
	gchar *send_port;
	gchar *send_socket_type;
	gchar *send_username;
	gchar *send_auth;
} EmailProvider;

gboolean mail_guess_servers (EmailProvider *provider);

#endif
