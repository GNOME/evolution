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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include <camel/camel-mime-parser.h>
#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-filter-basic.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-mime-filter-index.h>

#include <camel/camel-mime-utils.h>

#include "camel-mbox-summary.h"

#include <errno.h>
#include <ctype.h>
#include <netinet/in.h>

#define d(x)

#define CAMEL_MBOX_SUMMARY_VERSION 2

static int safe_write(int fd, char *buffer, size_t towrite);
static void camel_mbox_summary_add(CamelMboxSummary *s, CamelMboxMessageInfo *info);

/*
  Disk file format?

  message uid
message-block
  date:
  date received?

  subject: (unicode encoded)
  from: (unicode encoded)
  to: (unicode)

  content-block

content-block
  content-type: ; params;
  content-id:
  content-description:
  content-transfer-encoding:
  message-start:
  header-size:
  body-size:

  message-block
  multipart-block

 */

/* pah, i dont care, its almost no code and it works, dont need a glist */
struct _node {
	struct _node *next;
};

static struct _node *
my_list_append(struct _node **list, struct _node *n)
{
	struct _node *ln = (struct _node *)list;
	while (ln->next)
		ln = ln->next;
	n->next = 0;
	ln->next = n;
	return n;
}

static int
my_list_size(struct _node **list)
{
	int len = 0;
	struct _node *ln = (struct _node *)list;
	while (ln->next) {
		ln = ln->next;
		len++;
	}
	return len;
}

/* low-level io functions */
static int
encode_int (FILE *out, gint32 value)
{
	int i;

	for (i=28;i>0;i-=7) {
		if (value >= (1<<i)) {
			unsigned int c = (value>>i) & 0x7f;
			if (fputc(c, out) == -1)
				return -1;
		}
	}
	return fputc(value | 0x80, out);
}

static gint32
decode_int (FILE *in)
{
        gint32 value=0, v;

        /* until we get the last byte, keep decoding 7 bits at a time */
        while ( ((v = fgetc(in)) & 0x80) == 0 && v!=EOF) {
                value |= v;
                value <<= 7;
        }
        value |= (v&0x7f);
        return value;
}

static int
encode_fixed_int (FILE *out, gint32 value)
{
	guint32 save;

	save = htonl(value);
	return fwrite(&save, sizeof(save), 1, out);
}

static gint32
decode_fixed_int (FILE *out)
{
	guint32 save;

	if (fread(&save, sizeof(save), 1, out) != -1) {
		return ntohl(save);
	} else {
		return -1;
	}
}

/* should be sorted, for binary search */
/* This is a tokenisation mechanism for strings written to the
   summary - to save space.
   This list can have at most 31 words. */
static char * tokens[] = {
	"7bit",
	"8bit",
	"alternative",
	"application",
	"base64",
	"boundary",
	"charset",
	"filename",
	"html",
	"image",
	"iso-8859-1",
	"iso-8859-8",
	"message",
	"mixed",
	"multipart",
	"name",
	"octet-stream",
	"parallel",
	"plain",
	"quoted-printable",
	"rfc822",
	"text",
	"us-ascii",		/* 23 words */
};

#define tokens_len (sizeof(tokens)/sizeof(tokens[0]))

/* baiscally ...
    0 = null
    1-tokens_len == tokens[id-1]
    >=32 string, length = n-32
*/

static int
encode_string (FILE *out, char *str)
{
	if (str == NULL) {
		return encode_int(out, 0);
	} else {
		int len = strlen(str);
		int i, token=-1;

		if (len <= 16) {
			char lower[32];

			for (i=0;i<len;i++)
				lower[i] = tolower(str[i]);
			lower[i] = 0;
			for (i=0;i<tokens_len;i++) {
				if (!strcmp(tokens[i], lower)) {
					token = i;
					break;
				}
			}
		}
		if (token != -1) {
			return encode_int(out, token+1);
		} else {
			if (encode_int(out, len+32) == -1)
				return -1;
			return fwrite(str, len, 1, out);
		}
	}
	return 0;
}

static char *
decode_string (FILE *in)
{
	char *ret;
	int len;
	
	len = decode_int(in);

	if (len<32) {
		if (len <= 0) {
			ret = NULL;
		} else if (len<= tokens_len) {
			ret = g_strdup(tokens[len-1]);
		} else {
			g_warning("Invalid token encountered: %d", len);
			ret = NULL;
		}
	} else if (len > 10240) {
		g_warning("Got broken string header length: %d bytes", len);
		ret = NULL;
	} else {
		len -= 32;
		ret = g_malloc(len+1);
		if (fread(ret, len, 1, in) == -1) {
			g_free(ret);
			return NULL;
		}
		ret[len]=0;
	}

	return ret;
}



