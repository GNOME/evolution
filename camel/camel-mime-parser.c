/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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
 */

/* What should hopefully be a fast mail parser */

/* Do not change this code without asking me (Michael Zucchi) first

   There is almost always a reason something was done a certain way.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#include <stdio.h>
#include <errno.h>

#include <regex.h>
#include <ctype.h>

#include <glib.h>
#include "camel-mime-parser.h"
#include "camel-mime-utils.h"
#include "camel-mime-filter.h"
#include "camel-stream.h"
#include "camel-seekable-stream.h"

#define r(x) 
#define h(x) 
#define c(x) 
#define d(x) 

/*#define PRESERVE_HEADERS*/

/*#define PURIFY*/

#define MEMPOOL

#define STRUCT_ALIGN 4

#ifdef PURIFY
int inend_id = -1,
  inbuffer_id = -1;
#endif

#define SCAN_BUF 4096		/* size of read buffer */
#define SCAN_HEAD 128		/* headroom guaranteed to be before each read buffer */

/* a little hacky, but i couldn't be bothered renaming everything */
#define _header_scan_state _CamelMimeParserPrivate
#define _PRIVATE(o) (((CamelMimeParser *)(o))->priv)

#ifdef MEMPOOL
typedef struct _MemPoolNode {
	struct _MemPoolNode *next;

	int free;
	char data[1];
} MemPoolNode;

typedef struct _MemPoolThresholdNode {
	struct _MemPoolThresholdNode *next;
	char data[1];
} MemPoolThresholdNode;

typedef struct _MemPool {
	int blocksize;
	int threshold;
	struct _MemPoolNode *blocks;
	struct _MemPoolThresholdNode *threshold_blocks;
} MemPool;

MemPool *mempool_new(int blocksize, int threshold);
void *mempool_alloc(MemPool *pool, int size);
void mempool_flush(MemPool *pool, int freeall);
void mempool_free(MemPool *pool);

MemPool *mempool_new(int blocksize, int threshold)
{
	MemPool *pool;

	pool = g_malloc(sizeof(*pool));
	if (threshold >= blocksize)
		threshold = blocksize * 2 / 3;
	pool->blocksize = blocksize;
	pool->threshold = threshold;
	pool->blocks = NULL;
	pool->threshold_blocks = NULL;
	return pool;
}

void *mempool_alloc(MemPool *pool, int size)
{
	size = (size + STRUCT_ALIGN) & (~(STRUCT_ALIGN-1));
	if (size>=pool->threshold) {
		MemPoolThresholdNode *n;

		n = g_malloc(sizeof(*n) - sizeof(char) + size);
		n->next = pool->threshold_blocks;
		pool->threshold_blocks = n;
		return &n->data[0];
	} else {
		MemPoolNode *n;

		n = pool->blocks;
		while (n) {
			if (n->free >= size) {
				n->free -= size;
				return &n->data[n->free];
			}
			n = n->next;
		}

		n = g_malloc(sizeof(*n) - sizeof(char) + pool->blocksize);
		n->next = pool->blocks;
		pool->blocks = n;
		n->free = pool->blocksize - size;
		return &n->data[n->free];
	}
}

void mempool_flush(MemPool *pool, int freeall)
{
	MemPoolThresholdNode *tn, *tw;
	MemPoolNode *pw, *pn;

	tw = pool->threshold_blocks;
	while (tw) {
		tn = tw->next;
		g_free(tw);
		tw = tn;
	}
	pool->threshold_blocks = NULL;

	if (freeall) {
		pw = pool->blocks;
		while (pw) {
			pn = pw->next;
			g_free(pw);
			pw = pn;
		}
		pool->blocks = NULL;
	} else {
		pw = pool->blocks;
		while (pw) {
			pw->free = pool->blocksize;
			pw = pw->next;
		}
	}
}

void mempool_free(MemPool *pool)
{
	if (pool) {
		mempool_flush(pool, 1);
		g_free(pool);
	}
}

#endif

struct _header_scan_state {

    /* global state */

	enum _camel_mime_parser_state state;

	/* for building headers during scanning */
	char *outbuf;
	char *outptr;
	char *outend;

	int fd;			/* input for a fd input */
	CamelStream *stream;	/* or for a stream */

	int ioerrno;		/* io error state */

	/* for scanning input buffers */
	char *realbuf;		/* the real buffer, SCAN_HEAD*2 + SCAN_BUF bytes */
	char *inbuf;		/* points to a subset of the allocated memory, the underflow */
	char *inptr;		/* (upto SCAN_HEAD) is for use by filters so they dont copy all data */
	char *inend;

	int atleast;

	off_t seek;		/* current offset to start of buffer */
	int unstep;		/* how many states to 'unstep' (repeat the current state) */

	unsigned int midline:1;		/* are we mid-line interrupted? */
	unsigned int scan_from:1;	/* do we care about From lines? */
	unsigned int scan_pre_from:1;	/* do we return pre-from data? */
	unsigned int eof:1;		/* reached eof? */

	off_t start_of_from;	/* where from started */
	off_t start_of_headers;	/* where headers started from the last scan */

	off_t header_start;	/* start of last header, or -1 */

	/* filters to apply to all content before output */
	int filterid;		/* id of next filter */
	struct _header_scan_filter *filters;

    /* per message/part info */
	struct _header_scan_stack *parts;

};

struct _header_scan_stack {
	struct _header_scan_stack *parent;

	enum _camel_mime_parser_state savestate; /* state at invocation of this part */

#ifdef MEMPOOL
	MemPool *pool;		/* memory pool to keep track of headers/etc at this level */
#endif
	struct _camel_header_raw *headers;	/* headers for this part */

	CamelContentType *content_type;

	/* I dont use GString's casue you can't efficiently append a buffer to them */
	GByteArray *pretext;	/* for multipart types, save the pre-boundary data here */
	GByteArray *posttext;	/* for multipart types, save the post-boundary data here */
	int prestage;		/* used to determine if it is a pre-boundary or post-boundary data segment */

	GByteArray *from_line;	/* the from line */

	char *boundary;		/* for multipart/ * boundaries, including leading -- and trailing -- for the final part */
	int boundarylen;	/* actual length of boundary, including leading -- if there is one */
	int boundarylenfinal;	/* length of boundary, including trailing -- if there is one */
	int atleast;		/* the biggest boundary from here to the parent */
};

struct _header_scan_filter {
	struct _header_scan_filter *next;
	int id;
	CamelMimeFilter *filter;
};

