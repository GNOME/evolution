
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <camel/camel-folder-summary.h>
#include <camel/camel-store.h>
#include <camel/camel-utf8.h>
#include <camel/camel-string-utils.h>

#include "camel-imapp-folder.h"
#include "camel-imapp-stream.h"
#include "camel-imapp-utils.h"
#include "camel-imapp-exception.h"
#include "camel-imapp-engine.h"
#include "libedataserver/e-memory.h"

/* high-level parser state */
#define p(x)
/* debug */
#define d(x)

/* ANSI-C code produced by gperf version 2.7 */
/* Command-line: gperf -H imap_hash -N imap_tokenise -L ANSI-C -o -t -k1,$ imap-tokens.txt  */
struct _imap_keyword { char *name; enum _imap_id id; };
/*
 gperf input file
 best hash generated using: gperf -o -s-2 -k1,'$' -t -H imap_hash -N imap_tokenise -L ANSI-C 
*/

#define TOTAL_KEYWORDS 23
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 14
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 38
/* maximum key range = 37, duplicates = 0 */

#ifdef __GNUC__
__inline
#endif
static unsigned int
imap_hash (register const char *str, register unsigned int len)
{
  static unsigned char asso_values[] =
    {
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 10, 15, 39, 20,  0,
       0, 39,  0, 10, 39,  0, 39, 39, 10,  0,
       0, 39,  0, 10,  5, 10, 39, 39, 39,  0,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
      39, 39, 39, 39, 39, 39
    };
  return len + asso_values[(unsigned char)str[len - 1]] + asso_values[(unsigned char)str[0]];
}

#ifdef __GNUC__
__inline
#endif
enum _imap_id
imap_tokenise (register const char *str, register unsigned int len)
{
  static struct _imap_keyword wordlist[] =
    {
      {""}, {""},
      {"OK",		IMAP_OK},
      {""}, {""},
      {"PARSE",		IMAP_PARSE},
      {""},
      {"PREAUTH",	IMAP_PREAUTH},
      {"ENVELOPE",	IMAP_ENVELOPE},
      {"READ-ONLY",	IMAP_READ_ONLY},
      {"READ-WRITE",	IMAP_READ_WRITE},
      {"RFC822.SIZE",	IMAP_RFC822_SIZE},
      {"NO",		IMAP_NO},
      {"RFC822.HEADER",	IMAP_RFC822_HEADER},
      {"TRYCREATE",	IMAP_TRYCREATE},
      {"FLAGS",		IMAP_FLAGS},
      {"RFC822.TEXT",	IMAP_RFC822_TEXT},
      {"NEWNAME",	IMAP_NEWNAME},
      {"BYE",		IMAP_BYE},
      {"BODY",		IMAP_BODY},
      {"ALERT",          IMAP_ALERT},
      {"UIDVALIDITY",	IMAP_UIDVALIDITY},
      {"INTERNALDATE",	IMAP_INTERNALDATE},
      {""},
      {"PERMANENTFLAGS",	IMAP_PERMANENTFLAGS},
      {""},
      {"UNSEEN",		IMAP_UNSEEN},
      {""},
      {"BODYSTRUCTURE",	IMAP_BODYSTRUCTURE},
      {""}, {""}, {""}, {""},
      {"UID",		IMAP_UID},
      {""}, {""}, {""}, {""},
      {"BAD",		IMAP_BAD}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = imap_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return wordlist[key].id;
        }
    }
  return 0;
}

/* flag table */
static struct {
	char *name;
	guint32 flag;
} flag_table[] = {
	{ "\\ANSWERED", CAMEL_MESSAGE_ANSWERED },
	{ "\\DELETED", CAMEL_MESSAGE_DELETED },
	{ "\\DRAFT", CAMEL_MESSAGE_DRAFT },
	{ "\\FLAGGED", CAMEL_MESSAGE_FLAGGED },
	{ "\\SEEN", CAMEL_MESSAGE_SEEN },
	{ "\\RECENT", CAMEL_IMAPP_MESSAGE_RECENT },
	{ "\\*", CAMEL_MESSAGE_USER },
};

/* utility functions
   shoudl this be part of imapp-driver? */
/* mabye this should be a stream op? */
void
imap_parse_flags(CamelIMAPPStream *stream, guint32 *flagsp)
/* throws IO,PARSE exception */
{
	int tok, len, i;
	unsigned char *token, *p, c;
	guint32 flags = 0;

	*flagsp = flags;

	tok = camel_imapp_stream_token(stream, &token, &len);
	if (tok == '(') {
		do {
			tok = camel_imapp_stream_token(stream, &token, &len);
			if (tok == IMAP_TOK_TOKEN) {
				p = token;
				while ((c=*p))
					*p++ = toupper(c);
				for (i=0;i<(int)(sizeof(flag_table)/sizeof(flag_table[0]));i++)
					if (!strcmp(token, flag_table[i].name))
						flags |= flag_table[i].flag;
			} else if (tok != ')') {
				camel_exception_throw(1, "expecting flag");
			}
		} while (tok != ')');
	} else {
		camel_exception_throw(1, "expecting flag list");
	}

	*flagsp = flags;
}