/* allocation functions */

static void
body_part_dump(CamelMboxMessageContentInfo *bs, int depth)
{
	CamelMboxMessageContentInfo *c;
	char *prefix;

	if (bs == NULL)
		return;

	prefix = alloca(depth*2+1);
	memset(prefix, ' ', depth*2);
	prefix[depth*2]=0;
	printf("%scontent-range: %d %d %d\n", prefix, (int)bs->pos, (int)bs->bodypos, (int)bs->endpos);
	printf("%scontent-type: %s/%s\n", prefix, bs->info.type?bs->info.type->type:"?", bs->info.type?bs->info.type->subtype:"?");
	printf("%scontent-id: %s\n", prefix, bs->info.id);
	printf("%scontent-description: %s\n", prefix, bs->info.description);
	printf("%scontent-transfer-encoding: %s\n", prefix, bs->info.encoding);
	c = (CamelMboxMessageContentInfo *)bs->info.childs;
	while (c) {
		printf("%s -- \n", prefix);
		body_part_dump(c, depth+1);
		c = (CamelMboxMessageContentInfo *)c->info.next;
	}
}

static void
message_struct_dump(CamelMboxMessageInfo *ms)
{
	char *tmp;

	if (ms == NULL) {
		printf("Empty message?\n");
		return;
	}

	printf("Subject: %s\n", ms->info.subject);
	printf("From: %s\n", ms->info.from);
	printf("To: %s\n", ms->info.to);
	tmp = header_format_date(ms->info.date_sent, 0);
	printf("Date: %s\n", tmp);
	g_free(tmp);
	tmp = header_format_date(ms->info.date_received, 0);
	printf("Date-Received: %s\n", tmp);
	g_free(tmp);
	printf("UID: %08x-%04x\n", atoi(ms->info.uid), ms->info.flags);
	printf(" -- content ->\n");
	body_part_dump((CamelMboxMessageContentInfo *)ms->info.content, 1);
}

static CamelMboxMessageContentInfo *
body_part_new(CamelMimeParser *mp, CamelMboxMessageContentInfo *parent, int start, int body)
{
	CamelMboxMessageContentInfo *bs;

	bs = g_malloc0(sizeof(*bs));

	bs->info.parent = (CamelMessageContentInfo *)parent;

	bs->info.type = camel_mime_parser_content_type(mp);
	header_content_type_ref(bs->info.type);

	bs->info.id = header_msgid_decode(camel_mime_parser_header(mp, "content-id", NULL));
	bs->info.description = header_decode_string(camel_mime_parser_header(mp, "content-description", NULL));
	bs->info.encoding = header_content_encoding_decode(camel_mime_parser_header(mp, "content-transfer-encoding", NULL));

	/* not sure what to set here? */
	bs->pos = start;
	bs->bodypos = body;
	bs->endpos = -1;

	if (parent)
		my_list_append((struct _node **)&parent->info.childs, (struct _node *)bs);

	return bs;
}

static CamelMboxMessageInfo *
message_struct_new(CamelMimeParser *mp, CamelMboxMessageContentInfo *parent, int start, int body, off_t xev_offset)
{
	CamelMboxMessageInfo *ms;
	struct _header_address *addr;
	const char *text;

	ms = g_malloc0(sizeof(*ms));

	/* FIXME: what about cc, sender vs from? */
	ms->info.subject = header_decode_string(camel_mime_parser_header(mp, "subject", NULL));
	text = camel_mime_parser_header(mp, "from", NULL);
	addr = header_address_decode(text);
	if (addr) {
		ms->info.from = header_address_list_format(addr);
		header_address_list_clear(&addr);
	} else {
		ms->info.from = g_strdup(text);
	}
	text = camel_mime_parser_header(mp, "to", NULL);
	addr = header_address_decode(text);
	if (addr) {
		ms->info.to = header_address_list_format(addr);
		header_address_list_clear(&addr);
	} else {
		ms->info.to = g_strdup(text);
	}

	ms->info.date_sent = header_decode_date(camel_mime_parser_header(mp, "date", NULL), NULL);
	ms->info.date_received = 0;

	ms->info.content = (CamelMessageContentInfo *)body_part_new(mp, parent, start, body);
	ms->xev_offset = xev_offset;
	return ms;
}