static void folder_scan_step(struct _header_scan_state *s, char **databuffer, size_t *datalength);
static void folder_scan_drop_step(struct _header_scan_state *s);
static int folder_scan_init_with_fd(struct _header_scan_state *s, int fd);
static int folder_scan_init_with_stream(struct _header_scan_state *s, CamelStream *stream);
static struct _header_scan_state *folder_scan_init(void);
static void folder_scan_close(struct _header_scan_state *s);
static struct _header_scan_stack *folder_scan_content(struct _header_scan_state *s, int *lastone, char **data, size_t *length);
static struct _header_scan_stack *folder_scan_header(struct _header_scan_state *s, int *lastone);
static int folder_scan_skip_line(struct _header_scan_state *s, GByteArray *save);
static off_t folder_seek(struct _header_scan_state *s, off_t offset, int whence);
static off_t folder_tell(struct _header_scan_state *s);
static int folder_read(struct _header_scan_state *s);
#ifdef MEMPOOL
static void header_append_mempool(struct _header_scan_state *s, struct _header_scan_stack *h, char *header, int offset);
#endif

static void camel_mime_parser_class_init (CamelMimeParserClass *klass);
static void camel_mime_parser_init       (CamelMimeParser *obj);

#if d(!)0
static char *states[] = {
	"CAMEL_MIME_PARSER_STATE_INITIAL",
	"CAMEL_MIME_PARSER_STATE_PRE_FROM",	/* pre-from data */
	"CAMEL_MIME_PARSER_STATE_FROM",		/* got 'From' line */
	"CAMEL_MIME_PARSER_STATE_HEADER",		/* toplevel header */
	"CAMEL_MIME_PARSER_STATE_BODY",		/* scanning body of message */
	"CAMEL_MIME_PARSER_STATE_MULTIPART",	/* got multipart header */
	"CAMEL_MIME_PARSER_STATE_MESSAGE",	/* rfc822/news message */

	"CAMEL_MIME_PARSER_STATE_PART",		/* part of a multipart */

	"CAMEL_MIME_PARSER_STATE_EOF",		/* end of file */
	"CAMEL_MIME_PARSER_STATE_PRE_FROM_END",
	"CAMEL_MIME_PARSER_STATE_FROM_END",
	"CAMEL_MIME_PARSER_STATE_HEAER_END",
	"CAMEL_MIME_PARSER_STATE_BODY_END",
	"CAMEL_MIME_PARSER_STATE_MULTIPART_END",
	"CAMEL_MIME_PARSER_STATE_MESSAGE_END",
};
#endif

static CamelObjectClass *camel_mime_parser_parent;

static void
camel_mime_parser_class_init (CamelMimeParserClass *klass)
{
	camel_mime_parser_parent = camel_type_get_global_classfuncs (camel_object_get_type ());
}

static void
camel_mime_parser_init (CamelMimeParser *obj)
{
	struct _header_scan_state *s;

	s = folder_scan_init();
	_PRIVATE(obj) = s;
}

static void
camel_mime_parser_finalise(CamelObject *o)
{
	struct _header_scan_state *s = _PRIVATE(o);
#ifdef PURIFY
	purify_watch_remove_all();
#endif
	folder_scan_close(s);
}

CamelType
camel_mime_parser_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_object_get_type (), "CamelMimeParser",
					    sizeof (CamelMimeParser),
					    sizeof (CamelMimeParserClass),
					    (CamelObjectClassInitFunc) camel_mime_parser_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_mime_parser_init,
					    (CamelObjectFinalizeFunc) camel_mime_parser_finalise);
	}
	
	return type;
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
	CamelMimeParser *new = CAMEL_MIME_PARSER ( camel_object_new (camel_mime_parser_get_type ()));
	return new;
}


/**
 * camel_mime_parser_filter_add:
 * @m: 
 * @mf: 
 * 
 * Add a filter that will be applied to any body content before it is passed
 * to the caller.  Filters may be pipelined to perform multi-pass operations
 * on the content, and are applied in the order they were added.
 *
 * Note that filters are only applied to the body content of messages, and once
 * a filter has been set, all content returned by a filter_step() with a state
 * of CAMEL_MIME_PARSER_STATE_BODY will have passed through the filter.
 * 
 * Return value: An id that may be passed to filter_remove() to remove
 * the filter, or -1 if the operation failed.
 **/
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
	camel_object_ref((CamelObject *)mf);

	/* yes, this is correct, since 'next' is the first element of the struct */
	f = (struct _header_scan_filter *)&s->filters;
	while (f->next)
		f = f->next;
	f->next = new;
	return new->id;
}

/**
 * camel_mime_parser_filter_remove:
 * @m: 
 * @id: 
 * 
 * Remove a processing filter from the pipeline.  There is no
 * restriction on the order the filters can be removed.
 **/
void
camel_mime_parser_filter_remove(CamelMimeParser *m, int id)
{
	struct _header_scan_state *s = _PRIVATE(m);
	struct _header_scan_filter *f, *old;
	
	f = (struct _header_scan_filter *)&s->filters;
	while (f && f->next) {
		old = f->next;
		if (old->id == id) {
			camel_object_unref((CamelObject *)old->filter);
			f->next = old->next;
			g_free(old);
			/* there should only be a single matching id, but
			   scan the whole lot anyway */
		}
		f = f->next;
	}
}

/**
 * camel_mime_parser_header:
 * @m: 
 * @name: Name of header.
 * @offset: Pointer that can receive the offset of the header in
 * the stream from the start of parsing.
 * 
 * Lookup a header by name.
 * 
 * Return value: The header value, or NULL if the header is not
 * defined.
 **/
const char *
camel_mime_parser_header(CamelMimeParser *m, const char *name, int *offset)
{
	struct _header_scan_state *s = _PRIVATE(m);

	if (s->parts &&
	    s->parts->headers) {
		return camel_header_raw_find(&s->parts->headers, name, offset);
	}
	return NULL;
}

/**
 * camel_mime_parser_headers_raw:
 * @m: 
 * 
 * Get the list of the raw headers which are defined for the
 * current state of the parser.  These headers are valid
 * until the next call to parser_step(), or parser_drop_step().
 * 
 * Return value: The raw headers, or NULL if there are no headers
 * defined for the current part or state.  These are READ ONLY.
 **/
struct _camel_header_raw *
camel_mime_parser_headers_raw(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	if (s->parts)
		return s->parts->headers;
	return NULL;
}

static const char *
byte_array_to_string(GByteArray *array)
{
	if (array == NULL)
		return NULL;

	if (array->len == 0 || array->data[array->len-1] != '\0')
		g_byte_array_append(array, "", 1);

	return array->data;
}

