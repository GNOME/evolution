/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "camel-tcp-stream-raw.h"
#include "camel-file-utils.h"
#include "camel-operation.h"

static CamelTcpStreamClass *parent_class = NULL;

/* Returns the class for a CamelTcpStreamRaw */
#define CTSR_CLASS(so) CAMEL_TCP_STREAM_RAW_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);

static int stream_connect (CamelTcpStream *stream, struct addrinfo *host);
static int stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data);
static int stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data);
static struct sockaddr *stream_get_local_address (CamelTcpStream *stream, socklen_t *len);
static struct sockaddr *stream_get_remote_address (CamelTcpStream *stream, socklen_t *len);

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
	camel_tcp_stream_class->setsockopt  = stream_setsockopt;
	camel_tcp_stream_class->get_local_address  = stream_get_local_address;
	camel_tcp_stream_class->get_remote_address = stream_get_remote_address;
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

#ifdef SIMULATE_FLAKY_NETWORK
static ssize_t
flaky_tcp_write (int fd, const char *buffer, size_t buflen)
{
	size_t len = buflen;
	ssize_t nwritten;
	int val;
	
	if (buflen == 0)
		return 0;
	
	val = 1 + (int) (10.0 * rand () / (RAND_MAX + 1.0));
	
	switch (val) {
	case 1:
		printf ("flaky_tcp_write (%d, ..., %d): (-1) EINTR\n", fd, buflen);
		errno = EINTR;
		return -1;
	case 2:
		printf ("flaky_tcp_write (%d, ..., %d): (-1) EAGAIN\n", fd, buflen);
		errno = EAGAIN;
		return -1;
	case 3:
		printf ("flaky_tcp_write (%d, ..., %d): (-1) EWOULDBLOCK\n", fd, buflen);
		errno = EWOULDBLOCK;
		return -1;
	case 4:
	case 5:
	case 6:
		len = 1 + (size_t) (buflen * rand () / (RAND_MAX + 1.0));
		len = MIN (len, buflen);
		/* fall through... */
	default:
		printf ("flaky_tcp_write (%d, ..., %d): (%d) '%.*s'", fd, buflen, len, (int) len, buffer);
		nwritten = write (fd, buffer, len);
		if (nwritten < 0)
			printf (" errno => %s\n", strerror (errno));
		else if (nwritten < len)
			printf (" only wrote %d bytes\n", nwritten);
		else
			printf ("\n");
		
		return nwritten;
	}
}

#define write(fd, buffer, buflen) flaky_tcp_write (fd, buffer, buflen)

static ssize_t
flaky_tcp_read (int fd, char *buffer, size_t buflen)
{
	size_t len = buflen;
	ssize_t nread;
	int val;
	
	if (buflen == 0)
		return 0;
	
	val = 1 + (int) (10.0 * rand () / (RAND_MAX + 1.0));
	
	switch (val) {
	case 1:
		printf ("flaky_tcp_read (%d, ..., %d): (-1) EINTR\n", fd, buflen);
		errno = EINTR;
		return -1;
	case 2:
		printf ("flaky_tcp_read (%d, ..., %d): (-1) EAGAIN\n", fd, buflen);
		errno = EAGAIN;
		return -1;
	case 3:
		printf ("flaky_tcp_read (%d, ..., %d): (-1) EWOULDBLOCK\n", fd, buflen);
		errno = EWOULDBLOCK;
		return -1;
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
		len = 1 + (size_t) (10.0 * rand () / (RAND_MAX + 1.0));
		len = MIN (len, buflen);
		/* fall through... */
	default:
		printf ("flaky_tcp_read (%d, ..., %d): (%d)", fd, buflen, len);
		nread = read (fd, buffer, len);
		if (nread < 0)
			printf (" errno => %s\n", strerror (errno));
		else if (nread < len)
			printf (" only read %d bytes\n", nread);
		else
			printf ("\n");
		
		return nread;
	}
}

#define read(fd, buffer, buflen) flaky_tcp_read (fd, buffer, buflen)