static void
body_part_free(CamelMboxMessageContentInfo *bs)
{
	CamelMboxMessageContentInfo *c, *cn;

	c = (CamelMboxMessageContentInfo *)bs->info.childs;
	while (c) {
		cn = (CamelMboxMessageContentInfo *)c->info.next;
		body_part_free(c);
		c = cn;
	}
	g_free(bs->info.id);
	g_free(bs->info.description);
	g_free(bs->info.encoding);
	header_content_type_unref(bs->info.type);
	g_free(bs);
}

static void
message_struct_free(CamelMboxMessageInfo *ms)
{
	g_free(ms->info.subject);
	g_free(ms->info.to);
	g_free(ms->info.from);
	body_part_free((CamelMboxMessageContentInfo *)ms->info.content);
	g_free(ms);
}


/* IO functions */
static CamelMboxMessageContentInfo *
body_part_load(FILE *in)
{
	CamelMboxMessageContentInfo *bs = NULL, *c;
	struct _header_content_type *ct;
	char *type;
	char *subtype;
	int i, count;

	d(printf("got content-block\n"));
	bs = g_malloc0(sizeof(*bs));
	bs->pos = decode_int(in);
	bs->bodypos = bs->pos + decode_int(in);
	bs->endpos = bs->pos + decode_int(in);

	/* do content type */
	d(printf("got content-type\n"));
	type = decode_string(in);
	subtype = decode_string(in);

	ct = header_content_type_new(type, subtype);
	bs->info.type = ct;
	count = decode_int(in);
	d(printf("getting %d params\n", count));
	for (i=0;i<count;i++) {
		char *name = decode_string(in);
		char *value = decode_string(in);
		
		d(printf(" %s = \"%s\"\n", name, value));
		
		header_content_type_set_param(ct, name, value);
		/* FIXME: do this so we dont have to double alloc/free */
		g_free(name);
		g_free(value);
	}

	d(printf("got content-id\n"));
	bs->info.id = decode_string(in);
	d(printf("got content-description\n"));
	bs->info.description = decode_string(in);
	d(printf("got content-encoding\n"));
	bs->info.encoding = decode_string(in);

	count = decode_int(in);
	d(printf("got children, %d\n", count));
	for (i=0;i<count;i++) {
		c = body_part_load(in);
		if (c) {
			my_list_append((struct _node **)&bs->info.childs, (struct _node *)c);
			c->info.parent = (CamelMessageContentInfo *)bs;
		} else {
			printf("Cannot load child\n");
		}
	}

	return bs;
}

static int
body_part_save(FILE *out, CamelMboxMessageContentInfo *bs)
{
	CamelMboxMessageContentInfo *c, *cn;
	struct _header_content_type *ct;
	struct _header_param *hp;

	encode_int(out, bs->pos);
	encode_int(out, bs->bodypos - bs->pos);
	encode_int(out, bs->endpos - bs->pos);

	ct = bs->info.type;
	if (ct) {
		encode_string(out, ct->type);
		encode_string(out, ct->subtype);
		encode_int(out, my_list_size((struct _node **)&ct->params));
		hp = ct->params;
		while (hp) {
			encode_string(out, hp->name);
			encode_string(out, hp->value);
			hp = hp->next;
		}
	} else {
		encode_string(out, NULL);
		encode_string(out, NULL);
		encode_int(out, 0);
	}
	encode_string(out, bs->info.id);
	encode_string(out, bs->info.description);
	encode_string(out, bs->info.encoding);

	encode_int(out, my_list_size((struct _node **)&bs->info.childs));

	c = (CamelMboxMessageContentInfo *)bs->info.childs;
	while (c) {
		cn = (CamelMboxMessageContentInfo *)c->info.next;
		body_part_save(out, c);
		c = cn;
	}

	return 0;
}

static CamelMboxMessageInfo *
message_struct_load(FILE *in)
{
	CamelMboxMessageInfo *ms;

	ms = g_malloc0(sizeof(*ms));

	ms->info.uid = g_strdup_printf("%u", decode_int(in));
	ms->info.flags = decode_int(in);
	ms->info.date_sent = decode_int(in);
	ms->info.date_received = decode_int(in);
	ms->xev_offset = decode_int(in);
	ms->info.subject = decode_string(in);
	ms->info.from = decode_string(in);
	ms->info.to = decode_string(in);
	ms->info.content = (CamelMessageContentInfo *)body_part_load(in);

	return ms;
}