void
imap_write_flags(CamelStream *stream, guint32 flags)
/* throws IO exception */
{
	int i;

	/* all this ugly exception throwing goes away once camel streams throw their own? */
	if (camel_stream_write(stream, "(", 1) == -1)
		camel_exception_throw(1, "io error: %s", strerror(errno));

	for (i=0;flags!=0 && i<(int)(sizeof(flag_table)/sizeof(flag_table[0]));i++) {
		if (flag_table[i].flag & flags) {
			if (camel_stream_write(stream, flag_table[i].name, strlen(flag_table[i].name)) == -1)
				camel_exception_throw(1, "io error: %s", strerror(errno));
			flags &= ~flag_table[i].flag;
			if (flags != 0)
				if (camel_stream_write(stream, " ", 1) == -1)
					camel_exception_throw(1, "io error: %s", strerror(errno));
		}
	}

	if (camel_stream_write(stream, ")", 1) == -1)
		camel_exception_throw(1, "io error: %s", strerror(errno));
}

/*
body            ::= "(" body_type_1part / body_type_mpart ")"

body_extension  ::= nstring / number / "(" 1#body_extension ")"
                    ;; Future expansion.  Client implementations
                    ;; MUST accept body_extension fields.  Server
                    ;; implementations MUST NOT generate
                    ;; body_extension fields except as defined by
                    ;; future standard or standards-track
                    ;; revisions of this specification.

body_ext_1part  ::= body_fld_md5 [SPACE body_fld_dsp
                    [SPACE body_fld_lang
                    [SPACE 1#body_extension]]]
                    ;; MUST NOT be returned on non-extensible
                    ;; "BODY" fetch

body_ext_mpart  ::= body_fld_param
                    [SPACE body_fld_dsp SPACE body_fld_lang
                    [SPACE 1#body_extension]]
                    ;; MUST NOT be returned on non-extensible
                    ;; "BODY" fetch

body_fields     ::= body_fld_param SPACE body_fld_id SPACE
                    body_fld_desc SPACE body_fld_enc SPACE
                    body_fld_octets

body_fld_desc   ::= nstring

body_fld_dsp    ::= "(" string SPACE body_fld_param ")" / nil

body_fld_enc    ::= (<"> ("7BIT" / "8BIT" / "BINARY" / "BASE64"/
                    "QUOTED-PRINTABLE") <">) / string

body_fld_id     ::= nstring

body_fld_lang   ::= nstring / "(" 1#string ")"

body_fld_lines  ::= number

body_fld_md5    ::= nstring

body_fld_octets ::= number

body_fld_param  ::= "(" 1#(string SPACE string) ")" / nil

body_type_1part ::= (body_type_basic / body_type_msg / body_type_text)
                    [SPACE body_ext_1part]

body_type_basic ::= media_basic SPACE body_fields
                    ;; MESSAGE subtype MUST NOT be "RFC822"

body_type_mpart ::= 1*body SPACE media_subtype
                    [SPACE body_ext_mpart]

body_type_msg   ::= media_message SPACE body_fields SPACE envelope
                    SPACE body SPACE body_fld_lines

body_type_text  ::= media_text SPACE body_fields SPACE body_fld_lines

envelope        ::= "(" env_date SPACE env_subject SPACE env_from
                    SPACE env_sender SPACE env_reply_to SPACE env_to
                    SPACE env_cc SPACE env_bcc SPACE env_in_reply_to
                    SPACE env_message_id ")"

env_bcc         ::= "(" 1*address ")" / nil

env_cc          ::= "(" 1*address ")" / nil

env_date        ::= nstring

env_from        ::= "(" 1*address ")" / nil

env_in_reply_to ::= nstring

env_message_id  ::= nstring

env_reply_to    ::= "(" 1*address ")" / nil

env_sender      ::= "(" 1*address ")" / nil

env_subject     ::= nstring

env_to          ::= "(" 1*address ")" / nil

media_basic     ::= (<"> ("APPLICATION" / "AUDIO" / "IMAGE" /
                    "MESSAGE" / "VIDEO") <">) / string)
                    SPACE media_subtype
                    ;; Defined in [MIME-IMT]

media_message   ::= <"> "MESSAGE" <"> SPACE <"> "RFC822" <">
                    ;; Defined in [MIME-IMT]

media_subtype   ::= string
                    ;; Defined in [MIME-IMT]

media_text      ::= <"> "TEXT" <"> SPACE media_subtype
                    ;; Defined in [MIME-IMT]



 ( "type" "subtype"  body_fields [envelope body body_fld_lines]
                                 [body_fld_lines]



 (("TEXT" "PLAIN" ("CHARSET"
                     "US-ASCII") NIL NIL "7BIT" 1152 23)("TEXT" "PLAIN"
                     ("CHARSET" "US-ASCII" "NAME" "cc.diff")
                     "<960723163407.20117h@cac.washington.edu>"
                     "Compiler diff" "BASE64" 4554 73) "MIXED"))

*/

/*
struct _body_fields {
	CamelContentType *ct;
	char *msgid, *desc;
	CamelTransferEncoding encoding;
	guint32 size;
	};*/

void
imap_free_body(struct _CamelMessageContentInfo *cinfo)
{
	struct _CamelMessageContentInfo *list, *next;

	list = cinfo->childs;
	while (list) {
		next = list->next;
		imap_free_body(list);
		list = next;
	}
	
	if (cinfo->type)
		camel_content_type_unref(cinfo->type);
	g_free(cinfo->id);
	g_free(cinfo->description);
	g_free(cinfo->encoding);
	g_free(cinfo);
}