/**
 * camel_mime_parser_preface:
 * @m: 
 * 
 * Retrieve the preface text for the current multipart.
 * Can only be used when the state is CAMEL_MIME_PARSER_STATE_MULTIPART_END.
 * 
 * Return value: The preface text, or NULL if there wasn't any.
 **/
const char *
camel_mime_parser_preface(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	if (s->parts)
		return byte_array_to_string(s->parts->pretext);

	return NULL;
}

/**
 * camel_mime_parser_postface:
 * @m: 
 * 
 * Retrieve the postface text for the current multipart.
 * Only returns valid data when the current state if
 * CAMEL_MIME_PARSER_STATE_MULTIPART_END.
 * 
 * Return value: The postface text, or NULL if there wasn't any.
 **/
const char *
camel_mime_parser_postface(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	if (s->parts)
		return byte_array_to_string(s->parts->posttext);

	return NULL;
}

/**
 * camel_mime_parser_from_line:
 * @m: 
 * 
 * Get the last scanned "From " line, from a recently scanned from.
 * This should only be called in the CAMEL_MIME_PARSER_STATE_FROM state.  The
 * from line will include the closing \n found (if there was one).
 *
 * The return value will remain valid while in the CAMEL_MIME_PARSER_STATE_FROM
 * state, or any deeper state.
 * 
 * Return value: The From line, or NULL if called out of context.
 **/
const char *
camel_mime_parser_from_line(CamelMimeParser *m)
{
	struct _header_scan_state *s = _PRIVATE(m);

	if (s->parts)
		return byte_array_to_string(s->parts->from_line);

	return NULL;
}

/**
 * camel_mime_parser_init_with_fd:
 * @m: 
 * @fd: A valid file descriptor.
 * 
 * Initialise the scanner with an fd.  The scanner's offsets
 * will be relative to the current file position of the file
 * descriptor.  As a result, seekable descritors should
 * be seeked using the parser seek functions.
 *
 * Return value: Returns -1 on error.
 **/
int
camel_mime_parser_init_with_fd(CamelMimeParser *m, int fd)
{
	struct _header_scan_state *s = _PRIVATE(m);

	return folder_scan_init_with_fd(s, fd);
}

/**
 * camel_mime_parser_init_with_stream:
 * @m: 
 * @stream: 
 * 
 * Initialise the scanner with a source stream.  The scanner's
 * offsets will be relative to the current file position of
 * the stream.  As a result, seekable streams should only
 * be seeked using the parser seek function.
 * 
 * Return value: -1 on error.
 **/
int
camel_mime_parser_init_with_stream(CamelMimeParser *m, CamelStream *stream)
{
	struct _header_scan_state *s = _PRIVATE(m);

	return folder_scan_init_with_stream(s, stream);
}

/**
 * camel_mime_parser_scan_from:
 * @parser: MIME parser object
 * @scan_from: #TRUE if the scanner should scan From lines.
 * 
 * Tell the scanner if it should scan "^From " lines or not.
 *
 * If the scanner is scanning from lines, two additional
 * states CAMEL_MIME_PARSER_STATE_FROM and CAMEL_MIME_PARSER_STATE_FROM_END will be returned
 * to the caller during parsing.
 *
 * This may also be preceeded by an optional
 * CAMEL_MIME_PARSER_STATE_PRE_FROM state which contains the scanned data
 * found before the From line is encountered.  See also
 * scan_pre_from().
 **/
void
camel_mime_parser_scan_from (CamelMimeParser *parser, gboolean scan_from)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	s->scan_from = scan_from;
}

/**
 * camel_mime_parser_scan_pre_from:
 * @parser: MIME parser object
 * @scan_pre_from: #TRUE if we want to get pre-from data.
 * 
 * Tell the scanner whether we want to know abou the pre-from
 * data during a scan.  If we do, then we may get an additional
 * state CAMEL_MIME_PARSER_STATE_PRE_FROM which returns the specified data.
 **/
void
camel_mime_parser_scan_pre_from (CamelMimeParser *parser, gboolean scan_pre_from)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	s->scan_pre_from = scan_pre_from;
}

/**
 * camel_mime_parser_content_type:
 * @parser: MIME parser object
 * 
 * Get the content type defined in the current part.
 * 
 * Return value: A content_type structure, or NULL if there
 * is no content-type defined for this part of state of the
 * parser.
 **/
CamelContentType *
camel_mime_parser_content_type (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	/* FIXME: should this search up until it's found the 'right'
	   content-type?  can it? */
	if (s->parts)
		return s->parts->content_type;
	
	return NULL;
}

/**
 * camel_mime_parser_unstep:
 * @parser: MIME parser object
 * 
 * Cause the last step operation to repeat itself.  If this is 
 * called repeated times, then the same step will be repeated
 * that many times.
 *
 * Note that it is not possible to scan back using this function,
 * only to have a way of peeking the next state.
 **/
void
camel_mime_parser_unstep (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	s->unstep++;
}

/**
 * camel_mime_parser_drop_step:
 * @parser: MIME parser object
 * 
 * Drop the last step call.  This should only be used
 * in conjunction with seeking of the stream as the
 * stream may be in an undefined state relative to the
 * state of the parser.
 *
 * Use this call with care.
 **/
void
camel_mime_parser_drop_step (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	s->unstep = 0;
	folder_scan_drop_step(s);
}

/**
 * camel_mime_parser_step:
 * @parser: MIME parser object 
 * @databuffer: Pointer to accept a pointer to the data
 * associated with this step (if any).  May be #NULL,
 * in which case datalength is also ingored.
 * @datalength: Pointer to accept a pointer to the data
 * length associated with this step (if any).
 * 
 * Parse the next part of the MIME message.  If _unstep()
 * has been called, then continue to return the same state
 * for that many calls.
 *
 * If the step is CAMEL_MIME_PARSER_STATE_BODY then the databuffer and datalength
 * pointers will be setup to point to the internal data buffer
 * of the scanner and may be processed as required.  Any
 * filters will have already been applied to this data.
 *
 * Refer to the state diagram elsewhere for a full listing of
 * the states an application is gauranteed to get from the
 * scanner.
 *
 * Return value: The current new state of the parser
 * is returned.
 **/
enum _camel_mime_parser_state
camel_mime_parser_step (CamelMimeParser *parser, char **databuffer, size_t *datalength)
{
	struct _header_scan_state *s = _PRIVATE (parser);

	d(printf("OLD STATE:  '%s' :\n", states[s->state]));

	if (s->unstep <= 0) {
		char *dummy;
		size_t dummylength;

		if (databuffer == NULL) {
			databuffer = &dummy;
			datalength = &dummylength;
		}
			
		folder_scan_step(s, databuffer, datalength);
	} else
		s->unstep--;

	d(printf("NEW STATE:  '%s' :\n", states[s->state]));

	return s->state;
}