static int
message_struct_save(FILE *out, CamelMboxMessageInfo *ms)
{
	encode_int(out, strtoul(ms->info.uid, NULL, 10));
	encode_int(out, ms->info.flags);
	encode_int(out, ms->info.date_sent);
	encode_int(out, ms->info.date_received);
	encode_int(out, ms->xev_offset);
	encode_string(out, ms->info.subject);
	encode_string(out, ms->info.from);
	encode_string(out, ms->info.to);
	body_part_save(out, (CamelMboxMessageContentInfo *)ms->info.content);

	return 0;
}

static unsigned int
header_evolution_decode(const char *in, unsigned int *uid, unsigned int *flags)
{
	char *header;
	if (in
	    && (header = header_token_decode(in))) {
		if (strlen(header) == strlen("00000000-0000")
		    && sscanf(header, "%08x-%04x", uid, flags) == 2) {
			g_free(header);
			return *uid;
		}
		g_free(header);
	}

	return ~0;
}

static int
safe_write(int fd, char *buffer, size_t towrite)
{
	size_t donelen;
	size_t len;

	donelen = 0;
	while (donelen < towrite) {
		len = write(fd, buffer + donelen, towrite - donelen);
		if (len == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return -1;
		}
		donelen += len;
	}
	return donelen;
}

static int
header_write(int fd, struct _header_raw *header, unsigned int uid, unsigned int flags)
{
	struct iovec iv[4];
	int outlen = 0;

	iv[1].iov_base = ":";
	iv[1].iov_len = 1;
	iv[3].iov_base = "\n";
	iv[3].iov_len = 1;

	while (header) {
		if (strcasecmp(header->name, "x-evolution")) {
			int len;

			iv[0].iov_base = header->name;
			iv[0].iov_len = strlen(header->name);
			iv[2].iov_base = header->value;
			iv[2].iov_len = strlen(header->value);

			do {
				len = writev(fd, iv, 4);
			} while (len == -1 && errno == EINTR);

			if (len == -1)
				return -1;
			outlen += len;
		}
		header = header->next;
	}

	return outlen;
}

/* returns -1 on error, else number of bytes written */
int
camel_mbox_summary_copy_block(int fromfd, int tofd, off_t readpos, size_t bytes)
{
	char buffer[4096];
	int written = 0;
	off_t pos, newpos;

	pos = lseek(fromfd, 0, SEEK_CUR);
	if (pos == -1)
		return -1;

	newpos = lseek(fromfd, readpos, SEEK_SET);
	if (newpos == -1 || newpos != readpos)
		goto error;

	d(printf("oldpos = %d;  copying %d from %d\n", (int)pos, (int)bytes, (int)readpos));

	while (bytes>0) {
		int toread, towrite, donelen;

		toread = bytes;
		if (bytes>4096)
			toread = 4096;
		else
			toread = bytes;
	reread:
		towrite = read(fromfd, buffer, toread);
		if (towrite == -1) {
			if (errno == EINTR || errno == EAGAIN)
				goto reread;
			goto error;
		}

		/* check for 'end of file' */
		if (towrite == 0)
			break;

		if ( (donelen = safe_write(tofd, buffer, towrite)) == -1)
			goto error;

		written += donelen;
		bytes -= donelen;
	}

	d(printf("written %d bytes\n", written));

	newpos = lseek(fromfd, pos, SEEK_SET);
	if (newpos == -1 || newpos != pos);
		return -1;

	return written;

error:
	lseek(fromfd, pos, SEEK_SET);
	return -1;
}

#define SAVEIT