void
imap_parse_param_list(CamelIMAPPStream *is, struct _camel_header_param **plist)
{
	int tok, len;
	unsigned char *token, *param;

	p(printf("body_fld_param\n"));

	/* body_fld_param  ::= "(" 1#(string SPACE string) ")" / nil */
	tok = camel_imapp_stream_token(is, &token, &len);
	if (tok == '(') {
		while (1) {
			tok = camel_imapp_stream_token(is, &token, &len);
			if (tok == ')')
				break;
			camel_imapp_stream_ungettoken(is, tok, token, len);
			
			camel_imapp_stream_astring(is, &token);
			param = alloca(strlen(token)+1);
			strcpy(param, token);
			camel_imapp_stream_astring(is, &token);
			camel_header_set_param(plist, param, token);
		}
	} /* else check nil?  no need */
}

struct _CamelContentDisposition *
imap_parse_ext_optional(CamelIMAPPStream *is)
{
	int tok, len;
	unsigned char *token;
	struct _CamelContentDisposition * volatile dinfo = NULL;

	/* this parses both extension types, from the body_fld_dsp onwards */
	/* although the grammars are different, they can be parsed the same way */

	/* body_ext_1part  ::= body_fld_md5 [SPACE body_fld_dsp
	   [SPACE body_fld_lang
	   [SPACE 1#body_extension]]]
	   ;; MUST NOT be returned on non-extensible
	   ;; "BODY" fetch */

	/* body_ext_mpart  ::= body_fld_param
	   [SPACE body_fld_dsp SPACE body_fld_lang
	   [SPACE 1#body_extension]]
	   ;; MUST NOT be returned on non-extensible
	   ;; "BODY" fetch */

	CAMEL_TRY {
		/* body_fld_dsp    ::= "(" string SPACE body_fld_param ")" / nil */

		tok = camel_imapp_stream_token(is, &token, &len);
		switch (tok) {
		case '(':
			dinfo = g_malloc0(sizeof(*dinfo));
			dinfo->refcount = 1;
			/* should be string */
			camel_imapp_stream_astring(is, &token);
		
			dinfo->disposition = g_strdup(token);
			imap_parse_param_list(is, &dinfo->params);
		case IMAP_TOK_TOKEN:
			d(printf("body_fld_dsp: NIL\n"));
			break;
		default:
			camel_exception_throw(1, "body_fld_disp: expecting nil or list");
		}
	
		p(printf("body_fld_lang\n"));

		/* body_fld_lang   ::= nstring / "(" 1#string ")" */
	
		/* we just drop the lang string/list, save it somewhere? */
	
		tok = camel_imapp_stream_token(is, &token, &len);
		switch (tok) {
		case '(':
			while (1) {
				tok = camel_imapp_stream_token(is, &token, &len);
				if (tok == ')') {
					break;
				} else if (tok != IMAP_TOK_STRING) {
					camel_exception_throw(1, "expecting string");
				}
			}
			break;
		case IMAP_TOK_TOKEN:
			d(printf("body_fld_lang = nil\n"));
			/* treat as 'nil' */
			break;
		case IMAP_TOK_STRING:
			/* we have a string */
			break;
		case IMAP_TOK_LITERAL:
			/* we have a literal string */
			camel_imapp_stream_set_literal(is, len);
			while ((tok = camel_imapp_stream_getl(is, &token, &len)) > 0) {
				d(printf("Skip literal data '%.*s'\n", (int)len, token));
			}
			break;

		}
	} CAMEL_CATCH(ex) {
		if (dinfo)
			camel_content_disposition_unref(dinfo);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	return dinfo;
}

struct _CamelMessageContentInfo *
imap_parse_body_fields(CamelIMAPPStream *is)
{
	unsigned char *token, *type;
	struct _CamelMessageContentInfo *cinfo;
		
	/* body_fields     ::= body_fld_param SPACE body_fld_id SPACE
	   body_fld_desc SPACE body_fld_enc SPACE
	   body_fld_octets */

	p(printf("body_fields\n"));

	cinfo = g_malloc0(sizeof(*cinfo));

	CAMEL_TRY {
		/* this should be string not astring */
		camel_imapp_stream_astring(is, &token);
		type = alloca(strlen(token)+1);
		strcpy(type, token);
		camel_imapp_stream_astring(is, &token);
		cinfo->type = camel_content_type_new(type, token);
		imap_parse_param_list(is, &cinfo->type->params);
	
		/* body_fld_id     ::= nstring */
		camel_imapp_stream_nstring(is, &token);
		cinfo->id = g_strdup(token);
	
		/* body_fld_desc   ::= nstring */
		camel_imapp_stream_nstring(is, &token);
		cinfo->description = g_strdup(token);
	
		/* body_fld_enc    ::= (<"> ("7BIT" / "8BIT" / "BINARY" / "BASE64"/
		   "QUOTED-PRINTABLE") <">) / string */
		camel_imapp_stream_astring(is, &token);
		cinfo->encoding = g_strdup(token);
	
		/* body_fld_octets ::= number */
		cinfo->size = camel_imapp_stream_number(is);
	} CAMEL_CATCH(ex) {
		imap_free_body(cinfo);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	return cinfo;
}

struct _camel_header_address *
imap_parse_address_list(CamelIMAPPStream *is)
/* throws PARSE,IO exception */
{
	int tok, len;
	unsigned char *token, *host, *mbox;
	struct _camel_header_address *list = NULL;

	/* "(" 1*address ")" / nil */

	CAMEL_TRY {
		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok == '(') {
			while (1) {
				struct _camel_header_address *addr, *group = NULL;

				/* address         ::= "(" addr_name SPACE addr_adl SPACE addr_mailbox
				   SPACE addr_host ")" */
				tok = camel_imapp_stream_token(is, &token, &len);
				if (tok == ')')
					break;
				if (tok != '(')
					camel_exception_throw(1, "missing '(' for address");

				addr = camel_header_address_new();
				addr->type = CAMEL_HEADER_ADDRESS_NAME;
				tok = camel_imapp_stream_nstring(is, &token);
				addr->name = g_strdup(token);
				/* we ignore the route, nobody uses it in the real world */
				tok = camel_imapp_stream_nstring(is, &token);

				/* [RFC-822] group syntax is indicated by a special
				   form of address structure in which the host name
				   field is NIL.  If the mailbox name field is also
				   NIL, this is an end of group marker (semi-colon in
				   RFC 822 syntax).  If the mailbox name field is
				   non-NIL, this is a start of group marker, and the
				   mailbox name field holds the group name phrase. */

				tok = camel_imapp_stream_nstring(is, &mbox);
				mbox = g_strdup(mbox);
				tok = camel_imapp_stream_nstring(is, &host);
				if (host == NULL) {
					if (mbox == NULL) {
						group = NULL;
					} else {
						d(printf("adding group '%s'\n", mbox));
						g_free(addr->name);
						addr->name = mbox;
						addr->type = CAMEL_HEADER_ADDRESS_GROUP;
						camel_header_address_list_append(&list, addr);
						group = addr;
					}
				} else {
					addr->v.addr = g_strdup_printf("%s%s%s", mbox?(char *)mbox:"", host?"@":"", host?(char *)host:"");
					g_free(mbox);
					d(printf("adding address '%s'\n", addr->v.addr));
					if (group != NULL)
						camel_header_address_add_member(group, addr);
					else
						camel_header_address_list_append(&list, addr);
				}
				do {
					tok = camel_imapp_stream_token(is, &token, &len);
				} while (tok != ')');
			}
		} else {
			d(printf("empty, nil '%s'\n", token));
		}
	} CAMEL_CATCH(ex) {
		camel_header_address_list_clear(&list);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	return list;
}

struct _CamelMessageInfo *
imap_parse_envelope(CamelIMAPPStream *is)
{
	int tok, len;
	unsigned char *token;
	struct _camel_header_address *addr, *addr_from;
	char *addrstr;
	struct _CamelMessageInfoBase *minfo;

	/* envelope        ::= "(" env_date SPACE env_subject SPACE env_from
	   SPACE env_sender SPACE env_reply_to SPACE env_to
	   SPACE env_cc SPACE env_bcc SPACE env_in_reply_to
	   SPACE env_message_id ")" */

	p(printf("envelope\n"));

	minfo = (CamelMessageInfoBase *)camel_message_info_new(NULL);

	CAMEL_TRY {
		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok != '(')
			camel_exception_throw(1, "envelope: expecting '('");

		/* env_date        ::= nstring */
		camel_imapp_stream_nstring(is, &token);
		minfo->date_sent = camel_header_decode_date(token, NULL);

		/* env_subject     ::= nstring */
		tok = camel_imapp_stream_nstring(is, &token);
		minfo->subject = camel_pstring_strdup(token);

		/* we merge from/sender into from, append should probably merge more smartly? */

		/* env_from        ::= "(" 1*address ")" / nil */
		addr_from = imap_parse_address_list(is);

		/* env_sender      ::= "(" 1*address ")" / nil */
		addr = imap_parse_address_list(is);
		if (addr_from) {
			camel_header_address_list_clear(&addr);
#if 0
			if (addr)
				camel_header_address_list_append_list(&addr_from, &addr);
#endif
		} else {
			if (addr)
				addr_from = addr;
		}

		if (addr_from) {
			addrstr = camel_header_address_list_format(addr_from);
			minfo->from = camel_pstring_strdup(addrstr);
			g_free(addrstr);
			camel_header_address_list_clear(&addr_from);
		}

		/* we dont keep reply_to */

		/* env_reply_to    ::= "(" 1*address ")" / nil */
		addr = imap_parse_address_list(is);
		camel_header_address_list_clear(&addr);

		/* env_to          ::= "(" 1*address ")" / nil */
		addr = imap_parse_address_list(is);
		if (addr) {
			addrstr = camel_header_address_list_format(addr);
			minfo->to = camel_pstring_strdup(addrstr);
			g_free(addrstr);
			camel_header_address_list_clear(&addr);
		}

		/* env_cc          ::= "(" 1*address ")" / nil */
		addr = imap_parse_address_list(is);
		if (addr) {
			addrstr = camel_header_address_list_format(addr);
			minfo->cc = camel_pstring_strdup(addrstr);
			g_free(addrstr);
			camel_header_address_list_clear(&addr);
		}

		/* we dont keep bcc either */

		/* env_bcc         ::= "(" 1*address ")" / nil */
		addr = imap_parse_address_list(is);
		camel_header_address_list_clear(&addr);

		/* FIXME: need to put in-reply-to into references hash list */

		/* env_in_reply_to ::= nstring */
		tok = camel_imapp_stream_nstring(is, &token);

		/* FIXME: need to put message-id into message-id hash */

		/* env_message_id  ::= nstring */
		tok = camel_imapp_stream_nstring(is, &token);

		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok != ')')
			camel_exception_throw(1, "expecting ')'");
	} CAMEL_CATCH(ex) {
		camel_message_info_free(minfo);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	return (CamelMessageInfo *)minfo;
}
	
struct _CamelMessageContentInfo *
imap_parse_body(CamelIMAPPStream *is)
{
	int tok, len;
	unsigned char *token;
	struct _CamelMessageContentInfo * volatile cinfo = NULL;
	struct _CamelMessageContentInfo *subinfo, *last;
	struct _CamelContentDisposition * volatile dinfo = NULL;
	struct _CamelMessageInfo * volatile minfo = NULL;

	/* body            ::= "(" body_type_1part / body_type_mpart ")" */

	p(printf("body\n"));

	CAMEL_TRY {
		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok != '(')
			camel_exception_throw(1, "body: expecting '('");

		/* 1*body (optional for multiparts) */
		tok = camel_imapp_stream_token(is, &token, &len);
		camel_imapp_stream_ungettoken(is, tok, token, len);
		if (tok == '(') {
			/* body_type_mpart ::= 1*body SPACE media_subtype
			   [SPACE body_ext_mpart] */

			cinfo = g_malloc0(sizeof(*cinfo));
			last = (struct _CamelMessageContentInfo *)&cinfo->childs;
			do {
				subinfo = imap_parse_body(is);
				last->next = subinfo;
				last = subinfo;
				subinfo->parent = cinfo;
				tok = camel_imapp_stream_token(is, &token, &len);
				camel_imapp_stream_ungettoken(is, tok, token, len);
			} while (tok == '(');

			d(printf("media_subtype\n"));

			camel_imapp_stream_astring(is, &token);
			cinfo->type = camel_content_type_new("multipart", token);

			/* body_ext_mpart  ::= body_fld_param
			   [SPACE body_fld_dsp SPACE body_fld_lang
			   [SPACE 1#body_extension]]
			   ;; MUST NOT be returned on non-extensible
			   ;; "BODY" fetch */

			d(printf("body_ext_mpart\n"));

			tok = camel_imapp_stream_token(is, &token, &len);
			camel_imapp_stream_ungettoken(is, tok, token, len);
			if (tok == '(') {
				imap_parse_param_list(is, &cinfo->type->params);

				/* body_fld_dsp    ::= "(" string SPACE body_fld_param ")" / nil */

				tok = camel_imapp_stream_token(is, &token, &len);
				camel_imapp_stream_ungettoken(is, tok, token, len);
				if (tok == '(' || tok == IMAP_TOK_TOKEN) {
					dinfo = imap_parse_ext_optional(is);
					/* other extension fields?, soaked up below */
				} else {
					camel_imapp_stream_ungettoken(is, tok, token, len);
				}
			}
		} else {
			/* body_type_1part ::= (body_type_basic / body_type_msg / body_type_text)
			   [SPACE body_ext_1part]
			   
			   body_type_basic ::= media_basic SPACE body_fields
			   body_type_text  ::= media_text SPACE body_fields SPACE body_fld_lines
			   body_type_msg   ::= media_message SPACE body_fields SPACE envelope
			   SPACE body SPACE body_fld_lines */

			d(printf("Single part body\n"));

			cinfo = imap_parse_body_fields(is);

			d(printf("envelope?\n"));

			/* do we have an envelope following */
			tok = camel_imapp_stream_token(is, &token, &len);
			camel_imapp_stream_ungettoken(is, tok, token, len);
			if (tok == '(') {
				/* what do we do with the envelope?? */
				minfo = imap_parse_envelope(is);
				/* what do we do with the message content info?? */
				((CamelMessageInfoBase *)minfo)->content = imap_parse_body(is);
				camel_message_info_free(minfo);
				minfo = NULL;
				d(printf("Scanned envelope - what do i do with it?\n"));
			}

			d(printf("fld_lines?\n"));

			/* do we have fld_lines following? */
			tok = camel_imapp_stream_token(is, &token, &len);
			if (tok == IMAP_TOK_INT) {
				d(printf("field lines: %s\n", token));
				tok = camel_imapp_stream_token(is, &token, &len);
			}
			camel_imapp_stream_ungettoken(is, tok, token, len);

			/* body_ext_1part  ::= body_fld_md5 [SPACE body_fld_dsp
			   [SPACE body_fld_lang
			   [SPACE 1#body_extension]]]
			   ;; MUST NOT be returned on non-extensible
			   ;; "BODY" fetch */

			d(printf("extension data?\n"));

			if (tok != ')') {
				camel_imapp_stream_nstring(is, &token);

				d(printf("md5: %s\n", token?(char *)token:"NIL"));

				/* body_fld_dsp    ::= "(" string SPACE body_fld_param ")" / nil */

				tok = camel_imapp_stream_token(is, &token, &len);
				camel_imapp_stream_ungettoken(is, tok, token, len);
				if (tok == '(' || tok == IMAP_TOK_TOKEN) {
					dinfo = imap_parse_ext_optional(is);
					/* then other extension fields, soaked up below */
				}
			}
		}

		/* soak up any other extension fields that may be present */
		/* there should only be simple tokens, no lists */
		do {
			tok = camel_imapp_stream_token(is, &token, &len);
			if (tok != ')')
				d(printf("Dropping extension data '%s'\n", token));
		} while (tok != ')');
	} CAMEL_CATCH(ex) {
		if (cinfo)
			imap_free_body(cinfo);
		if (dinfo)
			camel_content_disposition_unref(dinfo);
		if (minfo)
			camel_message_info_free(minfo);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	/* FIXME: do something with the disposition, currently we have no way to pass it out? */
	if (dinfo)
		camel_content_disposition_unref(dinfo);

	return cinfo;
}

char *
imap_parse_section(CamelIMAPPStream *is)
{
	int tok, len;
	unsigned char *token;
	char * volatile section = NULL;

	/* currently we only return the part within the [section] specifier
	   any header fields are parsed, but dropped */

	/*
	  section         ::= "[" [section_text /
	  (nz_number *["." nz_number] ["." (section_text / "MIME")])] "]"

	  section_text    ::= "HEADER" / "HEADER.FIELDS" [".NOT"]
	  SPACE header_list / "TEXT"
	*/

	CAMEL_TRY {
		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok != '[')
			camel_exception_throw(1, "section: expecting '['");

		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok == IMAP_TOK_INT || tok == IMAP_TOK_TOKEN)
			section = g_strdup(token);
		else if (tok == ']') {
			section = g_strdup("");
			camel_imapp_stream_ungettoken(is, tok, token, len);
		} else
			camel_exception_throw(1, "section: expecting token");

		/* header_list     ::= "(" 1#header_fld_name ")"
		   header_fld_name ::= astring */

		/* we dont need the header specifiers */
		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok == '(') {
			do {
				tok = camel_imapp_stream_token(is, &token, &len);
				if (tok == IMAP_TOK_STRING || tok == IMAP_TOK_TOKEN || tok == IMAP_TOK_INT) {
					/* ?do something? */
				} else if (tok != ')')
					camel_exception_throw(1, "section: header fields: expecting string");
			} while (tok != ')');
			tok = camel_imapp_stream_token(is, &token, &len);
		}

		if (tok != ']')
			camel_exception_throw(1, "section: expecting ']'");
	} CAMEL_CATCH(ex) {
		g_free(section);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	return section;
}

