/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#include <config.h>
#include "camel-tcp-stream-raw.h"
#include "camel-operation.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static CamelTcpStreamClass *parent_class = NULL;

/* Returns the class for a CamelTcpStreamRaw */
#define CTSR_CLASS(so) CAMEL_TCP_STREAM_RAW_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);

static int stream_connect (CamelTcpStream *stream, struct hostent *host, int port);
static int stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data);
static int stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data);

static void
camel_tcp_stream_raw_class_init (CamelTcpStreamRawClass *camel_tcp_stream_raw_class)
{
	CamelTcpStreamClass *camel_tcp_stream_class =
		CAMEL_TCP_STREAM_CLASS (camel_tcp_stream_raw_class);
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_tcp_stream_raw_class);
	
	parent_class = CAMEL_TCP_STREAM_CLASS (camel_type_get_global_classfuncs (camel_tcp_stream_get_type ()));
	
	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->close = stream_close;
	
	camel_tcp_stream_class->connect = stream_connect;
	camel_tcp_stream_class->getsockopt = stream_getsockopt;
	camel_tcp_stream_class->setsockopt = stream_setsockopt;
}

static void
camel_tcp_stream_raw_init (gpointer object, gpointer klass)
{
	CamelTcpStreamRaw *stream = CAMEL_TCP_STREAM_RAW (object);
	
	stream->sockfd = -1;
}

static void
camel_tcp_stream_raw_finalize (CamelObject *object)
{
	CamelTcpStreamRaw *stream = CAMEL_TCP_STREAM_RAW (object);
	
	if (stream->sockfd != -1)
		close (stream->sockfd);
}


CamelType
camel_tcp_stream_raw_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_tcp_stream_get_type (),
					    "CamelTcpStreamRaw",
					    sizeof (CamelTcpStreamRaw),
					    sizeof (CamelTcpStreamRawClass),
					    (CamelObjectClassInitFunc) camel_tcp_stream_raw_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_tcp_stream_raw_init,
					    (CamelObjectFinalizeFunc) camel_tcp_stream_raw_finalize);
	}
	
	return type;
}


/**
 * camel_tcp_stream_raw_new:
 *
 * Return value: a tcp stream
 **/
CamelStream *
camel_tcp_stream_raw_new ()
{
	CamelTcpStreamRaw *stream;
	
	stream = CAMEL_TCP_STREAM_RAW (camel_object_new (camel_tcp_stream_raw_get_type ()));
	
	return CAMEL_STREAM (stream);
}

static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelTcpStreamRaw *tcp_stream_raw = CAMEL_TCP_STREAM_RAW (stream);
	ssize_t nread;
	int cancel_fd;
	
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return  -1;
	}
	
	cancel_fd = camel_operation_cancel_fd (NULL);
	if (cancel_fd == -1) {
		do {
			nread = read (tcp_stream_raw->sockfd, buffer, n);
		} while (nread == -1 && errno == EINTR);
	} else {
		int flags, fdmax;
		fd_set rdset;
		
		flags = fcntl (tcp_stream_raw->sockfd, F_GETFL);
		fcntl (tcp_stream_raw->sockfd, F_SETFL, flags | O_NONBLOCK);
		
		FD_ZERO (&rdset);
		FD_SET (tcp_stream_raw->sockfd, &rdset);
		FD_SET (cancel_fd, &rdset);
		fdmax = MAX (tcp_stream_raw->sockfd, cancel_fd) + 1;
		
		select (fdmax, &rdset, 0, 0, NULL);
		if (FD_ISSET (cancel_fd, &rdset)) {
			fcntl (tcp_stream_raw->sockfd, F_SETFL, flags);
			errno = EINTR;
			return -1;
		}
		
		nread = read (tcp_stream_raw->sockfd, buffer, n);
		fcntl (tcp_stream_raw->sockfd, F_SETFL, flags);
	}
	
	return nread;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelTcpStreamRaw *tcp_stream_raw = CAMEL_TCP_STREAM_RAW (stream);
	ssize_t w, written = 0;
	int cancel_fd;
	
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return  -1;
	}
	
	cancel_fd = camel_operation_cancel_fd (NULL);
	if (cancel_fd == -1) {
		do {
			written = write (tcp_stream_raw->sockfd, buffer, n);
		} while (written == -1 && errno == EINTR);
	} else {
		fd_set rdset, wrset;
		int flags, fdmax;
		
		flags = fcntl (tcp_stream_raw->sockfd, F_GETFL);
		fcntl (tcp_stream_raw->sockfd, F_SETFL, flags | O_NONBLOCK);
		
		fdmax = MAX (tcp_stream_raw->sockfd, cancel_fd) + 1;
		do {
			FD_ZERO (&rdset);
			FD_ZERO (&wrset);
			FD_SET (tcp_stream_raw->sockfd, &wrset);
			FD_SET (cancel_fd, &rdset);
			
			select (fdmax, &rdset, &wrset, 0, NULL);
			if (FD_ISSET (cancel_fd, &rdset)) {
				fcntl (tcp_stream_raw->sockfd, F_SETFL, flags);
				errno = EINTR;
				return -1;
			}
			
			w = write (tcp_stream_raw->sockfd, buffer + written, n - written);
			if (w > 0)
				written += w;
		} while (w != -1 && written < n);
		
		fcntl (tcp_stream_raw->sockfd, F_SETFL, flags);
	}
	
	return written;
}

static int
stream_flush (CamelStream *stream)
{
	return fsync (((CamelTcpStreamRaw *)stream)->sockfd);
}

static int
stream_close (CamelStream *stream)
{
	if (close (((CamelTcpStreamRaw *)stream)->sockfd) == -1)
		return -1;
	
	((CamelTcpStreamRaw *)stream)->sockfd = -1;
	return 0;
}

