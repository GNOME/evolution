/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* What should hopefully be a fast mail parser */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#include <stdio.h>
#include <errno.h>

#include <unicode.h>

#include "camel-mime-parser.h"
#include "camel-mime-utils.h"
#include "camel-mime-filter.h"
#include "camel-stream.h"
#include "camel-seekable-stream.h"

#define r(x)
#define h(x)
#define c(x)
#define d(x)

#define SCAN_BUF 4096		/* size of read buffer */
#define SCAN_HEAD 128		/* headroom guaranteed to be before each read buffer */

/* a little hacky, but i couldn't be bothered renaming everything */
#define _header_scan_state _CamelMimeParserPrivate
#define _PRIVATE(o) (((CamelMimeParser *)(o))->priv)

struct _header_scan_state {

    /* global state */

	enum _header_state state;

	/* for building headers during scanning */
	char *outbuf;
	char *outptr;
	char *outend;

	int fd;			/* input for a fd input */
	CamelStream *stream;	/* or for a stream */

	/* for scanning input buffers */
	char *realbuf;		/* the real buffer, SCAN_HEAD*2 + SCAN_BUF bytes */
	char *inbuf;		/* points to a subset of the allocated memory, the underflow */
	char *inptr;		/* (upto SCAN_HEAD) is for use by filters so they dont copy all data */
	char *inend;

	int atleast;

	int seek;		/* current offset to start of buffer */
	int unstep;		/* how many states to 'unstep' (repeat the current state) */

	int midline;		/* are we mid-line interrupted? */
	int scan_from;		/* do we care about From lines? */

	int start_of_from;	/* where from started */
	int start_of_headers;	/* where headers started from the last scan */

	int header_start;	/* start of last header, or -1 */

	struct _header_scan_stack *top_part;	/* top of message header */
	int top_start;		/* offset of start */

	struct _header_scan_stack *pending; /* if we're pending part info, from the wrong part end */

	/* filters to apply to all content before output */
	int filterid;		/* id of next filter */
	struct _header_scan_filter *filters;

    /* per message/part info */
	struct _header_scan_stack *parts;

};

struct _header_scan_stack {
	struct _header_scan_stack *parent;

	enum _header_state savestate; /* state at invocation of this part */

	struct _header_raw *headers;	/* headers for this part */

	struct _header_content_type *content_type;

	char *boundary;		/* for multipart/ * boundaries, including leading -- and trailing -- for the final part */
	int boundarylen;	/* length of boundary, including leading -- */
};

struct _header_scan_filter {
	struct _header_scan_filter *next;
	int id;
	CamelMimeFilter *filter;
};

static void folder_scan_step(struct _header_scan_state *s, char **databuffer, int *datalength);
static int folder_scan_init_with_fd(struct _header_scan_state *s, int fd);
static int folder_scan_init_with_stream(struct _header_scan_state *s, CamelStream *stream);
static struct _header_scan_state *folder_scan_init(void);
static void folder_scan_close(struct _header_scan_state *s);
static struct _header_scan_stack *folder_scan_content(struct _header_scan_state *s, int *lastone, char **data, int *length);
static struct _header_scan_stack *folder_scan_header(struct _header_scan_state *s, int *lastone);
static int folder_scan_skip_line(struct _header_scan_state *s);
static off_t folder_seek(struct _header_scan_state *s, off_t offset, int whence);
static off_t folder_tell(struct _header_scan_state *s);

static void camel_mime_parser_class_init (CamelMimeParserClass *klass);
static void camel_mime_parser_init       (CamelMimeParser *obj);

static char *states[] = {
	"HSCAN_INITIAL",
	"HSCAN_FROM",		/* got 'From' line */
	"HSCAN_HEADER",		/* toplevel header */
	"HSCAN_BODY",		/* scanning body of message */
	"HSCAN_MULTIPART",	/* got multipart header */
	"HSCAN_MESSAGE",		/* rfc822 message */

	"HSCAN_PART",		/* part of a multipart */
	"<invalid>",

	"HSCAN_EOF",		/* end of file */
	"HSCAN_FROM_END",
	"HSCAN_HEAER_END",
	"HSCAN_BODY_END",
	"HSCAN_MULTIPART_END",
	"HSCAN_MESSAGE_END",
};