/**
 * camel_mime_parser_read:
 * @parser: MIME parser object
 * @databuffer: 
 * @len: 
 * 
 * Read at most @len bytes from the internal mime parser buffer.
 *
 * Returns the address of the internal buffer in @databuffer,
 * and the length of useful data.
 *
 * @len may be specified as INT_MAX, in which case you will
 * get the full remainder of the buffer at each call.
 *
 * Note that no parsing of the data read through this function
 * occurs, so no state changes occur, but the seek position
 * is updated appropriately.
 *
 * Return value: The number of bytes available, or -1 on error.
 **/
int
camel_mime_parser_read (CamelMimeParser *parser, const char **databuffer, int len)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	int there;

	if (len == 0)
		return 0;

	d(printf("parser::read() reading %d bytes\n", len));

	there = MIN(s->inend - s->inptr, len);
	d(printf("parser::read() there = %d bytes\n", there));
	if (there > 0) {
		*databuffer = s->inptr;
		s->inptr += there;
		return there;
	}

	if (folder_read(s) == -1)
		return -1;

	there = MIN(s->inend - s->inptr, len);
	d(printf("parser::read() had to re-read, now there = %d bytes\n", there));

	*databuffer = s->inptr;
	s->inptr += there;

	return there;
}

/**
 * camel_mime_parser_tell:
 * @parser: MIME parser object
 * 
 * Return the current scanning offset.  The meaning of this
 * value will depend on the current state of the parser.
 *
 * An incomplete listing of the states:
 *
 * CAMEL_MIME_PARSER_STATE_INITIAL, The start of the current message.
 * CAMEL_MIME_PARSER_STATE_HEADER, CAMEL_MIME_PARSER_STATE_MESSAGE, CAMEL_MIME_PARSER_STATE_MULTIPART, the character
 * position immediately after the end of the header.
 * CAMEL_MIME_PARSER_STATE_BODY, Position within the message of the start
 * of the current data block.
 * CAMEL_MIME_PARSER_STATE_*_END, The position of the character starting
 * the next section of the scan (the last position + 1 of
 * the respective current state).
 * 
 * Return value: See above.
 **/
off_t
camel_mime_parser_tell (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);

	return folder_tell(s);
}

/**
 * camel_mime_parser_tell_start_headers:
 * @parser: MIME parser object
 * 
 * Find out the position within the file of where the
 * headers started, this is cached by the parser
 * at the time.
 * 
 * Return value: The header start position, or -1 if
 * no headers were scanned in the current state.
 **/
off_t
camel_mime_parser_tell_start_headers (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);

	return s->start_of_headers;
}

/**
 * camel_mime_parser_tell_start_from:
 * @parser: MIME parser object
 * 
 * If the parser is scanning From lines, then this returns
 * the position of the start of the From line.
 * 
 * Return value: The start of the from line, or -1 if there
 * was no From line, or From lines are not being scanned.
 **/
off_t
camel_mime_parser_tell_start_from (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);

	return s->start_of_from;
}

/**
 * camel_mime_parser_seek:
 * @parser: MIME parser object
 * @offset: Number of bytes to offset the seek by.
 * @whence: SEEK_SET, SEEK_CUR, SEEK_END
 * 
 * Reset the source position to a known value.
 *
 * Note that if the source stream/descriptor was not
 * positioned at 0 to begin with, and an absolute seek
 * is specified (whence != SEEK_CUR), then the seek
 * position may not match the desired seek position.
 * 
 * Return value: The new seek offset, or -1 on
 * an error (for example, trying to seek on a non-seekable
 * stream or file descriptor).
 **/
off_t
camel_mime_parser_seek(CamelMimeParser *parser, off_t offset, int whence)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	return folder_seek(s, offset, whence);
}

/**
 * camel_mime_parser_state:
 * @parser: MIME parser object
 * 
 * Get the current parser state.
 * 
 * Return value: The current parser state.
 **/
enum _camel_mime_parser_state
camel_mime_parser_state (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	return s->state;
}

/**
 * camel_mime_parser_stream:
 * @parser: MIME parser object
 * 
 * Get the stream, if any, the parser has been initialised
 * with.  May be used to setup sub-streams, but should not
 * be read from directly (without saving and restoring
 * the seek position in between).
 * 
 * Return value: The stream from _init_with_stream(), or NULL
 * if the parser is reading from a file descriptor or is
 * uninitialised.
 **/
CamelStream *
camel_mime_parser_stream (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	return s->stream;
}

/**
 * camel_mime_parser_fd:
 * @parser: MIME parser object
 * 
 * Return the file descriptor, if any, the parser has been
 * initialised with.
 *
 * Should not be read from unless the parser it to terminate,
 * or the seek offset can be reset before the next parse
 * step.
 * 
 * Return value: The file descriptor or -1 if the parser
 * is reading from a stream or has not been initialised.
 **/
int
camel_mime_parser_fd (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	return s->fd;
}

/* Return errno of the parser, incase any error occured during processing */
int
camel_mime_parser_errno (CamelMimeParser *parser)
{
	struct _header_scan_state *s = _PRIVATE (parser);
	
	return s->ioerrno;
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

	if (s->inptr<s->inend-s->atleast || s->eof)
		return s->inend-s->inptr;
#ifdef PURIFY
	purify_watch_remove(inend_id);
	purify_watch_remove(inbuffer_id);
#endif
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
		s->eof = (len == 0);
		r(printf("content = %d '%.*s'\n",s->inend - s->inptr,  s->inend - s->inptr, s->inptr));
	} else {
		s->ioerrno = errno?errno:EIO;
	}

	g_assert(s->inptr<=s->inend);
#ifdef PURIFY
	inend_id = purify_watch(&s->inend);
	inbuffer_id = purify_watch_n(s->inend+1, SCAN_HEAD-1, "rw");
#endif
	r(printf("content = %d '%.*s'\n", s->inend - s->inptr,  s->inend - s->inptr, s->inptr));
	/* set a sentinal, for the inner loops to check against */
	s->inend[0] = '\n';
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
#ifdef PURIFY
	purify_watch_remove(inend_id);
	purify_watch_remove(inbuffer_id);
#endif
	if (newoffset != -1) {
		s->seek = newoffset;
		s->inptr = s->inbuf;
		s->inend = s->inbuf;
		s->eof = FALSE;
	} else {
		s->ioerrno = errno?errno:EIO;
	}