static int index_folder(CamelMboxSummary *s, int startoffset)
{
	CamelMimeParser *mp;
	int fd;
	int fdout;
	int state;

	int toplevel = FALSE;
	const char *xev;
	char *data;
	int datalen;

	int enc_id=-1;
	int chr_id=-1;
	int idx_id=-1;
	struct _header_content_type *ct;
	int doindex=FALSE;
	CamelMimeFilterCharset *mfc = NULL;
	CamelMimeFilterIndex *mfi = NULL;
	CamelMimeFilterBasic *mf64 = NULL, *mfqp = NULL;

	CamelMboxMessageContentInfo *body = NULL, *parent = NULL;
	CamelMboxMessageInfo *message = NULL;

	int from_end = 0;	/* start of message */
	int from = 0;		/* start of headers */
	int last_write = 0;	/* last written position */
	int eof;
	int write_offset = 0;	/* how much does the dest differ from the source pos */
	int old_offset = 0;

	guint32 newuid;
	off_t xevoffset = -1;

	char *tmpname;

	printf("indexing %s (%s) from %d\n", s->folder_path, s->summary_path, startoffset);

	fd = open(s->folder_path, O_RDONLY);
	if (fd==-1) {
		perror("Can't open folder");
		return -1;
	}

	tmpname = g_strdup_printf("%s.tmp", s->folder_path);

	fdout = open(tmpname, O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (fdout==-1) {
		perror("Can't open output");
		g_free(tmpname);
		return -1;
	}

	mp = camel_mime_parser_new();
	camel_mime_parser_init_with_fd(mp, fd);
	camel_mime_parser_scan_from(mp, TRUE);

	/* FIXME: cleaner fail code */
	if (startoffset > 0) {
		if (camel_mime_parser_seek(mp, startoffset, SEEK_SET) != startoffset) {
			g_free(tmpname);
			gtk_object_unref((GtkObject *)mp);
			return -1;
		}
	}

	mfi = camel_mime_filter_index_new_ibex(s->index);

	while ( (state = camel_mime_parser_step(mp, &data, &datalen)) != HSCAN_EOF ) {
		switch(state) {
		case HSCAN_FROM: /* starting a new message content */
			/* save the current position */
			d(printf("from = %d\n", (int)camel_mime_parser_tell(mp)));
			toplevel = FALSE;
			from = camel_mime_parser_tell(mp);
			break;

		case HSCAN_FROM_END:
			d(printf("from-end = %d\n", (int)camel_mime_parser_tell(mp)));
			d(printf("message from %d to %d\n", from_end, (int)camel_mime_parser_tell(mp)));
			from_end = camel_mime_parser_tell(mp);
			break;

		case HSCAN_MESSAGE:
		case HSCAN_MULTIPART:
		case HSCAN_HEADER: /* starting a new header */
			newuid=~0;
			if (!toplevel) {
				char name[32];
				unsigned int olduid, oldflags;
				int headerlen;
				int docopy = FALSE;

				/* check for X-Evolution header ... if its there, nothing to do (skip content) */
				xev = camel_mime_parser_header(mp, "x-evolution", &xevoffset);
				if (xev) {
					d(printf("An x-evolution header exists at: %d = %s\n", xevoffset + write_offset, xev));
					xevoffset = xevoffset + write_offset;
					if (header_evolution_decode(xev, &olduid, &oldflags) != ~0) {
						d(printf(" uid = %d = %x\n", olduid, olduid));
						newuid = olduid;
#if 0
						while (camel_mime_parser_step(mp, &data, &datalen) != HSCAN_FROM_END)
							;
						break;
#endif
					} else {
						printf("Invalid xev header?  I need to write out a new one ...\n");
					}
				}

				toplevel = TRUE;

				/* assign a new uid for this message */
				if (newuid == ~0) {
					newuid = s->nextuid++;
					docopy = TRUE;
				} else {
					/* make sure we account for this uid when assigning uid's */
					/* this really needs a pre-scan pass ... *sigh* */
					camel_mbox_summary_set_uid(s, newuid);
				}

				/* setup index name for this uid */
				sprintf(name, "%x", newuid);
				camel_mime_filter_index_set_name(mfi, name);
				/* remove all references to this name from the index */
				if (s->index)
					ibex_unindex(s->index, name);

				d(printf("Message content starts at %d\n", camel_mime_parser_tell(mp)));
				
				if (docopy) {
					/* now, copy over bits of mbox from last write, and insert the X-Evolution header (at the top of headers) */
					/* if we already have a valid x-evolution header, use that, dont need to copy */
					camel_mbox_summary_copy_block(fd, fdout, last_write, from-last_write);
					last_write = from;

					headerlen = header_write(fdout, camel_mime_parser_headers_raw(mp), newuid, 0);
					sprintf(name, "X-Evolution: %08x-%04x\n\n", newuid, 0);
					safe_write(fdout, name, strlen(name));
					d(printf("new X-Evolution at %d\n", headerlen + from + write_offset));
					xevoffset = headerlen + from + write_offset;
					old_offset = write_offset;

					write_offset += (headerlen - (camel_mime_parser_tell(mp)-from)) + strlen(name);
					last_write = camel_mime_parser_tell(mp);
				}
			} else {
				old_offset = write_offset;
			}

			/* we only care about the rest for actual content parts */
			/* TODO: Cleanup, this is a huge mess */
			if (state != HSCAN_HEADER) {
				if (message == NULL) {
					message = message_struct_new(mp, parent, camel_mime_parser_tell_start_headers(mp)+old_offset, camel_mime_parser_tell(mp)+write_offset, xevoffset);
					parent = (CamelMboxMessageContentInfo *)message->info.content;
					if (newuid != ~0) {
						message->info.uid = g_strdup_printf("%u", newuid);
					} else {
						g_warning("This shouldn't happen?");
					}
				} else {
					parent = body_part_new(mp, parent, camel_mime_parser_tell_start_headers(mp)+old_offset, camel_mime_parser_tell(mp)+write_offset);
				}
				break;
			}

			if (message == NULL) {
				message = message_struct_new(mp, parent, camel_mime_parser_tell_start_headers(mp)+old_offset, camel_mime_parser_tell(mp)+write_offset, xevoffset);
				body = (CamelMboxMessageContentInfo *)message->info.content;
				if (newuid != ~0) {
					message->info.uid = g_strdup_printf("%u", newuid);
				} else {
					g_warning("This shouldn't happen?");
				}
			} else {
				body = body_part_new(mp, parent, camel_mime_parser_tell_start_headers(mp)+old_offset, camel_mime_parser_tell(mp)+write_offset);
			}

			/* check headers for types that we can index */
			ct = camel_mime_parser_content_type(mp);
			if (header_content_type_is(ct, "text", "*")) {
				char *encoding;
				const char *charset;
				
				/* TODO: The filters should all be cached, so they aren't recreated between
				   messages/message parts */
				encoding = header_content_encoding_decode(camel_mime_parser_header(mp, "content-transfer-encoding", NULL));
				if (encoding) {
					if (!strcasecmp(encoding, "base64")) {
						d(printf("Adding decoding filter for base64\n"));
						if (mf64 == NULL)
							mf64 = camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_BASE64_DEC);
						enc_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)mf64);
					} else if (!strcasecmp(encoding, "quoted-printable")) {
						d(printf("Adding decoding filter for quoted-printable\n"));
						if (mfqp == NULL)
							mfqp = camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_QP_DEC);
						enc_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)mfqp);
					}
					g_free(encoding);
				}
				
				charset = header_content_type_param(ct, "charset");
				if (charset!=NULL
				    && !(strcasecmp(charset, "us-ascii")==0
					|| strcasecmp(charset, "utf-8")==0)) {
					d(printf("Adding conversion filter from %s to utf-8\n", charset));
					if (mfc == NULL)
						mfc = camel_mime_filter_charset_new_convert(charset, "utf-8");
					if (mfc) {
						chr_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)mfc);
					} else {
						g_warning("Cannot convert '%s' to 'utf-8', message display may be corrupt", charset);
					}
				}

				doindex = TRUE;

				/* and this filter actually does the indexing */
				idx_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)mfi);
			} else {
				doindex = FALSE;
			}
			break;

			/* fixme, this needs thought *sigh* */
		case HSCAN_MESSAGE_END:
		case HSCAN_MULTIPART_END:
			if (parent) {
				parent->endpos = camel_mime_parser_tell(mp)+write_offset;
				if (parent->info.parent == NULL) {
					camel_mbox_summary_add(s, message);
					message = NULL;
					parent = NULL;
				} else {
					parent = (CamelMboxMessageContentInfo *)parent->info.parent;
				}
			}
			break;

		case HSCAN_BODY:
			if (doindex) {
				d(printf("Got content to index:\n%.*s", datalen, data));
			}
			break;

		case HSCAN_BODY_END:
			if (body) {
				body->endpos = camel_mime_parser_tell(mp)+write_offset;
				if (body->info.parent == NULL) {
					camel_mbox_summary_add(s, message);
					message = NULL;
				}
			}

			d(printf("end of content, removing decoders\n"));
			if (enc_id != -1) {
				camel_mime_parser_filter_remove(mp, enc_id);
				enc_id = -1;
			}
			if (chr_id != -1) {
				camel_mime_parser_filter_remove(mp, chr_id);
				chr_id = -1;
			}
			if (idx_id != -1) {
				camel_mime_parser_filter_remove(mp, idx_id);
				idx_id = -1;
			}
			break;
		}
	}

	/* did we actually write anything out?   Then rename and be done with it. */
	if (last_write>0) {
		eof = camel_mime_parser_tell(mp);
		camel_mbox_summary_copy_block(fd, fdout, last_write, eof-last_write);

		if (close(fdout) == -1) {
			perror("Could not close output file");
			unlink(tmpname);
		} else {
			printf("renaming %s to %s\n", tmpname, s->folder_path);
			if (rename(tmpname, s->folder_path) == -1) {
				perror("Error renaming file");
				unlink(tmpname);
			}
		}
	} else {
		/* no, then dont bother touching the inbox */
		printf("No written changes to mbox, removing tmp file\n");
		close(fdout);
		unlink(tmpname);
	}

	close(fd);

	if (mf64) gtk_object_unref((GtkObject *)mf64);
	if (mfqp) gtk_object_unref((GtkObject *)mfqp);
	if (mfc) gtk_object_unref((GtkObject *)mfc);
	if (mfi) gtk_object_unref((GtkObject *)mfi);

	/* force an index sync? */
	if (s->index) {
		ibex_write(s->index);
	}

        gtk_object_unref((GtkObject *)mp);

	/* and finally ... update the summary sync info */
	{
		struct stat st;

		if (stat(s->folder_path, &st) == 0) {
			s->time = st.st_mtime;
			s->size = st.st_size;
		}
	}

	g_free(tmpname);

	return 0;
}