static GtkObjectClass *camel_mime_parser_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_mime_parser_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelMimeParser",
			sizeof (CamelMimeParser),
			sizeof (CamelMimeParserClass),
			(GtkClassInitFunc) camel_mime_parser_class_init,
			(GtkObjectInitFunc) camel_mime_parser_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_object_get_type (), &type_info);
	}
	
	return type;
}

static void
finalise(GtkObject *o)
{
	struct _header_scan_state *s = _PRIVATE(o);

	folder_scan_close(s);

	((GtkObjectClass *)camel_mime_parser_parent)->finalize (o);
}

static void
camel_mime_parser_class_init (CamelMimeParserClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	camel_mime_parser_parent = gtk_type_class (gtk_object_get_type ());

	object_class->finalize = finalise;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_mime_parser_init (CamelMimeParser *obj)
{
	struct _header_scan_state *s;

	s = folder_scan_init();
	_PRIVATE(obj) = s;
}

/**
 * camel_mime_parser_new:
 *
 * Create a new CamelMimeParser object.
 * 
 * Return value: A new CamelMimeParser widget.
 **/
CamelMimeParser *
camel_mime_parser_new (void)
{
	CamelMimeParser *new = CAMEL_MIME_PARSER ( gtk_type_new (camel_mime_parser_get_type ()));
	return new;
}


int
camel_mime_parser_filter_add(CamelMimeParser *m, CamelMimeFilter *mf)
{
	struct _header_scan_state *s = _PRIVATE(m);
	struct _header_scan_filter *f, *new;

	new = g_malloc(sizeof(*new));
	new->filter = mf;
	new->id = s->filterid++;
	if (s->filterid == -1)
		s->filterid++;
	new->next = 0;
	gtk_object_ref((GtkObject *)mf);

	/* yes, this is correct, since 'next' is the first element of the struct */
	f = (struct _header_scan_filter *)&s->filters;
	while (f->next)
		f = f->next;
	f->next = new;
	return new->id;
}

void
camel_mime_parser_filter_remove(CamelMimeParser *m, int id)
{
	struct _header_scan_state *s = _PRIVATE(m);
	struct _header_scan_filter *f, *old;
	
	f = (struct _header_scan_filter *)&s->filters;
	while (f && f->next) {
		old = f->next;
		if (old->id == id) {
			gtk_object_unref((GtkObject *)old->filter);
			f->next = old->next;
			g_free(old);
			/* there should only be a single matching id, but
			   scan the whole lot anyway */
		}
		f = f->next;
	}
}

const char *
camel_mime_parser_header(CamelMimeParser *m, const char *name, int *offset)
{
	struct _header_scan_state *s = _PRIVATE(m);

	if (s->parts &&
	    s->parts->headers) {
		return header_raw_find(&s->parts->headers, name, offset);
	}
	return NULL;
}

struct _header_raw *
camel_mime_parser_headers_raw(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	if (s->parts)
		return s->parts->headers;
	return NULL;
}

int
camel_mime_parser_init_with_fd(CamelMimeParser *m, int fd)
{
	struct _header_scan_state *s = _PRIVATE(m);

	return folder_scan_init_with_fd(s, fd);
}

int
camel_mime_parser_init_with_stream(CamelMimeParser *m, CamelStream *stream)
{
	struct _header_scan_state *s = _PRIVATE(m);

	return folder_scan_init_with_stream(s, stream);
}

void
camel_mime_parser_scan_from(CamelMimeParser *m, int scan_from)
{
	struct _header_scan_state *s = _PRIVATE(m);
	s->scan_from = scan_from;
}

struct _header_content_type *
camel_mime_parser_content_type(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	/* FIXME: should this search up until its found the 'right'
	   content-type?  can it? */
	if (s->parts)
		return s->parts->content_type;
	return NULL;
}

void camel_mime_parser_unstep(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	s->unstep++;
}

enum _header_state
camel_mime_parser_step(CamelMimeParser *m, char **databuffer, int *datalength)
{
	struct _header_scan_state *s = _PRIVATE(m);

	d(printf("OLD STATE:  '%s' :\n", states[s->state]));

	if (s->unstep <= 0)
		folder_scan_step(s, databuffer, datalength);
	else
		s->unstep--;

	d(printf("NEW STATE:  '%s' :\n", states[s->state]));

	return s->state;
}

off_t camel_mime_parser_tell(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	return folder_tell(s);
}

off_t camel_mime_parser_tell_start_headers(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	return s->start_of_headers;
}

off_t camel_mime_parser_tell_start_from(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	return s->start_of_from;
}

off_t camel_mime_parser_seek(CamelMimeParser *m, off_t off, int whence)
{
	struct _header_scan_state *s = _PRIVATE(m);
	return folder_seek(s, off, whence);
}

enum _header_state camel_mime_parser_state(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);
	return s->state;
}