#ifdef PURIFY
	inend_id = purify_watch(&s->inend);
	inbuffer_id = purify_watch_n(s->inend+1, SCAN_HEAD-1, "rw");
#endif
	return newoffset;
}

static void
folder_push_part(struct _header_scan_state *s, struct _header_scan_stack *h)
{
	if (s->parts && s->parts->atleast > h->boundarylenfinal)
		h->atleast = s->parts->atleast;
	else
		h->atleast = MAX(h->boundarylenfinal, 1);

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
#ifdef MEMPOOL
		mempool_free(h->pool);
#else
		camel_header_raw_clear(&h->headers);
#endif
		camel_content_type_unref(h->content_type);
		if (h->pretext)
			g_byte_array_free(h->pretext, TRUE);
		if (h->posttext)
			g_byte_array_free(h->posttext, TRUE);
		if (h->from_line)
			g_byte_array_free(h->from_line, TRUE);
		g_free(h);
	} else {
		g_warning("Header stack underflow!\n");
	}
}

static int
folder_scan_skip_line(struct _header_scan_state *s, GByteArray *save)
{
	int atleast = s->atleast;
	register char *inptr, *inend, c;
	int len;

	s->atleast = 1;

	d(printf("skipping line\n"));

	while ( (len = folder_read(s)) > 0 && len > s->atleast) { /* ensure we have at least enough room here */
		inptr = s->inptr;
		inend = s->inend;

		c = -1;
		while (inptr<inend
		       && (c = *inptr++)!='\n') {
			d(printf("(%2x,%c)", c, isprint(c)?c:'.'));
			;
		}

		if (save)
			g_byte_array_append(save, s->inptr, inptr-s->inptr);

		s->inptr = inptr;

		if (c=='\n') {
			s->atleast = atleast;
			return 0;
		}
	}

	d(printf("couldn't find end of line?\n"));

	s->atleast = atleast;

	return -1;		/* not found */
}