#endif /* SIMULATE_FLAKY_NETWORK */



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
	CamelTcpStreamRaw *raw = CAMEL_TCP_STREAM_RAW (stream);
	
	return camel_read (raw->sockfd, buffer, n);
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelTcpStreamRaw *raw = CAMEL_TCP_STREAM_RAW (stream);
	
	return camel_write (raw->sockfd, buffer, n);
}

static int
stream_flush (CamelStream *stream)
{
	return 0;
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
socket_connect(struct addrinfo *h)
{
	struct timeval tv;
	socklen_t len;
	int cancel_fd;
	int errnosav;
	int ret, fd;
	
	/* see if we're cancelled yet */
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return -1;
	}
	
	if (h->ai_socktype != SOCK_STREAM) {
		errno = EINVAL;
		return -1;
	}

	if ((fd = socket (h->ai_family, SOCK_STREAM, 0)) == -1)
		return -1;
	
	cancel_fd = camel_operation_cancel_fd (NULL);
	if (cancel_fd == -1) {
		if (connect (fd, h->ai_addr, h->ai_addrlen) == -1) {
			errnosav = errno;
			close (fd);
			errno = errnosav;
			return -1;
		}
		
		return fd;
	} else {
		int flags, fdmax, status;
		fd_set rdset, wrset;
		
		flags = fcntl (fd, F_GETFL);
		fcntl (fd, F_SETFL, flags | O_NONBLOCK);
		
		if (connect (fd, h->ai_addr, h->ai_addrlen) == 0) {
			fcntl (fd, F_SETFL, flags);
			return fd;
		}
		
		if (errno != EINPROGRESS) {
			errnosav = errno;
			close (fd);
			errno = errnosav;
			return -1;
		}
		
		do {
			FD_ZERO (&rdset);
			FD_ZERO (&wrset);
			FD_SET (fd, &wrset);
			FD_SET (cancel_fd, &rdset);
			fdmax = MAX (fd, cancel_fd) + 1;
			tv.tv_sec = 60 * 4;
			tv.tv_usec = 0;
			
			status = select (fdmax, &rdset, &wrset, 0, &tv);
		} while (status == -1 && errno == EINTR);
		
		if (status <= 0) {
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
				errnosav = errno;
				close (fd);
				errno = errnosav;
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
stream_connect (CamelTcpStream *stream, struct addrinfo *host)
{
	CamelTcpStreamRaw *raw = CAMEL_TCP_STREAM_RAW (stream);
	
	g_return_val_if_fail (host != NULL, -1);

	while (host) {
		raw->sockfd = socket_connect(host);
		if (raw->sockfd != -1)
			return 0;

		host = host->ai_next;
	}

	return -1;
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
		
		data->value.non_blocking = flags & O_NONBLOCK ? TRUE : FALSE;
		
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
		
		set = data->value.non_blocking ? O_NONBLOCK : 0;
		flags = (flags & ~O_NONBLOCK) | set;
		
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

static struct sockaddr *
stream_get_local_address (CamelTcpStream *stream, socklen_t *len)
{
#ifdef ENABLE_IPv6
	struct sockaddr_in6 sin;
#else
	struct sockaddr_in sin;
#endif
	struct sockaddr *saddr = (struct sockaddr *)&sin;

	*len = sizeof(sin);
	if (getsockname (CAMEL_TCP_STREAM_RAW (stream)->sockfd, saddr, len) == -1)
		return NULL;

	saddr = g_malloc(*len);
	memcpy(saddr, &sin, *len);

	return saddr;
}

static struct sockaddr *
stream_get_remote_address (CamelTcpStream *stream, socklen_t *len)
{
#ifdef ENABLE_IPv6
	struct sockaddr_in6 sin;
#else
	struct sockaddr_in sin;
#endif
	struct sockaddr *saddr = (struct sockaddr *)&sin;

	*len = sizeof(sin);
	if (getpeername (CAMEL_TCP_STREAM_RAW (stream)->sockfd, saddr, len) == -1)
		return NULL;

	saddr = g_malloc(*len);
	memcpy(saddr, &sin, *len);

	return saddr;
}