CamelStream *camel_mime_parser_stream(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);
	return s->stream;
}

int camel_mime_parser_fd(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);
	return s->fd;
}

/* ********************************************************************** */
/*    Implementation							  */
/* ********************************************************************** */

/* read the next bit of data, ensure there is enough room 'atleast' bytes */
static int
folder_read(struct _header_scan_state *s)
{
	int len;
	int inoffset;

	if (s->inptr<s->inend-s->atleast)
		return s->inend-s->inptr;

	/* check for any remaning bytes (under the atleast limit( */
	inoffset = s->inend - s->inptr;
	if (inoffset>0) {
		memcpy(s->inbuf, s->inptr, inoffset);
	}
	if (s->stream) {
		len = camel_stream_read(s->stream, s->inbuf+inoffset, SCAN_BUF-inoffset);
	} else {
		len = read(s->fd, s->inbuf+inoffset, SCAN_BUF-inoffset);
	}
	r(printf("read %d bytes, offset = %d\n", len, inoffset));
	if (len>=0) {
		/* add on the last read block */
		s->seek += s->inptr - s->inbuf;
		s->inptr = s->inbuf;
		s->inend = s->inbuf+len+inoffset;
		r(printf("content = %d '%.*s'\n",s->inend - s->inptr,  s->inend - s->inptr, s->inptr));
	}
	r(printf("content = %d '%.*s'\n", s->inend - s->inptr,  s->inend - s->inptr, s->inptr));
	return s->inend-s->inptr;
}

/* return the current absolute position of the data pointer */
static off_t
folder_tell(struct _header_scan_state *s)
{
	return s->seek + (s->inptr - s->inbuf);
}

/*
  need some way to prime the parser state, so this actually works for 
  other than top-level messages
*/
static off_t
folder_seek(struct _header_scan_state *s, off_t offset, int whence)
{
	off_t newoffset;
	int len;

	if (s->stream) {
		if (CAMEL_IS_SEEKABLE_STREAM(s->stream)) {
			/* NOTE: assumes whence seekable stream == whence libc, which is probably
			   the case (or bloody well should've been) */
			newoffset = camel_seekable_stream_seek((CamelSeekableStream *)s->stream, offset, whence);
		} else {
			newoffset = -1;
			errno = EINVAL;
		}
	} else {
		newoffset = lseek(s->fd, offset, whence);
	}
	if (newoffset != -1) {
		s->seek = newoffset;
		s->inptr = s->inbuf;
		s->inend = s->inbuf;
		if (s->stream)
			len = camel_stream_read(s->stream, s->inbuf, SCAN_BUF);
		else
			len = read(s->fd, s->inbuf, SCAN_BUF);
		if (len>=0)
			s->inend = s->inbuf+len;
		else
			newoffset = -1;
	}
	return newoffset;
}

static void
folder_push_part(struct _header_scan_state *s, struct _header_scan_stack *h)
{
	h->parent = s->parts;
	s->parts = h;
}

static void
folder_pull_part(struct _header_scan_state *s)
{
	struct _header_scan_stack *h;

	h = s->parts;
	if (h) {
		s->parts = h->parent;
		g_free(h->boundary);
		header_raw_clear(&h->headers);
		header_content_type_unref(h->content_type);
		g_free(h);
	} else {
		g_warning("Header stack underflow!\n");
	}
}