void
imap_free_fetch(struct _fetch_info *finfo)
{
	if (finfo == NULL)
		return;

	if (finfo->body)
		camel_object_unref((CamelObject *)finfo->body);
	if (finfo->text)
		camel_object_unref((CamelObject *)finfo->text);
	if (finfo->header)
		camel_object_unref((CamelObject *)finfo->header);
	if (finfo->minfo)
		camel_message_info_free(finfo->minfo);
	if (finfo->cinfo)
		imap_free_body(finfo->cinfo);
	g_free(finfo->date);
	g_free(finfo->section);
	g_free(finfo->uid);
	g_free(finfo);
}

extern void camel_content_info_dump(CamelMessageContentInfo *ci, int depth);
extern void camel_message_info_dump(CamelMessageInfo *mi);

#include <camel/camel-stream-fs.h>

/* debug, dump one out */
void
imap_dump_fetch(struct _fetch_info *finfo)
{
	CamelStream *sout;
	int fd;

	printf("Fetch info:\n");
	if (finfo == NULL) {
		printf("Empty\n");
		return;
	}

	fd = dup(1);
	sout = camel_stream_fs_new_with_fd(fd);
	if (finfo->body) {
		camel_stream_printf(sout, "Body content:\n");
		camel_stream_write_to_stream(finfo->body, sout);
	}
	if (finfo->text) {
		camel_stream_printf(sout, "Text content:\n");
		camel_stream_write_to_stream(finfo->text, sout);
	}
	if (finfo->header) {
		camel_stream_printf(sout, "Header content:\n");
		camel_stream_write_to_stream(finfo->header, sout);
	}
	if (finfo->minfo) {
		camel_stream_printf(sout, "Message Info:\n");
		camel_message_info_dump(finfo->minfo);
	}
	if (finfo->cinfo) {
		camel_stream_printf(sout, "Content Info:\n");
		camel_content_info_dump(finfo->cinfo, 0);
	}
	if (finfo->got & FETCH_SIZE)
		camel_stream_printf(sout, "Size: %d\n", (int)finfo->size);
	if (finfo->got & FETCH_BODY)
		camel_stream_printf(sout, "Offset: %d\n", (int)finfo->offset);
	if (finfo->got & FETCH_FLAGS)
		camel_stream_printf(sout, "Flags: %08x\n", (int)finfo->flags);
	if (finfo->date)
		camel_stream_printf(sout, "Date: '%s'\n", finfo->date);
	if (finfo->section)
		camel_stream_printf(sout, "Section: '%s'\n", finfo->section);
	if (finfo->date)
		camel_stream_printf(sout, "UID: '%s'\n", finfo->uid);
	camel_object_unref((CamelObject *)sout);
}