/* TODO: Is there any way to make this run faster?  It gets called a lot ... */
static struct _header_scan_stack *
folder_boundary_check(struct _header_scan_state *s, const char *boundary, int *lastone)
{
	struct _header_scan_stack *part;
	int len = s->inend - boundary; /* make sure we dont access past the buffer */

	h(printf("checking boundary marker upto %d bytes\n", len));
	part = s->parts;
	while (part) {
		h(printf("  boundary: %s\n", part->boundary));
		h(printf("   against: '%.*s'\n", part->boundarylen, boundary));
		if (part->boundary
		    && part->boundarylen <= len
		    && memcmp(boundary, part->boundary, part->boundarylen)==0) {
			h(printf("matched boundary: %s\n", part->boundary));
			/* again, make sure we're in range */
			if (part->boundarylenfinal <= len) {
				int extra = part->boundarylenfinal - part->boundarylen;
				
				/* check the extra stuff on an final boundary, normally -- for mime parts */
				if (extra>0) {
					*lastone = memcmp(&boundary[part->boundarylen],
							  &part->boundary[part->boundarylen],
							  extra) == 0;
				} else {
					*lastone = TRUE;
				}
				h(printf("checking lastone = %s\n", *lastone?"TRUE":"FALSE"));
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

#ifdef MEMPOOL
static void
header_append_mempool(struct _header_scan_state *s, struct _header_scan_stack *h, char *header, int offset)
{
	struct _camel_header_raw *l, *n;
	char *content;
	
	content = strchr(header, ':');
	if (content) {
		register int len;
		n = mempool_alloc(h->pool, sizeof(*n));
		n->next = NULL;
		
		len = content-header;
		n->name = mempool_alloc(h->pool, len+1);
		memcpy(n->name, header, len);
		n->name[len] = 0;
		
		content++;
		
		len = s->outptr - content;
		n->value = mempool_alloc(h->pool, len+1);
		memcpy(n->value, content, len);
		n->value[len] = 0;
		
		n->offset = offset;
		
		l = (struct _camel_header_raw *)&h->headers;
		while (l->next) {
			l = l->next;
		}
		l->next = n;
	}
	
}

#define header_raw_append_parse(a, b, c) (header_append_mempool(s, h, b, c))

#endif

/* Copy the string start->inptr into the header buffer (s->outbuf),
   grow if necessary
   remove trailing \r chars (\n's assumed already removed)
   and track the start offset of the header */
/* Basically an optimised version of g_byte_array_append() */
#define header_append(s, start, inptr)								\
{												\
	register int headerlen = inptr-start;							\
												\
	if (headerlen > 0) {									\
		if (headerlen >= (s->outend - s->outptr)) {					\
			register char *outnew;							\
			register int len = ((s->outend - s->outbuf)+headerlen)*2+1;		\
			outnew = g_realloc(s->outbuf, len);					\
			s->outptr = s->outptr - s->outbuf + outnew;				\
			s->outbuf = outnew;							\
			s->outend = outnew + len;						\
		}										\
		if (start[headerlen-1] == '\r')							\
			headerlen--;								\
		memcpy(s->outptr, start, headerlen);						\
		s->outptr += headerlen;								\
	}											\
	if (s->header_start == -1)								\
		s->header_start = (start-s->inbuf) + s->seek;					\
}

static struct _header_scan_stack *
folder_scan_header(struct _header_scan_state *s, int *lastone)
{
	int atleast = s->atleast, newatleast;
	char *start = NULL;
	int len;
	struct _header_scan_stack *h;
	char *inend;
	register char *inptr;

	h(printf("scanning first bit\n"));

	h = g_malloc0(sizeof(*h));
#ifdef MEMPOOL
	h->pool = mempool_new(8192, 4096);
#endif

	if (s->parts)
		newatleast = s->parts->atleast;
	else
		newatleast = 1;
	*lastone = FALSE;

	do {
		s->atleast = newatleast;

		h(printf("atleast = %d\n", s->atleast));

		while ((len = folder_read(s))>0 && len >= s->atleast) { /* ensure we have at least enough room here */
			inptr = s->inptr;
			inend = s->inend-s->atleast+1;
			
			while (inptr<inend) {
				if (!s->midline) {
					if (folder_boundary_check(s, inptr, lastone)) {
						if ((s->outptr>s->outbuf))
							goto header_truncated; /* may not actually be truncated */
						
						goto header_done;
					}
				}
				
				start = inptr;

				/* goto next line/sentinal */
				while ((*inptr++)!='\n')
					;
			
				g_assert(inptr<=s->inend+1);
				
				/* check for sentinal or real end of line */
				if (inptr > inend) {
					h(printf("not at end of line yet, going further\n"));
					/* didn't find end of line within our allowed area */
					inptr = inend;
					s->midline = TRUE;
					header_append(s, start, inptr);
				} else {
					h(printf("got line part: '%.*s'\n", inptr-1-start, start));
					/* got a line, strip and add it, process it */
					s->midline = FALSE;
#ifdef PRESERVE_HEADERS
					header_append(s, start, inptr);
#else
					header_append(s, start, inptr-1);
#endif
					/* check for end of headers */
					if (s->outbuf == s->outptr)
						goto header_done;

					/* check for continuation/compress headers, we have atleast 1 char here to work with */
					if (inptr[0] ==  ' ' || inptr[0] == '\t') {
						h(printf("continuation\n"));
#ifndef PRESERVE_HEADERS
						/* TODO: this wont catch multiple space continuation across a read boundary, but
						   that is assumed rare, and not fatal anyway */
						do
							inptr++;
						while (*inptr == ' ' || *inptr == '\t');
						inptr--;
						*inptr = ' ';
#endif
					} else {
						/* otherwise, complete header, add it */
#ifdef PRESERVE_HEADERS
						s->outptr--;
						if (s->outptr[-1] == '\r')
							s->outptr--;
#endif
						s->outptr[0] = 0;
				
						h(printf("header '%s' at %d\n", s->outbuf, (int)s->header_start));
						
						header_raw_append_parse(&h->headers, s->outbuf, s->header_start);
						s->outptr = s->outbuf;
						s->header_start = -1;
					}
				}
			}
			s->inptr = inptr;
		}
		h(printf("end of file?  read %d bytes\n", len));
		newatleast = 1;
	} while (s->atleast > 1);

	if ((s->outptr > s->outbuf) || s->inend > s->inptr) {
		start = s->inptr;
		inptr = s->inend;
		if (inptr > start) {
			if (inptr[-1] == '\n')
				inptr--;
		}
		goto header_truncated;
	}
	
	s->atleast = atleast;
	
	return h;
	
header_truncated:
	header_append(s, start, inptr);
	
	s->outptr[0] = 0;
	if (s->outbuf == s->outptr)
		goto header_done;
	
	header_raw_append_parse(&h->headers, s->outbuf, s->header_start);
	
	s->outptr = s->outbuf;
header_done:
	s->inptr = inptr;
	s->atleast = atleast;
	s->header_start = -1;
	return h;
}

static struct _header_scan_stack *
folder_scan_content(struct _header_scan_state *s, int *lastone, char **data, size_t *length)
{
	int atleast = s->atleast, newatleast;
	register char *inptr;
	char *inend;
	char *start;
	int len;
	struct _header_scan_stack *part;
	int onboundary = FALSE;

	c(printf("scanning content\n"));

	part = s->parts;
	if (part)
		newatleast = part->atleast;
	else
		newatleast = 1;
	*lastone = FALSE;

	c(printf("atleast = %d\n", newatleast));

	do {
		s->atleast = newatleast;

		while ((len = folder_read(s))>0 && len >= s->atleast) { /* ensure we have at least enough room here */
			inptr = s->inptr;
			if (s->eof)
				inend = s->inend;
			else
				inend = s->inend-s->atleast+1;
			start = inptr;

			c(printf("inptr = %p, inend = %p\n", inptr, inend));

			while (inptr<inend) {
				if (!s->midline
				    && (part = folder_boundary_check(s, inptr, lastone))) {
					onboundary = TRUE;

					/* since we truncate the boundary data, we need at least 1 char here spare,
					   to remain in the same state */
					if ( (inptr-start) > 1)
						goto content;

					/* otherwise, jump to the state of the boundary we actually found */
					goto normal_exit;
				}
				
				/* goto the next line */
				while ((*inptr++)!='\n')
					;

				/* check the sentinal, if we went past the atleast limit, and reset it to there */
				if (inptr > inend) {
					s->midline = TRUE;
					inptr = inend;
				} else {
					s->midline = FALSE;
				}
			}

			c(printf("ran out of input, dumping what i have (%d) bytes midline = %s\n",
				inptr-start, s->midline?"TRUE":"FALSE"));
			goto content;
		}
		newatleast = 1;
	} while (s->atleast > 1);

	c(printf("length read = %d\n", len));

	if (s->inend > s->inptr) {
		start = s->inptr;
		inptr = s->inend;
		goto content;
	}

	*length = 0;
	s->atleast = atleast;
	return NULL;

content:
	/* treat eof as the last boundary in From mode */
	if (s->scan_from && s->eof && s->atleast <= 1) {
		onboundary = TRUE;
		part = NULL;
	} else {
		part = s->parts;
	}
normal_exit:
	s->atleast = atleast;
	s->inptr = inptr;

	*data = start;
	/* if we hit a boundary, we should not include the closing \n */
	if (onboundary && (inptr-start)>0)
		*length = inptr-start-1;
	else
		*length = inptr-start;

	/*printf("got %scontent: '%.*s'\n", s->midline?"partial ":"", inptr-start, start);*/

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
	if (s->stream) {
		camel_object_unref((CamelObject *)s->stream);
	}
	g_free(s);
}


static struct _header_scan_state *
folder_scan_init(void)
{
	struct _header_scan_state *s;

	s = g_malloc(sizeof(*s));

	s->fd = -1;
	s->stream = NULL;
	s->ioerrno = 0;

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
	s->scan_pre_from = FALSE;
	s->eof = FALSE;

	s->filters = NULL;
	s->filterid = 1;

	s->parts = NULL;

	s->state = CAMEL_MIME_PARSER_STATE_INITIAL;
	return s;
}

static void
drop_states(struct _header_scan_state *s)
{
	while (s->parts) {
		folder_scan_drop_step(s);
	}
	s->unstep = 0;
	s->state = CAMEL_MIME_PARSER_STATE_INITIAL;
}

static void
folder_scan_reset(struct _header_scan_state *s)
{
	drop_states(s);
	s->inend = s->inbuf;
	s->inptr = s->inbuf;
	s->inend[0] = '\n';
	if (s->fd != -1) {
		close(s->fd);
		s->fd = -1;
	}
	if (s->stream) {
		camel_object_unref((CamelObject *)s->stream);
		s->stream = NULL;
	}
	s->ioerrno = 0;
	s->eof = FALSE;
}

static int
folder_scan_init_with_fd(struct _header_scan_state *s, int fd)
{
	folder_scan_reset(s);
	s->fd = fd;

	return 0;
}

static int
folder_scan_init_with_stream(struct _header_scan_state *s, CamelStream *stream)
{
	folder_scan_reset(s);
	s->stream = stream;
	camel_object_ref((CamelObject *)stream);

	return 0;
}

#define USE_FROM

static void
folder_scan_step(struct _header_scan_state *s, char **databuffer, size_t *datalength)
{
	struct _header_scan_stack *h, *hb;
	const char *content;
	const char *bound;
	int type;
	int state;
	CamelContentType *ct = NULL;
	struct _header_scan_filter *f;
	size_t presize;

/*	printf("\nSCAN PASS: state = %d '%s'\n", s->state, states[s->state]);*/

tail_recurse:
	d({
		printf("\nSCAN STACK:\n");
		printf(" '%s' :\n", states[s->state]);
		hb = s->parts;
		while (hb) {
			printf("  '%s' : %s ", states[hb->savestate], hb->boundary);
			if (hb->content_type) {
				printf("(%s/%s)", hb->content_type->type, hb->content_type->subtype);
			} else {
				printf("(default)");
			}
			printf("\n");
			hb = hb->parent;
		}
		printf("\n");
	});

	switch (s->state) {

#ifdef USE_FROM
	case CAMEL_MIME_PARSER_STATE_INITIAL:
		if (s->scan_from) {
			h = g_malloc0(sizeof(*h));
			h->boundary = g_strdup("From ");
			h->boundarylen = strlen(h->boundary);
			h->boundarylenfinal = h->boundarylen;
			h->from_line = g_byte_array_new();
			folder_push_part(s, h);
			s->state = CAMEL_MIME_PARSER_STATE_PRE_FROM;
		} else {
			s->start_of_from = -1;
			goto scan_header;
		}

	case CAMEL_MIME_PARSER_STATE_PRE_FROM:

		h = s->parts;
		do {
			hb = folder_scan_content(s, &state, databuffer, datalength);
			if (s->scan_pre_from && *datalength > 0) {
				d(printf("got pre-from content %d bytes\n", *datalength));
				return;
			}
		} while (hb==h && *datalength>0);

		if (*datalength==0 && hb==h) {
			d(printf("found 'From '\n"));
			s->start_of_from = folder_tell(s);
			folder_scan_skip_line(s, h->from_line);
			h->savestate = CAMEL_MIME_PARSER_STATE_INITIAL;
			s->state = CAMEL_MIME_PARSER_STATE_FROM;
		} else {
			folder_pull_part(s);
			s->state = CAMEL_MIME_PARSER_STATE_EOF;
		}
		return;
#else
	case CAMEL_MIME_PARSER_STATE_INITIAL:
	case CAMEL_MIME_PARSER_STATE_PRE_FROM:
#endif /* !USE_FROM */

	scan_header:
	case CAMEL_MIME_PARSER_STATE_FROM:
		s->start_of_headers = folder_tell(s);
		h = folder_scan_header(s, &state);
#ifdef USE_FROM
		if (s->scan_from)
			h->savestate = CAMEL_MIME_PARSER_STATE_FROM_END;
		else
#endif
			h->savestate = CAMEL_MIME_PARSER_STATE_EOF;

		/* FIXME: should this check for MIME-Version: 1.0 as well? */

		type = CAMEL_MIME_PARSER_STATE_HEADER;
		if ( (content = camel_header_raw_find(&h->headers, "Content-Type", NULL))
		     && (ct = camel_content_type_decode(content))) {
			if (!g_ascii_strcasecmp(ct->type, "multipart")) {
				if (!camel_content_type_is(ct, "multipart", "signed")
				    && (bound = camel_content_type_param(ct, "boundary"))) {
					d(printf("multipart, boundary = %s\n", bound));
					h->boundarylen = strlen(bound)+2;
					h->boundarylenfinal = h->boundarylen+2;
					h->boundary = g_malloc(h->boundarylen+3);
					sprintf(h->boundary, "--%s--", bound);
					type = CAMEL_MIME_PARSER_STATE_MULTIPART;
				} else {
					/*camel_content_type_unref(ct);
					  ct = camel_content_type_decode("text/plain");*/
/* We can't quite do this, as it will mess up all the offsets ... */
/*					camel_header_raw_replace(&h->headers, "Content-Type", "text/plain", offset);*/
					/*g_warning("Multipart with no boundary, treating as text/plain");*/
				}
			} else if (!strcasecmp(ct->type, "message")) {
				if (!strcasecmp(ct->subtype, "rfc822")
				    || !strcasecmp(ct->subtype, "news")
				    /*|| !g_ascii_strcasecmp(ct->subtype, "partial")*/) {
					type = CAMEL_MIME_PARSER_STATE_MESSAGE;
				}
			}
		} else {
			/* make the default type for multipart/digest be message/rfc822 */
			if ((s->parts
			     && camel_content_type_is(s->parts->content_type, "multipart", "digest"))) {
				ct = camel_content_type_decode("message/rfc822");
				type = CAMEL_MIME_PARSER_STATE_MESSAGE;
				d(printf("parent was multipart/digest, autoupgrading to message/rfc822?\n"));
				/* maybe we should do this too?
				   header_raw_append_parse(&h->headers, "Content-Type: message/rfc822", -1);*/
			} else {
				ct = camel_content_type_decode("text/plain");
			}
		}
		h->content_type = ct;
		folder_push_part(s, h);
		s->state = type;
		return;
		
	case CAMEL_MIME_PARSER_STATE_HEADER:
		s->state = CAMEL_MIME_PARSER_STATE_BODY;
		
	case CAMEL_MIME_PARSER_STATE_BODY:
		h = s->parts;
		*datalength = 0;
		presize = SCAN_HEAD;
		f = s->filters;
		
		do {
			hb = folder_scan_content (s, &state, databuffer, datalength);

			d(printf ("\n\nOriginal content: '"));
			d(fwrite(*databuffer, sizeof(char), *datalength, stdout));
			d(printf("'\n"));

			if (*datalength > 0) {
				while (f) {
					camel_mime_filter_filter(f->filter, *databuffer, *datalength, presize,
								 databuffer, datalength, &presize);
					d(printf("Filtered content (%s): '", ((CamelObject *)f->filter)->klass->name));
					d(fwrite(*databuffer, sizeof(char), *datalength, stdout));
					d(printf("'\n"));
					f = f->next;
				}
				return;
			}
		} while (hb == h && *datalength > 0);
		
		/* check for any filter completion data */
		while (f) {
			camel_mime_filter_complete(f->filter, *databuffer, *datalength, presize,
						   databuffer, datalength, &presize);
			f = f->next;
		}

		if (*datalength > 0)
			return;
		
		s->state = CAMEL_MIME_PARSER_STATE_BODY_END;
		break;
		
	case CAMEL_MIME_PARSER_STATE_MULTIPART:
		h = s->parts;
		do {
			do {
				hb = folder_scan_content(s, &state, databuffer, datalength);
				if (*datalength>0) {
					/* instead of a new state, we'll just store it locally and provide
					   an accessor function */
					d(printf("Multipart %s Content %p: '%.*s'\n",
						 h->prestage>0?"post":"pre", h, *datalength, *databuffer));
					if (h->prestage > 0) {
						if (h->posttext == NULL)
							h->posttext = g_byte_array_new();
						g_byte_array_append(h->posttext, *databuffer, *datalength);
					} else {
						if (h->pretext == NULL)
							h->pretext = g_byte_array_new();
						g_byte_array_append(h->pretext, *databuffer, *datalength);
					}
				}
			} while (hb==h && *datalength>0);
			h->prestage++;
			if (*datalength==0 && hb==h) {
				d(printf("got boundary: %s\n", hb->boundary));
				folder_scan_skip_line(s, NULL);
				if (!state) {
					s->state = CAMEL_MIME_PARSER_STATE_FROM;
					folder_scan_step(s, databuffer, datalength);
					s->parts->savestate = CAMEL_MIME_PARSER_STATE_MULTIPART; /* set return state for the new head part */
					return;
				}
			} else {
				break;
			}
		} while (1);

		s->state = CAMEL_MIME_PARSER_STATE_MULTIPART_END;
		break;

	case CAMEL_MIME_PARSER_STATE_MESSAGE:
		s->state = CAMEL_MIME_PARSER_STATE_FROM;
		folder_scan_step(s, databuffer, datalength);
		s->parts->savestate = CAMEL_MIME_PARSER_STATE_MESSAGE_END;
		break;

	case CAMEL_MIME_PARSER_STATE_FROM_END:
	case CAMEL_MIME_PARSER_STATE_BODY_END:
	case CAMEL_MIME_PARSER_STATE_MULTIPART_END:
	case CAMEL_MIME_PARSER_STATE_MESSAGE_END:
		s->state = s->parts->savestate;
		folder_pull_part(s);
		if (s->state & CAMEL_MIME_PARSER_STATE_END)
			return;
		goto tail_recurse;

	case CAMEL_MIME_PARSER_STATE_EOF:
		return;

	default:
		g_warning("Invalid state in camel-mime-parser: %d", s->state);
		break;
	}

	return;
}

/* drops the current state back one */
static void
folder_scan_drop_step(struct _header_scan_state *s)
{
	switch (s->state) {
	case CAMEL_MIME_PARSER_STATE_EOF:
		s->state = CAMEL_MIME_PARSER_STATE_INITIAL;
	case CAMEL_MIME_PARSER_STATE_INITIAL:
		return;

	case CAMEL_MIME_PARSER_STATE_FROM:
	case CAMEL_MIME_PARSER_STATE_PRE_FROM:
		s->state = CAMEL_MIME_PARSER_STATE_INITIAL;
		folder_pull_part(s);
		return;

	case CAMEL_MIME_PARSER_STATE_MESSAGE:
	case CAMEL_MIME_PARSER_STATE_HEADER:
	case CAMEL_MIME_PARSER_STATE_MULTIPART:

	case CAMEL_MIME_PARSER_STATE_FROM_END:
	case CAMEL_MIME_PARSER_STATE_BODY_END:
	case CAMEL_MIME_PARSER_STATE_MULTIPART_END:
	case CAMEL_MIME_PARSER_STATE_MESSAGE_END:

		s->state = s->parts->savestate;
		folder_pull_part(s);
		if (s->state & CAMEL_MIME_PARSER_STATE_END) {
			s->state &= ~CAMEL_MIME_PARSER_STATE_END;
		}
		return;
	default:
		/* FIXME: not sure if this is entirely right */
		break;
	}
}

#ifdef STANDALONE
int main(int argc, char **argv)
{
	int fd;
	struct _header_scan_state *s;
	char *data;
	size_t len;
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
		s = folder_scan_init();
		folder_scan_init_with_fd(s, fd);
		s->scan_from = FALSE;
#if 0
		h = g_malloc0(sizeof(*h));
		h->savestate = CAMEL_MIME_PARSER_STATE_EOF;
		folder_push_part(s, h);
#endif	
		while (s->state != CAMEL_MIME_PARSER_STATE_EOF) {
			folder_scan_step(s, &data, &len);
			printf("\n -- PARSER STEP RETURN -- %d '%s'\n\n", s->state, states[s->state]);
			switch (s->state) {
			case CAMEL_MIME_PARSER_STATE_HEADER:
				if (s->parts->content_type
				    && (charset = camel_content_type_param(s->parts->content_type, "charset"))) {
					if (g_ascii_strcasecmp(charset, "us-ascii")) {
#if 0
						folder_push_filter_charset(s, "UTF-8", charset);
#endif
					} else {
						charset = NULL;
					}
				} else {
					charset = NULL;
				}

				encoding = camel_header_raw_find(&s->parts->headers, "Content-transfer-encoding", 0);
				printf("encoding = '%s'\n", encoding);
				if (encoding && !strncasecmp(encoding, " base64", 7)) {
					printf("adding base64 filter\n");
					attachname = g_strdup_printf("attach.%d.%d", i, attach++);
#if 0
					folder_push_filter_save(s, attachname);
#endif
					g_free(attachname);
#if 0
					folder_push_filter_mime(s, 0);
#endif
				}
				if (encoding && !strncasecmp(encoding, " quoted-printable", 17)) {
					printf("adding quoted-printable filter\n");
					attachname = g_strdup_printf("attach.%d.%d", i, attach++);
#if 0
					folder_push_filter_save(s, attachname);
#endif
					g_free(attachname);
#if 0
					folder_push_filter_mime(s, 1);
#endif
				}

				break;
			case CAMEL_MIME_PARSER_STATE_BODY:
				printf("got body %d '%.*s'\n",  len, len, data);
				break;
			case CAMEL_MIME_PARSER_STATE_BODY_END:
				printf("end body %d '%.*s'\n",  len, len, data);
				if (encoding && !strncasecmp(encoding, " base64", 7)) {
					printf("removing filters\n");
#if 0
					folder_filter_pull(s);
					folder_filter_pull(s);
#endif
				}
				if (encoding && !strncasecmp(encoding, " quoted-printable", 17)) {
					printf("removing filters\n");
#if 0
					folder_filter_pull(s);
					folder_filter_pull(s);
#endif
				}
				if (charset) {
#if 0
					folder_filter_pull(s);
#endif
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