static int
folder_scan_skip_line(struct _header_scan_state *s)
{
	int atleast = s->atleast;
	register char *inptr, *inend, c;
	int len;

	s->atleast = 1;

	while ( (len = folder_read(s)) > 0 && len > s->atleast) { /* ensure we have at least enough room here */
		inptr = s->inptr;
		inend = s->inend-1;

		c = -1;
		while (inptr<inend
		       && (c = *inptr++)!='\n')
			;

		s->inptr = inptr;

		if (c=='\n') {
			s->atleast = atleast;
			return 0;
		}
	}

	s->atleast = atleast;

	return -1;		/* not found */
}

/* TODO: Is there any way to make this run faster?  It gets called a lot ... */
static struct _header_scan_stack *
folder_boundary_check(struct _header_scan_state *s, const char *boundary, int *lastone)
{
	struct _header_scan_stack *part;
	int len = s->atleast-2;	/* make sure we dont access past the buffer */

	h(printf("checking boundary marker upto %d bytes\n", len));
	part = s->parts;
	while (part) {
		h(printf("  boundary: %s\n", part->boundary));
		h(printf("   against: '%.*s'\n", len, boundary));
		if (part->boundary
		    && part->boundarylen <= len
		    && memcmp(boundary, part->boundary, part->boundarylen)==0) {
			h(printf("matched boundary: %s\n", part->boundary));
			/* again, make sure we're in range */
			if (part->boundarylen <= len+2) {
				h(printf("checking lastone\n"));
				*lastone = (boundary[part->boundarylen]=='-'
					    && boundary[part->boundarylen+1]=='-');
			} else {
				h(printf("not enough room to check last one?\n"));
				*lastone = FALSE;
			}
			/*printf("ok, we found it! : %s \n", (*lastone)?"Last one":"More to come?");*/
			return part;
		}
		part = part->parent;
	}
	return NULL;
}

/* Copy the string start->inptr into the header buffer (s->outbuf),
   grow if necessary
   and track the start offset of the header */
/* Basically an optimised version of g_byte_array_append() */
#define header_append(s, start, inptr)						\
{										\
	register int headerlen = inptr-start;					\
										\
	if (headerlen >= (s->outend - s->outptr)) {				\
		register char *outnew;						\
		register int len = ((s->outend - s->outbuf)+headerlen)*2+1;	\
		outnew = g_realloc(s->outbuf, len);				\
		s->outptr = s->outptr - s->outbuf + outnew;			\
		s->outbuf = outnew;						\
		s->outend = outnew + len;					\
	}									\
	memcpy(s->outptr, start, headerlen);					\
	s->outptr += headerlen;							\
	if (s->header_start == -1)						\
		s->header_start = (start-s->inbuf) + s->seek;			\
}