struct _fetch_info *
imap_parse_fetch(CamelIMAPPStream *is)
{
	int tok, len;
	unsigned char *token, *p, c;
	struct _fetch_info *finfo;

	finfo = g_malloc0(sizeof(*finfo));

	CAMEL_TRY {
		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok != '(')
			camel_exception_throw(1, "fetch: expecting '('");

		while ( (tok = camel_imapp_stream_token(is, &token, &len)) == IMAP_TOK_TOKEN ) {

			p = token;
			while ((c=*p))
				*p++ = toupper(c);

			switch(imap_tokenise(token, len)) {
			case IMAP_ENVELOPE:
				finfo->minfo = imap_parse_envelope(is);
				finfo->got |= FETCH_MINFO;
				break;
			case IMAP_FLAGS:
				imap_parse_flags(is, &finfo->flags);
				finfo->got |= FETCH_FLAGS;
				break;
			case IMAP_INTERNALDATE:
				camel_imapp_stream_nstring(is, &token);
				/* TODO: convert to camel format? */
				finfo->date = g_strdup(token);
				finfo->got |= FETCH_DATE;
				break;
			case IMAP_RFC822_HEADER:
				camel_imapp_stream_nstring_stream(is, &finfo->header);
				finfo->got |= FETCH_HEADER;
				break;
			case IMAP_RFC822_TEXT:
				camel_imapp_stream_nstring_stream(is, &finfo->text);
				finfo->got |= FETCH_TEXT;
				break;
			case IMAP_RFC822_SIZE:
				finfo->size = camel_imapp_stream_number(is);
				finfo->got |= FETCH_SIZE;
				break;
			case IMAP_BODYSTRUCTURE:
				finfo->cinfo = imap_parse_body(is);
				finfo->got |= FETCH_CINFO;
				break;
			case IMAP_BODY:
				tok = camel_imapp_stream_token(is, &token, &len);
				camel_imapp_stream_ungettoken(is, tok, token, len);
				if (tok == '(') {
					finfo->cinfo = imap_parse_body(is);
					finfo->got |= FETCH_CINFO;
				} else if (tok == '[') {
					finfo->section = imap_parse_section(is);
					finfo->got |= FETCH_SECTION;
					tok = camel_imapp_stream_token(is, &token, &len);
					if (token[0] == '<') {
						finfo->offset = strtoul(token+1, NULL, 10);
					} else {
						camel_imapp_stream_ungettoken(is, tok, token, len);
					}
					camel_imapp_stream_nstring_stream(is, &finfo->body);
					finfo->got |= FETCH_BODY;					
				} else {
					camel_exception_throw(1, "unknown body response");
				}
				break;
			case IMAP_UID:
				tok = camel_imapp_stream_token(is, &token, &len);
				if (tok != IMAP_TOK_INT)
					camel_exception_throw(1, "uid not integer");
				finfo->uid = g_strdup(token);
				finfo->got |= FETCH_UID;
				break;
			default:
				camel_exception_throw(1, "unknown body response");
			}
		}

		if (tok != ')')
			camel_exception_throw(1, "missing closing ')' on fetch response");
	} CAMEL_CATCH(ex) {
		imap_free_fetch(finfo);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	return finfo;
}