/* this is a 'cancellable' connect, cancellable from camel_operation_cancel etc */
/* returns -1 & errno == EINTR if the connection was cancelled */
static int
socket_connect (struct hostent *h, int port)
{
	struct sockaddr_in sin;
	int fd;
	int ret;
	socklen_t len;
	struct timeval tv;
	int cancel_fd;
	
	/* see if we're cancelled yet */
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return -1;
	}
	
	/* setup connect, we do it using a nonblocking socket so we can poll it */
	sin.sin_port = htons (port);
	sin.sin_family = h->h_addrtype;
	memcpy (&sin.sin_addr, h->h_addr, sizeof (sin.sin_addr));
	
	fd = socket (h->h_addrtype, SOCK_STREAM, 0);
	
	cancel_fd = camel_operation_cancel_fd (NULL);
	if (cancel_fd == -1) {
		ret = connect (fd, (struct sockaddr *)&sin, sizeof (sin));
		if (ret == -1) {
			close (fd);
			return -1;
		}
		
		return fd;
	} else {
		fd_set rdset, wrset;
		int flags, fdmax;
		
		flags = fcntl (fd, F_GETFL);
		fcntl (fd, F_SETFL, flags | O_NONBLOCK);
		
		ret = connect (fd, (struct sockaddr *)&sin, sizeof (sin));
		if (ret == 0) {
			fcntl (fd, F_SETFL, flags);
			return fd;
		}
		
		if (errno != EINPROGRESS) {
			close (fd);
			return -1;
		}
		
		FD_ZERO (&rdset);
		FD_ZERO (&wrset);
		FD_SET (fd, &wrset);
		FD_SET (cancel_fd, &rdset);
		fdmax = MAX (fd, cancel_fd) + 1;
		tv.tv_usec = 0;
		tv.tv_sec = 60 * 4;
		
		if (select (fdmax, &rdset, &wrset, 0, &tv) == 0) {
			close (fd);
			errno = ETIMEDOUT;
			return -1;
		}
		
		if (cancel_fd != -1 && FD_ISSET (cancel_fd, &rdset)) {
			close (fd);
			errno = EINTR;
			return -1;
		} else {
			len = sizeof (int);
			
			if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &ret, &len) == -1) {
				close (fd);
				return -1;
			}
			
			if (ret != 0) {
				close (fd);
				errno = ret;
				return -1;
			}
		}
		
		fcntl (fd, F_SETFL, flags);
	}
	
	return fd;
}

static int
stream_connect (CamelTcpStream *stream, struct hostent *host, int port)
{
	CamelTcpStreamRaw *raw = CAMEL_TCP_STREAM_RAW (stream);
	int fd;
	
	g_return_val_if_fail (host != NULL, -1);
	
	fd = socket_connect (host, port);
	if (fd == -1)
		return -1;
	
	raw->sockfd = fd;
	
	return 0;
}


static int
get_sockopt_level (const CamelSockOptData *data)
{
	switch (data->option) {
	case CAMEL_SOCKOPT_MAXSEGMENT:
	case CAMEL_SOCKOPT_NODELAY:
		return IPPROTO_TCP;
	default:
		return SOL_SOCKET;
	}
}

static int
get_sockopt_optname (const CamelSockOptData *data)
{
	switch (data->option) {
	case CAMEL_SOCKOPT_MAXSEGMENT:
		return TCP_MAXSEG;
	case CAMEL_SOCKOPT_NODELAY:
		return TCP_NODELAY;
	case CAMEL_SOCKOPT_BROADCAST:
		return SO_BROADCAST;
	case CAMEL_SOCKOPT_KEEPALIVE:
		return SO_KEEPALIVE;
	case CAMEL_SOCKOPT_LINGER:
		return SO_LINGER;
	case CAMEL_SOCKOPT_RECVBUFFERSIZE:
		return SO_RCVBUF;
	case CAMEL_SOCKOPT_SENDBUFFERSIZE:
		return SO_SNDBUF;
	case CAMEL_SOCKOPT_REUSEADDR:
		return SO_REUSEADDR;
	case CAMEL_SOCKOPT_IPTYPEOFSERVICE:
		return SO_TYPE;
	default:
		return -1;
	}
}

static int
stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data)
{
	int optname, optlen;
	
	if ((optname = get_sockopt_optname (data)) == -1)
		return -1;
	
	if (data->option == CAMEL_SOCKOPT_NONBLOCKING) {
		int flags;
		
		flags = fcntl (((CamelTcpStreamRaw *)stream)->sockfd, F_GETFL);
		if (flags == -1)
			return -1;
		
		data->value.non_blocking = flags & O_NONBLOCK;
		
		return 0;
	}
	
	return getsockopt (((CamelTcpStreamRaw *)stream)->sockfd,
			   get_sockopt_level (data),
			   optname,
			   (void *) &data->value,
			   &optlen);
}

static int
stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data)
{
	int optname;
	
	if ((optname = get_sockopt_optname (data)) == -1)
		return -1;
	
	if (data->option == CAMEL_SOCKOPT_NONBLOCKING) {
		int flags, set;
		
		flags = fcntl (((CamelTcpStreamRaw *)stream)->sockfd, F_GETFL);
		if (flags == -1)
			return -1;
		
		set = data->value.non_blocking ? 1 : 0;
		flags = (flags & ~O_NONBLOCK) | (set & O_NONBLOCK);
		
		if (fcntl (((CamelTcpStreamRaw *)stream)->sockfd, F_SETFL, flags) == -1)
			return -1;
		
		return 0;
	}
	
	return setsockopt (((CamelTcpStreamRaw *)stream)->sockfd,
			   get_sockopt_level (data),
			   optname,
			   (void *) &data->value,
			   sizeof (data->value));
}