static struct _header_scan_stack *
folder_scan_header(struct _header_scan_state *s, int *lastone)
{
	int atleast = s->atleast;
	register char *inptr, *inend;
	char *start;
	int len;
	struct _header_scan_stack *part, *overpart = s->parts;
	struct _header_scan_stack *h;

	h(printf("scanning first bit\n"));

	h = g_malloc0(sizeof(*h));

	/* FIXME: this info should be cached ? */
	part = s->parts;
	s->atleast = 5;
	while (part) {
		if (part->boundary)
			s->atleast = MAX(s->atleast, part->boundarylen+2);
		part = part->parent;
	}
#if 0
	s->atleast = MAX(s->atleast, 5);
	if (s->parts)
		s->atleast = MAX(s->atleast, s->parts->boundarylen+2);
#endif

	*lastone = FALSE;
retry:

	while ((len = folder_read(s))>0 && len >= s->atleast) { /* ensure we have at least enough room here */
		inptr = s->inptr;
		inend = s->inend-s->atleast;
		start = inptr;

		while (inptr<=inend) {
			register int c=-1;
			/*printf("  '%.20s'\n", inptr);*/

			if (!s->midline
			    && (part = folder_boundary_check(s, inptr, lastone))) {
				if ((s->outptr>s->outbuf) || (inptr-start))
					goto header_truncated; /* may not actually be truncated */
				
				goto normal_exit;
			}

			/* goto next line */
			while (inptr<=inend && (c = *inptr++)!='\n')
				;

			header_append(s, start, inptr);

			h(printf("outbuf[0] = %02x '%c' oubuf[1] = %02x '%c'\n",
				 s->outbuf[0], isprint(s->outbuf[0])?s->outbuf[0]:'.',
				 s->outbuf[1], isprint(s->outbuf[1])?s->outbuf[1]:'.'));

			if (c!='\n') {
				s->midline = TRUE;
			} else {
				if (!(inptr[0] == ' ' || inptr[0] == '\t')) {
					if (s->outbuf[0] == '\n'
					    || (s->outbuf[0] == '\r' && s->outbuf[1]=='\n')) {
						goto header_done;
					}

					/* we always have at least _1_ char here ... */
					if (s->outptr[-1] == '\n')
						s->outptr--;
					s->outptr[0] = 0;

					d(printf("header %.10s at %d\n", s->outbuf, s->header_start));

					header_raw_append_parse(&h->headers, s->outbuf, s->header_start);
					if (inptr[0]=='\n'
					    || (inptr[0] == '\r' && inptr[1]=='\n')) {
						inptr++;
						goto header_done;
					}
					s->outptr = s->outbuf;
					s->header_start = -1;
				}
				s->midline = FALSE;
				start = inptr;
			}
		}
		s->inptr = inptr;
	}

	/* ok, we're at the end of the data, just make sure we're not missing out some small
	   truncated header markers */
	if (overpart) {
		overpart = overpart->parent;
		while (overpart) {
			if (overpart->boundary && (overpart->boundarylen+2) < s->atleast) {
				s->atleast = overpart->boundarylen+2;
				h(printf("Retrying next smaller part ...\n"));
				goto retry;
			}
			overpart = overpart->parent;
		}
	}

	if ((s->outptr > s->outbuf) || s->inend > s->inptr) {
		start = s->inptr;
		inptr = s->inend;
		goto header_truncated;
	}

	s->atleast = atleast;

	return h;

header_truncated:

	header_append(s, start, inptr);

	if (s->outptr>s->outbuf && s->outptr[-1] == '\n')
		s->outptr--;
	s->outptr[0] = 0;

	if (s->outbuf[0] == '\n'
	    || (s->outbuf[0] == '\r' && s->outbuf[1]=='\n')) {
		goto header_done;
	}

	header_raw_append_parse(&h->headers, s->outbuf, s->header_start);

header_done:
	part = s->parts;

	s->outptr = s->outbuf;
normal_exit:
	s->inptr = inptr;
	s->atleast = atleast;
	s->header_start = -1;
	return h;
}

static struct _header_scan_stack *
folder_scan_content(struct _header_scan_state *s, int *lastone, char **data, int *length)
{
	int atleast = s->atleast;
	register char *inptr, *inend;
	char *start;
	int len;
	struct _header_scan_stack *part, *overpart = s->parts;
	int already_packed = FALSE;

	/*printf("scanning content\n");*/

	/* FIXME: this info should be cached ? */
	part = s->parts;
	s->atleast = 5;
	while (part) {
		if (part->boundary) {
			c(printf("boundary: %s\n", part->boundary));
			s->atleast = MAX(s->atleast, part->boundarylen+2);
		}
		part = part->parent;
	}
/*	s->atleast = MAX(s->atleast, 5);*/
#if 0
	if (s->parts)
		s->atleast = MAX(s->atleast, s->parts->boundarylen+2);
#endif
	*lastone = FALSE;

retry:
	c(printf("atleast = %d\n", s->atleast));
	
	while ((len = folder_read(s))>0 && len >= s->atleast) { /* ensure we have at least enough room here */
		inptr = s->inptr;
		inend = s->inend-s->atleast;
		start = inptr;

		c(printf("inptr = %p, inend = %p\n", inptr, inend));

		while (inptr<=inend) {
			if (!s->midline
			    && (part = folder_boundary_check(s, inptr, lastone))) {
				if ( (inptr-start) )
					goto content;
				
				goto normal_exit;
			}
			/* goto the next line */
			while (inptr<=inend && (*inptr++)!='\n')
				;
			
			s->midline = FALSE;
		}

		/* *sigh* so much for the beautiful simplicity of the code so far - here we
		   have the snot to deal with the nasty end-cases that come from the read-ahead
		   buffers we use */
		/* what this does, is if we are somewhere near the end of the buffer,
		   force it to the front, and re-read, ensuring we bunch as much together
		   as possible, for the final read, without copying too much of the time */
		/* make sure we dont loop forever, but also make sure we try smaller
		   boundaries, if there are any, so we dont miss any. */
		/* this is not needed for the header scanner, since it copies its own
		   data */
		c(printf("start offset = %d  atleast = %d\n", start-s->inbuf, s->atleast));
		if (start > (s->inbuf + s->atleast)) {
			/* force a re-scan of this data */
			s->inptr = start;
			if (already_packed)
				goto smaller_boundary;
			c(printf("near the end, try and bunch things up a bit first\n"));
			already_packed = TRUE;
		} else {
			c(printf("dumping what i've got ...\n"));
			/* what would be nice here, is if that we're at eof, we bunch the last
			   little bit in the same content, but i dont think this is easy */
			goto content_mid;
		}
	}

	c(printf("length read = %d\n", len));
smaller_boundary:

	/* ok, we're at the end of the data, just make sure we're not missing out some small
	   truncated header markers */
	if (overpart) {
		overpart = overpart->parent;
		while (overpart) {
			if (overpart->boundary && (overpart->boundarylen+2) < s->atleast) {
				s->atleast = overpart->boundarylen+2;
				c(printf("Retrying next smaller part ...\n"));
				goto retry;
			}
			overpart = overpart->parent;
		}
	}

	if (s->inend > s->inptr) {
		start = s->inptr;
		inptr = s->inend;
		goto content;
	}

	*length = 0;
	s->atleast = atleast;
	return NULL;

content_mid:
	s->midline = TRUE;
content:
	part = s->parts;
normal_exit:
	s->atleast = atleast;
	s->inptr = inptr;

	*data = start;
	*length = inptr-start;

/*	printf("got %scontent: %.*s", s->midline?"partial ":"", inptr-start, start);*/

	return part;
}