CamelMboxSummary *camel_mbox_summary_new(const char *summary, const char *folder, ibex *index)
{
	CamelMboxSummary *s;

	s = g_malloc0(sizeof(*s));

	s->dirty = TRUE;
	s->folder_path = g_strdup(folder);
	s->summary_path = g_strdup(summary);
	/* FIXME: refcount index? */
	s->index = index;

	s->messages = g_ptr_array_new();
	s->message_uid = g_hash_table_new(g_str_hash, g_str_equal);

	/* always force an update */
	s->time = 0;
	s->size = 0;

	s->nextuid = 1;

	/* TODO: force an initial load right now? */

	return s;
}

void camel_mbox_summary_unref(CamelMboxSummary *s)
{
	g_warning("Unimplemented function, mbox_summary_unref");
}

/* check that the summary is uptodate, TRUE means it is uptodate */
int camel_mbox_summary_check(CamelMboxSummary *s)
{
	struct stat st;

	/* no folder at all? */
	if (stat(s->folder_path, &st) != 0)
		return FALSE;

	return (st.st_size == s->size) && (st.st_mtime == s->time);
}

static void camel_mbox_summary_add(CamelMboxSummary *s, CamelMboxMessageInfo *info)
{
	if (info->info.uid == NULL) {
		info->info.uid = g_strdup_printf("%u", s->nextuid++);
	}
	if (g_hash_table_lookup(s->message_uid, info->info.uid)) {
		g_error("Trying to insert message with clashing uid's");
	}
	d(printf("adding %s\n", info->info.uid));
	g_ptr_array_add(s->messages, info);
	g_hash_table_insert(s->message_uid, info->info.uid, info);
	s->dirty = TRUE;
}

