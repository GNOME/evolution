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
	char *domain;
	char *email;
	char *username;

	/* Receiving server*/
	char *recv_type;
	char *recv_hostname;
	char *recv_port;
	char *recv_socket_type;
	char *recv_username;
	char *recv_auth;

	/* Sending server */
	char *send_type;
	char *send_hostname;
	char *send_port;
	char *send_socket_type;
	char *send_username;
	char *send_auth;
} EmailProvider;

gboolean mail_guess_servers (EmailProvider *provider);

#endif