static void
folder_scan_close(struct _header_scan_state *s)
{
	g_free(s->realbuf);
	g_free(s->outbuf);
	while (s->parts)
		folder_pull_part(s);
	if (s->fd != -1)
		close(s->fd);
	if (s->stream)
		gtk_object_unref((GtkObject *)s->stream);
	g_free(s);
}


static struct _header_scan_state *
folder_scan_init(void)
{
	struct _header_scan_state *s;

	s = g_malloc(sizeof(*s));

	s->fd = -1;
	s->stream = NULL;

	s->outbuf = g_malloc(1024);
	s->outptr = s->outbuf;
	s->outend = s->outbuf+1024;

	s->realbuf = g_malloc(SCAN_BUF + SCAN_HEAD*2);
	s->inbuf = s->realbuf + SCAN_HEAD;
	s->inptr = s->inbuf;
	s->inend = s->inbuf;
	s->atleast = 0;

	s->seek = 0;		/* current character position in file of the last read block */
	s->unstep = 0;

	s->header_start = -1;

	s->start_of_from = -1;
	s->start_of_headers = -1;

	s->midline = FALSE;
	s->scan_from = FALSE;

	s->filters = NULL;
	s->filterid = 1;

	s->parts = NULL;

	s->state = HSCAN_INITIAL;
	return s;
}

static int
folder_scan_init_with_fd(struct _header_scan_state *s, int fd)
{
	int len;

	len = read(fd, s->inbuf, SCAN_BUF);
	if (len>=0) {
		s->inend = s->inbuf+len;
		if (s->fd != -1)
			close(s->fd);
		s->fd = fd;
		if (s->stream) {
			gtk_object_unref((GtkObject *)s->stream);
			s->stream = NULL;
		}
		return 0;
	} else {
		return -1;
	}
}

static int
folder_scan_init_with_stream(struct _header_scan_state *s, CamelStream *stream)
{
	int len;

	len = camel_stream_read(stream, s->inbuf, SCAN_BUF);
	if (len>=0) {
		s->inend = s->inbuf+len;
		if (s->stream)
			gtk_object_unref((GtkObject *)s->stream);
		s->stream = stream;
		gtk_object_ref((GtkObject *)stream);
		if (s->fd != -1) {
			close(s->fd);
			s->fd = -1;
		}
		return 0;
	} else {
		return -1;
	}
}

#define USE_FROM