static int summary_header_read(FILE *fp, guint32 *version, time_t *time, size_t *size, guint32 *nextuid)
{
	fseek(fp, 0, SEEK_SET);
	*version = decode_fixed_int(fp);
	*time = decode_fixed_int(fp);
	*size = decode_fixed_int(fp);
	*nextuid = decode_fixed_int(fp);
	return ferror(fp);
}

static void
summary_clear(CamelMboxSummary *s)
{
	int i;

	for (i=0;i<s->messages->len;i++) {
		message_struct_free(g_ptr_array_index(s->messages, i));
	}
	g_ptr_array_free(s->messages, TRUE);
	g_hash_table_destroy(s->message_uid);

	s->messages = g_ptr_array_new();
	s->message_uid = g_hash_table_new(g_str_hash, g_str_equal);
}

int camel_mbox_summary_load(CamelMboxSummary *s)
{
	struct stat st;
	FILE *fp;
	int i, total;
	CamelMboxMessageInfo *info;

	summary_clear(s);

	if ((fp = fopen(s->summary_path, "r")) == NULL) {
		g_warning("Loading non-existant summary, generating summary for folder: %s: %s", s->summary_path, strerror(errno));
		index_folder(s, 0);
		camel_mbox_summary_save(s);
	} else {
		guint32 version, nextuid;
		time_t time;
		size_t size;

		if (stat(s->folder_path, &st) != 0) {
			g_warning("Uh, no folder anyway, aborting");
			fclose(fp);
			return -1;
		}

		if (summary_header_read(fp, &version, &time, &size, &nextuid) != 0
		    || version != CAMEL_MBOX_SUMMARY_VERSION) {
			g_warning("Summary missing or version mismatch, reloading summary");
			fclose(fp);
			index_folder(s, 0);
			camel_mbox_summary_save(s);
			return 0;
		}

		s->nextuid = MAX(s->nextuid, nextuid);
		s->time = time;
		s->size = size;
		total = decode_fixed_int(fp);
			
		if (time != st.st_mtime || size != st.st_size) {
			/* if its grown, then just index the new stuff, and load the rest from the summary */
			if (size < st.st_size) {
				g_warning("Indexing/summarizing from start position: %d", size);

				d(printf("loading %d items from summary file\n", total));
				for (i=0;i<total;i++) {
					info = message_struct_load(fp);
					if (info) {
						camel_mbox_summary_add(s, info);
					} else {
						break;
					}
				}
				fclose(fp);
				s->dirty = FALSE;
				index_folder(s, size); /* if it adds any, it'll dirtify it */
				camel_mbox_summary_save(s);
			} else {
				g_warning("Folder changed/smaller, reindexing everything");
				index_folder(s, 0);
				camel_mbox_summary_save(s);
				fclose(fp);
			}
			return 0;
		}

		printf("loading %d items from summary file\n", total);
		for (i=0;i<total;i++) {
			info = message_struct_load(fp);
			if (info) {
				camel_mbox_summary_add(s, info);
			} else {
				break;
			}
		}
		fclose(fp);
		s->dirty = FALSE;
	}
	return 0;
}

