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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "camel-io.h"
#include "camel-operation.h"


#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* FIXME: should we trade out select() for a poll() instead? */

ssize_t
camel_read (int fd, char *buf, size_t n)
{
	ssize_t nread;
	int cancel_fd;
	
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return -1;
	}
	
	cancel_fd = camel_operation_cancel_fd (NULL);
	if (cancel_fd == -1) {
		do {
			nread = read (fd, buf, n);
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
	} else {
		int errnosav, flags, fdmax;
		fd_set rdset;
		
		flags = fcntl (fd, F_GETFL);
		fcntl (fd, F_SETFL, flags | O_NONBLOCK);
		
		do {
			FD_ZERO (&rdset);
			FD_SET (fd, &rdset);
			FD_SET (cancel_fd, &rdset);
			fdmax = MAX (fd, cancel_fd) + 1;
			
			nread = -1;
			if (select (fdmax, &rdset, 0, 0, NULL) != -1) {
				if (FD_ISSET (cancel_fd, &rdset)) {
					fcntl (fd, F_SETFL, flags);
					errno = EINTR;
					return -1;
				}
				
				do {
					nread = read (fd, buf, n);
				} while (nread == -1 && errno == EINTR);
			} else if (errno == EINTR) {
				errno = EAGAIN;
			}
		} while (nread == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
		
		errnosav = errno;
		fcntl (fd, F_SETFL, flags);
		errno = errnosav;
	}
	
	return nread;
}


ssize_t
camel_write (int fd, const char *buf, size_t n)
{
	ssize_t w, written = 0;
	int cancel_fd;
	
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return -1;
	}
	
	cancel_fd = camel_operation_cancel_fd (NULL);
	if (cancel_fd == -1) {
		do {
			do {
				w = write (fd, buf + written, n - written);
			} while (w == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
			
			if (w > 0)
				written += w;
		} while (w != -1 && written < n);
	} else {
		int errnosav, flags, fdmax;
		fd_set rdset, wrset;
		
		flags = fcntl (fd, F_GETFL);
		fcntl (fd, F_SETFL, flags | O_NONBLOCK);
		
		fdmax = MAX (fd, cancel_fd) + 1;
		do {
			FD_ZERO (&rdset);
			FD_ZERO (&wrset);
			FD_SET (fd, &wrset);
			FD_SET (cancel_fd, &rdset);
			
			w = -1;
			if (select (fdmax, &rdset, &wrset, 0, NULL) != -1) {
				if (FD_ISSET (cancel_fd, &rdset)) {
					fcntl (fd, F_SETFL, flags);
					errno = EINTR;
					return -1;
				}
				
				do {
					w = write (fd, buf + written, n - written);
				} while (w == -1 && errno == EINTR);
				
				if (w == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						w = 0;
					} else {
						errnosav = errno;
						fcntl (fd, F_SETFL, flags);
						errno = errnosav;
						return -1;
					}
				} else
					written += w;
			} else if (errno == EINTR) {
				w = 0;
			}
		} while (w != -1 && written < n);
		
		errnosav = errno;
		fcntl (fd, F_SETFL, flags);
		errno = errnosav;
	}
	
	if (w == -1)
		return -1;
	
	return written;
}