/* rfc 2060 section 7.1 Status Responses */
/* shoudl this start after [ or before the [? token_unget anyone? */
struct _status_info *
imap_parse_status(CamelIMAPPStream *is)
{
	int tok, len;
	unsigned char *token;
	struct _status_info *sinfo;

	sinfo = g_malloc0(sizeof(*sinfo));

	CAMEL_TRY {
		camel_imapp_stream_atom(is, &token, &len);

		/*
		  resp_cond_auth  ::= ("OK" / "PREAUTH") SPACE resp_text
		  ;; Authentication condition

		  resp_cond_bye   ::= "BYE" SPACE resp_text

		  resp_cond_state ::= ("OK" / "NO" / "BAD") SPACE resp_text
		  ;; Status condition
		*/

		sinfo->result = imap_tokenise(token, len);
		switch (sinfo->result) {
		case IMAP_OK:
		case IMAP_NO:
		case IMAP_BAD:
		case IMAP_PREAUTH:
		case IMAP_BYE:
			break;
		default:
			camel_exception_throw(1, "expecting OK/NO/BAD");
		}

		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok == '[') {
			camel_imapp_stream_atom(is, &token, &len);
			sinfo->condition = imap_tokenise(token, len);

			/* parse any details */
			switch (sinfo->condition) {
			case IMAP_READ_ONLY:
			case IMAP_READ_WRITE:
			case IMAP_ALERT:
			case IMAP_PARSE:
			case IMAP_TRYCREATE:
				break;
			case IMAP_NEWNAME:
				/* the rfc doesn't specify the bnf for this */
				camel_imapp_stream_astring(is, &token);
				sinfo->u.newname.oldname = g_strdup(token);
				camel_imapp_stream_astring(is, &token);
				sinfo->u.newname.newname = g_strdup(token);
				break;
			case IMAP_PERMANENTFLAGS:
				imap_parse_flags(is, &sinfo->u.permanentflags);
				break;
			case IMAP_UIDVALIDITY:
				sinfo->u.uidvalidity = camel_imapp_stream_number(is);
				break;
			case IMAP_UNSEEN:
				sinfo->u.unseen = camel_imapp_stream_number(is);
				break;
			default:
				sinfo->condition = IMAP_UNKNOWN;
				printf("Got unknown response code: %s: ignored\n", token);
			}
			
			/* ignore anything we dont know about */
			do {
				tok = camel_imapp_stream_token(is, &token, &len);
				if (tok == '\n')
					camel_exception_throw(1, "server response truncated");
			} while (tok != ']');
		} else {
			camel_imapp_stream_ungettoken(is, tok, token, len);
		}

		/* and take the human readable response */
		camel_imapp_stream_text(is, (unsigned char **)&sinfo->text);
	} CAMEL_CATCH(ex) {
		imap_free_status(sinfo);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	return sinfo;
}