static void
folder_scan_step(struct _header_scan_state *s, char **databuffer, int *datalength)
{
	struct _header_scan_stack *h, *hb;
	const char *content;
	const char *bound;
	int type;
	int state;
	struct _header_content_type *ct = NULL;
	struct _header_scan_filter *f;
	size_t presize;

/*	printf("\nSCAN PASS: state = %d '%s'\n", s->state, states[s->state]);*/

tail_recurse:
	d({
		printf("\nSCAN STACK:\n");
		printf("  '%s' :\n", states[s->state]);
		hb = s->parts;
		while (hb) {
			printf("  '%s' : %s\n", states[hb->savestate], hb->boundary);
			hb = hb->parent;
		}
		printf("\n");
	});

	switch (s->state) {

	case HSCAN_INITIAL:
#ifdef USE_FROM
		if (s->scan_from) {
			/* FIXME: it would be nice not to have to allocate this every pass */
			h = g_malloc0(sizeof(*h));
			h->boundary = g_strdup("From ");
			h->boundarylen = strlen(h->boundary);
			folder_push_part(s, h);
			
			h = s->parts;
			do {
				hb = folder_scan_content(s, &state, databuffer, datalength);
			} while (hb==h && *datalength>0);
			
			if (*datalength==0 && hb==h) {
				d(printf("found 'From '\n"));
				s->start_of_from = folder_tell(s);
				folder_scan_skip_line(s);
				h->savestate = HSCAN_INITIAL;
				s->state = HSCAN_FROM;
			} else {
				folder_pull_part(s);
				s->state = HSCAN_EOF;
			}
			return;
		} else {
			s->start_of_from = -1;
		}

#endif
	case HSCAN_FROM:
		s->start_of_headers = folder_tell(s);
		h = folder_scan_header(s, &state);
#ifdef USE_FROM
		if (s->scan_from)
			h->savestate = HSCAN_FROM_END;
		else
#endif
			h->savestate = HSCAN_EOF;

		/* FIXME: should this check for MIME-Version: 1.0 as well? */

		type = HSCAN_HEADER;
		if ( (content = header_raw_find(&h->headers, "Content-Type", NULL))
		     && (ct = header_content_type_decode(content))) {
			if (!strcasecmp(ct->type, "multipart")) {
				bound = header_content_type_param(ct, "boundary");
				if (bound) {
					d(printf("multipart, boundary = %s\n", bound));
					h->boundarylen = strlen(bound)+2;
					h->boundary = g_malloc(h->boundarylen+3);
					sprintf(h->boundary, "--%s--", bound);
					type = HSCAN_MULTIPART;
				} else {
					header_content_type_unref(ct);
					ct = header_content_type_decode("text/plain");
/* We can't quite do this, as it will mess up all the offsets ... */
/*					header_raw_replace(&h->headers, "Content-Type", "text/plain", offset);*/
					g_warning("Multipart with no boundary, treating as text/plain");
				}
			} else if (!strcasecmp(ct->type, "message")) {
				if (!strcasecmp(ct->subtype, "rfc822")
				    /*|| !strcasecmp(ct->subtype, "partial")*/) {
					type = HSCAN_MESSAGE;
				}
			}
		}
		h->content_type = ct;
		folder_push_part(s, h);
		s->state = type;
		return;

	case HSCAN_HEADER:
		s->state = HSCAN_BODY;

	case HSCAN_BODY:
		h = s->parts;
		*datalength = 0;
		presize = SCAN_HEAD;
		f = s->filters;

		do {
			hb = folder_scan_content(s, &state, databuffer, datalength);
			if (*datalength>0) {
				d(printf("Content raw: '%.*s'\n", *datalength, *databuffer));

				while (f) {
					camel_mime_filter_filter(f->filter, *databuffer, *datalength, presize,
								 databuffer, datalength, &presize);
					f = f->next;
				}
				return;
			}
		} while (hb==h && *datalength>0);

		/* check for any filter completion data */
		while (f) {
			camel_mime_filter_filter(f->filter, *databuffer, *datalength, presize,
						 databuffer, datalength, &presize);
			f = f->next;
		}
		if (*datalength > 0)
			return;

		s->state = HSCAN_BODY_END;
		break;

	case HSCAN_MULTIPART:
		h = s->parts;
		do {
			do {
				hb = folder_scan_content(s, &state, databuffer, datalength);
				if (*datalength>0) {
					/* FIXME: needs a state to return this shit??? */
					d(printf("Multipart Content: '%.*s'\n", *datalength, *databuffer));
				}
			} while (hb==h && *datalength>0);
			if (*datalength==0 && hb==h) {
				d(printf("got boundary: %s\n", hb->boundary));
				folder_scan_skip_line(s);
				if (!state) {
					s->state = HSCAN_FROM;
					folder_scan_step(s, databuffer, datalength);
					s->parts->savestate = HSCAN_MULTIPART; /* set return state for the new head part */
					return;
				}
			} else {
				break;
			}
		} while (1);

		s->state = HSCAN_MULTIPART_END;
		break;

	case HSCAN_MESSAGE:
		s->state = HSCAN_FROM;
		folder_scan_step(s, databuffer, datalength);
		s->parts->savestate = HSCAN_MESSAGE_END;
		break;

	case HSCAN_FROM_END:
	case HSCAN_BODY_END:
	case HSCAN_MULTIPART_END:
	case HSCAN_MESSAGE_END:
		s->state = s->parts->savestate;
		folder_pull_part(s);
		if (s->state & HSCAN_END)
			return;
		goto tail_recurse;

	case HSCAN_EOF:
		return;

	default:
		g_warning("Invalid state in camel-mime-parser: %d", s->state);
		break;
	}

	return;
}