static int summary_header_write(FILE *fp, CamelMboxSummary *s)
{
	fseek(fp, 0, SEEK_SET);
	encode_fixed_int(fp, CAMEL_MBOX_SUMMARY_VERSION);
	encode_fixed_int(fp, s->time);
	/* if we're dirty, then dont *really* save it ... */
	if (s->dirty)
		encode_fixed_int(fp, 0);
	else
		encode_fixed_int(fp, s->size);
	encode_fixed_int(fp, s->nextuid);
	fflush(fp);
	return ferror(fp);
}

static int summary_header_save(CamelMboxSummary *s)
{
	int fd;
	FILE *fp;

	fd = open(s->summary_path, O_WRONLY|O_CREAT, 0600);
	if (fd == -1)
		return -1;
	fp = fdopen(fd, "w");
	if (fp == NULL)
		return -1;

	summary_header_write(fp, s);
	return fclose(fp);
}

int camel_mbox_summary_save(CamelMboxSummary *s)
{
	int i, fd;
	FILE *fp;

	printf("saving summary? %s\n", s->summary_path);

	/* FIXME: error checking */
	if (s->dirty) {
		printf("yes\n");
		fd = open(s->summary_path, O_WRONLY|O_CREAT|O_TRUNC, 0600);
		if (fd == -1)
			return -1;
		fp = fdopen(fd, "w");
		if (fp == NULL)
			return -1;

		s->dirty = FALSE;

		summary_header_write(fp, s);
		encode_fixed_int(fp, s->messages->len);

		printf("message count = %d\n", s->messages->len);

		for (i=0;i<s->messages->len;i++) {
			message_struct_save(fp, g_ptr_array_index(s->messages, i));
		}
		fclose(fp);
	} else {
		printf("no\n");
	}
	return 0;
}

CamelMboxMessageInfo *camel_mbox_summary_uid(CamelMboxSummary *s, const char *uid)
{
	return g_hash_table_lookup(s->message_uid, uid);
}

CamelMboxMessageInfo *camel_mbox_summary_index(CamelMboxSummary *s, int index)
{
	return g_ptr_array_index(s->messages, index);
}

int camel_mbox_summary_message_count(CamelMboxSummary *s)
{
	return s->messages->len;
}

guint32 camel_mbox_summary_next_uid(CamelMboxSummary *s)
{
	guint32 uid = s->nextuid++;

	summary_header_save(s);
	return uid;
}

guint32 camel_mbox_summary_set_uid(CamelMboxSummary *s, guint32 uid)
{
	if (s->nextuid <= uid) {
		s->nextuid = uid+1;
		summary_header_save(s);
	}
	return s->nextuid;
}

#if 0
int main(int argc, char **argv)
{
	if (argc<2) {
		printf("usage: %s mbox\n", argv[0]);
		return 1;
	}

	gtk_init(&argc, &argv);

	index_folder(argv[1]);

	return 0;
}
#endif
