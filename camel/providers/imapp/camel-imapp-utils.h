
#ifndef _CAMEL_IMAPP_UTILS_H
#define _CAMEL_IMAPP_UTILS_H

#include <camel/camel-mime-utils.h>

/* FIXME: the enum should be split up into logical groups, so that testing
   can be done more accurately? */

/* list of strings we know about that can be *quickly* tokenised */
enum _imap_id {
	IMAP_UNKNOWN = 0,
	IMAP_ALERT,
	IMAP_BYE,
	IMAP_BAD,
	IMAP_NO,
	IMAP_OK,
	IMAP_PREAUTH,
	IMAP_NEWNAME,
	IMAP_PARSE,
	IMAP_PERMANENTFLAGS,
	IMAP_READ_ONLY,
	IMAP_READ_WRITE,
	IMAP_TRYCREATE,
	IMAP_UIDVALIDITY,
	IMAP_UNSEEN,
	IMAP_ENVELOPE,
	IMAP_FLAGS,
	IMAP_INTERNALDATE,
	IMAP_RFC822_HEADER,
	IMAP_RFC822_TEXT,
	IMAP_RFC822_SIZE,
	IMAP_BODYSTRUCTURE,
	IMAP_BODY,
	IMAP_UID,
};

/* str MUST be in upper case, tokenised using gperf function */
enum _imap_id imap_tokenise(register const char *str, register unsigned int len);

/* this flag should be part of imapfoldersummary */
enum {
	CAMEL_IMAPP_MESSAGE_RECENT = (1<<8),
};

/* ********************************************************************** */
void imap_parse_flags(CamelIMAPPStream *stream, guint32 *flagsp) /* IO,PARSE */;
void imap_write_flags(CamelStream *stream, guint32 flags) /* IO */;

/* ********************************************************************** */
void imap_parse_param_list(CamelIMAPPStream *is, struct _camel_header_param **plist) /* IO,PARSE */;
struct _CamelContentDisposition *imap_parse_ext_optional(CamelIMAPPStream *is) /* IO,PARSE */;
struct _CamelMessageContentInfo *imap_parse_body_fields(CamelIMAPPStream *is) /* IO,PARSE */;
struct _camel_header_address *imap_parse_address_list(CamelIMAPPStream *is) /* IO,PARSE */;
struct _CamelMessageInfo *imap_parse_envelope(CamelIMAPPStream *is) /* IO, PARSE */;
struct _CamelMessageContentInfo *imap_parse_body(CamelIMAPPStream *is) /* IO,PARSE */;
char *imap_parse_section(CamelIMAPPStream *is) /* IO,PARSE */;
void imap_free_body(struct _CamelMessageContentInfo *cinfo);

/* ********************************************************************** */
/* all the possible stuff we might get from a fetch request */
/* this assumes the caller/server doesn't send any one of these types twice */
struct _fetch_info {
	guint32 got;		/* what we got, see below */
	CamelStream *body;	/* BODY[.*](<.*>)? */
	CamelStream *text;	/* RFC822.TEXT */
	CamelStream *header;	/* RFC822.HEADER */
	struct _CamelMessageInfo *minfo; /* ENVELOPE */
	struct _CamelMessageContentInfo *cinfo;	/* BODYSTRUCTURE,BODY */
	guint32 size;		/* RFC822.SIZE */
	guint32 offset;		/* start offset of a BODY[]<offset.length> request */
	guint32 flags;		/* FLAGS */
	char *date;		/* INTERNALDATE */
	char *section;		/* section for a BODY[section] request */
	char *uid;		/* UID */
};

#define FETCH_BODY (1<<0)
#define FETCH_TEXT (1<<1)
#define FETCH_HEADER (1<<2)
#define FETCH_MINFO (1<<3)
#define FETCH_CINFO (1<<4)
#define FETCH_SIZE (1<<5)
#define FETCH_OFFSET (1<<6)
#define FETCH_FLAGS (1<<7)
#define FETCH_DATE (1<<8)
#define FETCH_SECTION (1<<9)
#define FETCH_UID (1<<10)

struct _fetch_info *imap_parse_fetch(CamelIMAPPStream *is);
void imap_free_fetch(struct _fetch_info *finfo);
void imap_dump_fetch(struct _fetch_info *finfo);

/* ********************************************************************** */

struct _status_info {
	enum _imap_id result; /* ok/no/bad/preauth only */
	enum _imap_id condition; /* read-only/read-write/alert/parse/trycreate/newname/permanentflags/uidvalidity/unseen */

	union {
		struct {
			char *oldname;
			char *newname;
		} newname;
		guint32 permanentflags;
		guint32 uidvalidity;
		guint32 unseen;
	} u;

	char *text;
};

struct _status_info *imap_parse_status(CamelIMAPPStream *is);
void imap_free_status(struct _status_info *sinfo);

/* ********************************************************************** */

/* should this just return a FolderInfo? 
   should this just return the name & flags & separator by reference? */
struct _list_info {
	guint32 flags:24;
	char separator;
	char *name;
};

struct _list_info *imap_parse_list(CamelIMAPPStream *is);
char *imapp_list_get_path(struct _list_info *li);
void imap_free_list(struct _list_info *linfo);

/* ********************************************************************** */

struct _uidset_state {
	struct _CamelIMAPPEngine *ie;
	int len;
	guint32 start;
	guint32 last;
};

struct _CamelIMAPPEngine;
struct _CamelIMAPPCommand;
void imapp_uidset_init(struct _uidset_state *ss, struct _CamelIMAPPEngine *ie);
int imapp_uidset_done(struct _uidset_state *ss, struct _CamelIMAPPCommand *ic);
int imapp_uidset_add(struct _uidset_state *ss, struct _CamelIMAPPCommand *ic, const char *uid);

#endif