#ifdef STANDALONE
int main(int argc, char **argv)
{
	int fd;
	struct _header_scan_state *s;
	char *data;
	int len;
	int state;
	char *name = "/tmp/evmail/Inbox";
	struct _header_scan_stack *h;
	int i;
	int attach = 0;

	if (argc==2)
		name = argv[1];

	printf("opening: %s", name);

	for (i=1;i<argc;i++) {
		const char *encoding = NULL, *charset = NULL;
		char *attachname;

		name = argv[i];
		printf("opening: %s", name);
		
		fd = open(name, O_RDONLY);
		if (fd==-1) {
			perror("Cannot open mailbox");
			exit(1);
		}
		s = folder_scan_init(fd);
		s->scan_from = FALSE;
#if 0
		h = g_malloc0(sizeof(*h));
		h->savestate = HSCAN_EOF;
		folder_push_part(s, h);
#endif	
		while (s->state != HSCAN_EOF) {
			folder_scan_step(s, &data, &len);
			printf("\n -- PARSER STEP RETURN -- %d '%s'\n\n", s->state, states[s->state]);
			switch (s->state) {
			case HSCAN_HEADER:
				if (s->parts->content_type
				    && (charset = header_content_type_param(s->parts->content_type, "charset"))) {
					if (strcasecmp(charset, "us-ascii")) {
						folder_push_filter_charset(s, "UTF-8", charset);
					} else {
						charset = NULL;
					}
				} else {
					charset = NULL;
				}

				encoding = header_raw_find(&s->parts->headers, "Content-transfer-encoding");
				printf("encoding = '%s'\n", encoding);
				if (encoding && !strncasecmp(encoding, " base64", 7)) {
					printf("adding base64 filter\n");
					attachname = g_strdup_printf("attach.%d.%d", i, attach++);
					folder_push_filter_save(s, attachname);
					g_free(attachname);
					folder_push_filter_mime(s, 0);
				}
				if (encoding && !strncasecmp(encoding, " quoted-printable", 17)) {
					printf("adding quoted-printable filter\n");
					attachname = g_strdup_printf("attach.%d.%d", i, attach++);
					folder_push_filter_save(s, attachname);
					g_free(attachname);
					folder_push_filter_mime(s, 1);
				}

				break;
			case HSCAN_BODY:
				break;
			case HSCAN_BODY_END:
				if (encoding && !strncasecmp(encoding, " base64", 7)) {
					printf("removing filters\n");
					folder_filter_pull(s);
					folder_filter_pull(s);
				}
				if (encoding && !strncasecmp(encoding, " quoted-printable", 17)) {
					printf("removing filters\n");
					folder_filter_pull(s);
					folder_filter_pull(s);
				}
				if (charset) {
					folder_filter_pull(s);
					charset = NULL;
				}
				encoding = NULL;
				break;
			default:
				break;
			}
		}
		folder_scan_close(s);
		close(fd);
	}
	return 0;
}

#endif /* STANDALONE */