void
imap_free_status(struct _status_info *sinfo)
{
	if (sinfo == NULL)
		return;

	switch (sinfo->condition) {
	case IMAP_NEWNAME:
		g_free(sinfo->u.newname.oldname);
		g_free(sinfo->u.newname.newname);
	default:
		break;
	}

	g_free(sinfo->text);
	g_free(sinfo);
}

/* FIXME: use tokeniser? */
/* FIXME: real flags */
static struct {
	char *name;
	guint32 flag;
} list_flag_table[] = {
	{ "\\NOINFERIORS", CAMEL_FOLDER_NOINFERIORS },
	{ "\\NOSELECT", CAMEL_FOLDER_NOSELECT },
	{ "\\MARKED", 1<<8 },
	{ "\\UNMARKED", 1<<9 },
};

struct _list_info *
imap_parse_list(CamelIMAPPStream *is)
/* throws io, parse */
{
	int tok, len, i;
	unsigned char *token, *p, c;
	struct _list_info * volatile linfo;

	linfo = g_malloc0(sizeof(*linfo));
	
	CAMEL_TRY {
		/* mailbox_list    ::= "(" #("\Marked" / "\Noinferiors" /
		   "\Noselect" / "\Unmarked" / flag_extension) ")"
		   SPACE (<"> QUOTED_CHAR <"> / nil) SPACE mailbox */

		tok = camel_imapp_stream_token(is, &token, &len);
		if (tok != '(')
			camel_exception_throw(1, "list: expecting '('");

		while ( (tok = camel_imapp_stream_token(is, &token, &len)) != ')' ) {
			if (tok == IMAP_TOK_STRING || tok == IMAP_TOK_TOKEN) {
				p = token;
				while ((c=*p))
					*p++ = toupper(c);
				for (i=0;i<(int)(sizeof(list_flag_table)/sizeof(list_flag_table[0]));i++)
					if (!strcmp(token, list_flag_table[i].name))
						linfo->flags |= list_flag_table[i].flag;
			} else {
				camel_exception_throw(1, "list: expecting flag or ')'");
			}
		}

		camel_imapp_stream_nstring(is, &token);
		linfo->separator = token?*token:0;
		camel_imapp_stream_astring(is, &token);
		linfo->name = g_strdup(token);
	} CAMEL_CATCH(ex) {
		imap_free_list(linfo);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;

	return linfo;
}

char *
imapp_list_get_path(struct _list_info *li)
{
	char *path, *p;
	int c;
	const char *f;

	if (li->separator != 0 && li->separator != '/') {
		p = path = alloca(strlen(li->name)*3+1);
		f = li->name;
		while ( (c = *f++ & 0xff) ) {
			if (c == li->separator)
				*p++ = '/';
			else if (c == '/' || c == '%')
				p += sprintf(p, "%%%02X", c);
			else
				*p++ = c;
		}
		*p = 0;
	} else
		path = li->name;

	return camel_utf7_utf8(path);
}

void
imap_free_list(struct _list_info *linfo)
{
	if (linfo) {
		g_free(linfo->name);
		g_free(linfo);
	}
}


/* ********************************************************************** */
/* utility functions */

/* should the rest of imapp-utils go into imapp-parse? */

/* this creates a uid (or sequence number) set directly into the command,
  optionally breaking it into smaller chunks */

void
imapp_uidset_init(struct _uidset_state *ss, CamelIMAPPEngine *ie)
{
	ss->ie = ie;
	ss->len = 0;
	ss->start = 0;
	ss->last = 0;
}

int
imapp_uidset_done(struct _uidset_state *ss, CamelIMAPPCommand *ic)
{
	int ret = 0;

	if (ss->last != 0 && ss->last != ss->start) {
		camel_imapp_engine_command_add(ss->ie, ic, ":%d", ss->last);
		printf(":%d", ss->last);
	}

	ret = ss->last != 0;

	ss->start = 0;
	ss->last = 0;
	ss->len = 0;

	return ret;
}

int
imapp_uidset_add(struct _uidset_state *ss, CamelIMAPPCommand *ic, const char *uid)
{
	guint32 uidn;

	uidn = strtoul(uid, NULL, 10);
	if (uidn == 0)
		return -1;

	if (ss->last == 0) {
		camel_imapp_engine_command_add(ss->ie, ic, "%d", uidn);
		printf("%d", uidn);
		ss->len ++;
		ss->start = uidn;
	} else {
		if (ss->last != uidn-1) {
			if (ss->last == ss->start) {
				camel_imapp_engine_command_add(ss->ie, ic, ",%d", uidn);
				printf(",%d", uidn);
				ss->len ++;
			} else {
				camel_imapp_engine_command_add(ss->ie, ic, ":%d,%d", ss->last, uidn);
				printf(":%d,%d", ss->last, uidn);
				ss->len+=2;
			}
			ss->start = uidn;
		}
	}

	ss->last = uidn;

	if (ss->len > 10) {
		imapp_uidset_done(ss, ic);
		return 1;
	}

	return 0;
}
