
#include "config.h"

#include <stdio.h>
#include <string.h>

#include "camel-imapp-engine.h"
#include "camel-imapp-stream.h"
#include "camel-imapp-utils.h"
#include "camel-imapp-exception.h"

#include <camel/camel-folder-summary.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-data-wrapper.h>
#include <camel/camel-sasl.h>

#include <ctype.h>

#define e(x) 
#define c(x)			/* command build debug */

static void imap_engine_command_addv(CamelIMAPPEngine *imap, CamelIMAPPCommand *ic, const char *fmt, va_list ap);
static void imap_engine_command_complete(CamelIMAPPEngine *imap, CamelIMAPPCommand *ic);

struct _handler {
	CamelIMAPPEngineFunc func;
	void *data;
	char name[1];
};

static void
class_init(CamelIMAPPEngineClass *ieclass)
{
	ieclass->tagprefix = 'A';

	camel_object_class_add_event((CamelObjectClass *)ieclass, "status", NULL);
}

static void
object_init(CamelIMAPPEngine *ie, CamelIMAPPEngineClass *ieclass)
{
	ie->handlers = g_hash_table_new(g_str_hash, g_str_equal);
	e_dlist_init(&ie->active);
	e_dlist_init(&ie->done);

	ie->port = e_msgport_new();

	ie->tagprefix = ieclass->tagprefix;
	ieclass->tagprefix++;
	if (ieclass->tagprefix > 'Z')
		ieclass->tagprefix = 'A';
	ie->tagprefix = 'A';

	ie->state = IMAP_ENGINE_DISCONNECT;
}

static void
handler_free(void *key, void *mem, void *data)
{
	g_free(mem);
}

static void
object_finalise(CamelIMAPPEngine *ie, CamelIMAPPEngineClass *ieclass)
{
	/* FIXME: need to free the commands ... */
	while (camel_imapp_engine_iterate(ie, NULL) > 0)
		;

	g_hash_table_foreach(ie->handlers, (GHFunc)handler_free, NULL);
	g_hash_table_destroy(ie->handlers);
}

CamelType
camel_imapp_engine_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_object_get_type (),
			"CamelIMAPPEngine",
			sizeof (CamelIMAPPEngine),
			sizeof (CamelIMAPPEngineClass),
			(CamelObjectClassInitFunc) class_init,
			NULL,
			(CamelObjectInitFunc) object_init,
			(CamelObjectFinalizeFunc) object_finalise);
	}
	
	return type;
}

/* FIXME: check this, just taken from old code, not rfc */
struct {
	char *name;
	guint32 flag;
} capa_table[] = {
	{ "IMAP4", IMAP_CAPABILITY_IMAP4 },
	{ "IMAP4REV1", IMAP_CAPABILITY_IMAP4REV1 },
	{ "STATUS",  IMAP_CAPABILITY_STATUS } ,
	{ "NAMESPACE", IMAP_CAPABILITY_NAMESPACE },
	{ "UIDPLUS",  IMAP_CAPABILITY_UIDPLUS },
	{ "LITERAL+", IMAP_CAPABILITY_LITERALPLUS },
	{ "STARTTLS", IMAP_CAPABILITY_STARTTLS },
};


/*
capability_data ::= "CAPABILITY" SPACE [1#capability SPACE] "IMAP4rev1"
                    [SPACE 1#capability]
                    ;; IMAP4rev1 servers which offer RFC 1730
                    ;; compatibility MUST list "IMAP4" as the first
                    ;; capability.
*/
static int resp_capability(CamelIMAPPEngine *ie, guint32 id, void *data)
{
	int tok, len, i;
	unsigned char *token, *p, c;

	/* FIXME: handle auth types */

	printf("got capability response:\n");
	while (1) {
		tok = camel_imapp_stream_token(ie->stream, &token, &len);
		switch(tok) {
		case IMAP_TOK_TOKEN:
			p = token;
			while ((c = *p))
				*p++ = toupper(c);
		case IMAP_TOK_INT:
			printf(" cap: '%s'\n", token);
			for (i=0;i<(int)(sizeof(capa_table)/sizeof(capa_table[0]));i++)
				if (strcmp(token, capa_table[i].name))
					ie->capa |= capa_table[i].flag;
			break;
		case '\n':
			return 0;
		case IMAP_TOK_ERROR:
		case IMAP_TOK_PROTOCOL:
			camel_imapp_engine_skip(ie);
			return -1;
		default:
			printf("Unknown Response token %02x '%c'\n", tok, isprint(tok)?tok:'.');
		}
	} while (tok != '\n');

	return 0;
}

/* expunge command, id is expunged seq number */
/* message_data    ::= nz_number SPACE ("EXPUNGE" /
   ("FETCH" SPACE msg_att)) */
static int resp_expunge(CamelIMAPPEngine *ie, guint32 id, void *data)
{
	printf("message expunged: %d\n", id);

	return camel_imapp_engine_skip(ie);
}

static int resp_flags(CamelIMAPPEngine *ie, guint32 id, void *data)
{
	guint32 flags;

	imap_parse_flags(ie->stream, &flags);

	printf("flags: %08x\n", flags);

	return camel_imapp_engine_skip(ie);
}

/* exists count */
static int resp_exists(CamelIMAPPEngine *ie, guint32 id, void *data)
{
	printf("messages exist: %d\n", id);

	if (ie->select_response)
		ie->select_response->exists = id;

	return camel_imapp_engine_skip(ie);
}

static int resp_recent(CamelIMAPPEngine *ie, guint32 id, void *data)
{
	printf("messages recent: %d\n", id);

	if (ie->select_response)
		ie->select_response->recent = id;

	return camel_imapp_engine_skip(ie);
}

static int resp_fetch(CamelIMAPPEngine *ie, guint32 id, void *data)
{
	struct _fetch_info *finfo;

	finfo = imap_parse_fetch(ie->stream);
	imap_dump_fetch(finfo);
	imap_free_fetch(finfo);

	return camel_imapp_engine_skip(ie);
}

#if 0
static int resp_list(CamelIMAPPEngine *ie, guint32 id, void *data)
{
	struct _list_info *linfo;

	linfo = imap_parse_list(ie->stream);
	printf("list:  '%s' (%c)\n", linfo->name, linfo->separator);
	imap_free_list(linfo);

	return camel_imapp_engine_skip(ie);
}
#endif

CamelIMAPPEngine *
camel_imapp_engine_new(CamelIMAPPStream *stream)
{
	CamelIMAPPEngine * volatile engine;

	engine = CAMEL_IMAPP_ENGINE (camel_object_new (CAMEL_IMAPP_ENGINE_TYPE));
	engine->stream = stream;
	camel_object_ref((CamelObject *)stream);

	camel_imapp_engine_add_handler(engine, "CAPABILITY", resp_capability, engine);

	/* mailbox_data */
	camel_imapp_engine_add_handler(engine, "FLAGS", (CamelIMAPPEngineFunc)resp_flags, engine);
	camel_imapp_engine_add_handler(engine, "EXISTS", (CamelIMAPPEngineFunc)resp_exists, engine);
	camel_imapp_engine_add_handler(engine, "RECENT", (CamelIMAPPEngineFunc)resp_recent, engine);

#if 0
	camel_imapp_engine_add_handler(engine, "LIST", (CamelIMAPPEngineFunc)resp_list, engine);
	camel_imapp_engine_add_handler(engine, "LSUB", (CamelIMAPPEngineFunc)resp_list, engine);
#endif
	/* message_data */
	camel_imapp_engine_add_handler(engine, "EXPUNGE", (CamelIMAPPEngineFunc)resp_expunge, engine);
	camel_imapp_engine_add_handler(engine, "FETCH", (CamelIMAPPEngineFunc)resp_fetch, engine);

	/* TODO: move this to a driver:connect call? */
	CAMEL_TRY {
		unsigned char *token;
		unsigned int len;
		int tok;

		tok = camel_imapp_stream_token(stream, &token, &len);
		if (tok == '*') {
			struct _status_info *sinfo = imap_parse_status(stream);

			switch (sinfo->result) {
			case IMAP_OK:
				engine->state = IMAP_ENGINE_CONNECT;
				printf("Server connected ok: %s\n", sinfo->text);
				break;
			case IMAP_PREAUTH:
				printf("pre-authenticated ...\n");
				engine->state = IMAP_ENGINE_AUTH;
				break;
			default:
				imap_free_status(sinfo);
				camel_exception_throw(1, "Server refused connection: %s", sinfo->text);
				break;
			}
			imap_free_status(sinfo);
		} else {
			engine->state = IMAP_ENGINE_CONNECT;
			printf("unknwon server greeting, ignored\n");
			camel_imapp_engine_skip(engine);
		}
		camel_imapp_engine_capabilities(engine);
	} CAMEL_CATCH(ex) {
		printf("connection failed: %s\n", ex->desc);
		camel_object_unref((CamelObject *)engine);
		engine = NULL;
	} CAMEL_DONE;

	return engine;
}

void
camel_imapp_engine_add_handler(CamelIMAPPEngine *imap, const char *response, CamelIMAPPEngineFunc func, void *data)
{
	struct _handler *h;
	const unsigned char *p;
	unsigned char *o, c;
	
	h = g_malloc0(sizeof(*h) + strlen(response));
	h->func = func;
	h->data = data;

	p = response;
	o = h->name;
	while ((c = *p++))
		*o++ = toupper(c);
	*o = 0;

	g_hash_table_insert(imap->handlers, h->name, h);
}

int
camel_imapp_engine_capabilities(CamelIMAPPEngine *ie)
{
	CamelIMAPPCommand *ic;

	/* reset capabilities */
	ie->capa = 0;

	ic = camel_imapp_engine_command_new(ie, "CAPABILITY", NULL, "CAPABILITY");
	camel_imapp_engine_command_queue(ie, ic);
	while (camel_imapp_engine_iterate(ie, ic)>0)
		;
	camel_imapp_engine_command_free(ie, ic);

	return 0;
}

/* skip the rest of the line of tokens */
int
camel_imapp_engine_skip(CamelIMAPPEngine *imap)
{
	int tok;
	unsigned char *token;
	unsigned int len;

	do {
		tok = camel_imapp_stream_token(imap->stream, &token, &len);
		if (tok == IMAP_TOK_LITERAL) {
			camel_imapp_stream_set_literal(imap->stream, len);
			while ((tok = camel_imapp_stream_getl(imap->stream, &token, &len)) > 0) {
				printf("Skip literal data '%.*s'\n", (int)len, token);
			}
		}
	} while (tok != '\n' && tok >= 0);

	if (tok < 0)
		return -1;

	return 0;
}

/* handle any untagged responses */
static int
iterate_untagged(CamelIMAPPEngine *imap)
{
	unsigned int id, len;
	unsigned char *token, *p, c;
	int tok;
	struct _handler *h;
	struct _status_info *sinfo;
	
	e(printf("got untagged response\n"));
	id = 0;
	tok = camel_imapp_stream_token(imap->stream, &token, &len);
	if (tok == IMAP_TOK_INT) {
		id = strtoul(token, NULL, 10);
		tok = camel_imapp_stream_token(imap->stream, &token, &len);
	}

	if (tok == '\n')
		camel_exception_throw(1, "truncated server response");

	e(printf("Have token '%s' id %d\n", token, id));
	p = token;
	while ((c = *p))
		*p++ = toupper(c);

	/* first, check for generic unsolicited response */
	h = g_hash_table_lookup(imap->handlers, token);
	if (h) {
		tok = h->func(imap, id, h->data);
		if (tok < 0)
			return tok;
		return 1;
	}

	/* TODO: apart from bye/preauth, these could be callbacks/events? */

	/* now, check for status responses */
	switch (imap_tokenise(token, len)) {
	case IMAP_BYE:
	case IMAP_OK:
	case IMAP_NO:
	case IMAP_BAD:
	case IMAP_PREAUTH:
		/* TODO: validate which ones of these can happen as unsolicited responses */
		/* TODO: handle bye/preauth differently */
		/* FIXME: free sinfo */
		camel_imapp_stream_ungettoken(imap->stream, tok, token, len);
		sinfo = imap_parse_status(imap->stream);
		camel_object_trigger_event(imap, "status", sinfo);
		imap_free_status(sinfo);
#if 0
		switch(sinfo->condition) {
		case IMAP_READ_WRITE:
			printf("folder is read-write\n");
			break;
		case IMAP_READ_ONLY:
			printf("folder is read-only\n");
			break;
		case IMAP_UIDVALIDITY:
			if (imap->select_response)
				imap->select_response->uidvalidity = sinfo->u.uidvalidity;
			break;
#if 0	
			/* not defined yet ... */
		case IMAP_UIDNEXT:
			printf("got uidnext for folder: %d\n", sinfo->u.uidnext);
			break;
#endif	
		case IMAP_UNSEEN:
			if (imap->select_response)
				imap->select_response->unseen = sinfo->u.unseen;
			break;
		case IMAP_PERMANENTFLAGS:
			if (imap->select_response)
				imap->select_response->permanentflags = sinfo->u.permanentflags;
			break;
		case IMAP_ALERT:
			printf("ALERT!: %s\n", sinfo->text);
			break;
		case IMAP_PARSE:
			printf("PARSE: %s\n", sinfo->text);
			break;
		default:
			break;
		}
#endif
		break;
	default:
		printf("unknown token: %s\n", token);
		camel_imapp_engine_skip(imap);
		/* unknown response, just ignore it */
	}

	return 1;
}

/* handle any continuation requests
   either data continuations, or auth continuation */
static int
iterate_continuation(CamelIMAPPEngine *imap)
{
	CamelIMAPPCommand *ic;
	CamelIMAPPCommandPart *cp;
	
	printf("got continuation response\n");

	ic = imap->literal;
	imap->literal = NULL;
	if (ic == NULL) {
		camel_imapp_engine_skip(imap);
		printf("got continuation response with no outstanding continuation requests?\n");
		return 1;
	}

	printf("got continuation response for data\n");
	cp = ic->current;
	switch(cp->type & CAMEL_IMAPP_COMMAND_MASK) {
	case CAMEL_IMAPP_COMMAND_DATAWRAPPER:
		printf("writing data wrapper to literal\n");
		camel_data_wrapper_write_to_stream((CamelDataWrapper *)cp->ob, (CamelStream *)imap->stream);
		break;
	case CAMEL_IMAPP_COMMAND_STREAM:
		printf("writing stream to literal\n");
		camel_stream_write_to_stream((CamelStream *)cp->ob, (CamelStream *)imap->stream);
		break;
	case CAMEL_IMAPP_COMMAND_AUTH: {
		CamelException *ex = camel_exception_new();
		char *resp;
		unsigned char *token;
		int tok, len;
		
		tok = camel_imapp_stream_token(imap->stream, &token, &len);
		resp = camel_sasl_challenge_base64((CamelSasl *)cp->ob, token, ex);
		if (camel_exception_is_set(ex))
			camel_exception_throw_ex(ex);
		camel_exception_free(ex);
		
		printf("got auth continuation, feeding token '%s' back to auth mech\n", resp);
		
		camel_stream_write((CamelStream *)imap->stream, resp, strlen(resp));
		
		/* we want to keep getting called until we get a status reponse from the server
		   ignore what sasl tells us */
		imap->literal = ic;
		
		break; }
	default:
		/* should we just ignore? */
		camel_exception_throw(1, "continuation response for non-continuation request");
	}
	
	camel_imapp_engine_skip(imap);
	
	cp = cp->next;
	if (cp->next) {
		ic->current = cp;
		printf("next part of command \"A%05u: %s\"\n", ic->tag, cp->data);
		camel_stream_printf((CamelStream *)imap->stream, "%s\r\n", cp->data);
		if (cp->type & CAMEL_IMAPP_COMMAND_CONTINUATION) {
			imap->literal = ic;
		} else {
			g_assert(cp->next->next == NULL);
		}
	} else {
		printf("%p: queueing continuation\n", ic);
		camel_stream_printf((CamelStream *)imap->stream, "\r\n");
	}
	
	if (imap->literal == NULL) {
		ic = (CamelIMAPPCommand *)e_dlist_remhead(&imap->queue);
		if (ic) {
			printf("found outstanding op, queueing\n");
			camel_imapp_engine_command_queue(imap, ic);
		}
	}

	return 1;
}

/* handle a completion line */
static int
iterate_completion(CamelIMAPPEngine *imap, unsigned char *token)
{
	CamelIMAPPCommand *ic;
	unsigned int tag;

	if (token[0] != imap->tagprefix)
		camel_exception_throw(1, "Server sent unexpected response: %s", token);

	tag = strtoul(token+1, NULL, 10);
	ic = camel_imapp_engine_command_find_tag(imap, tag);
	if (ic) {
		printf("Got completion response for command %05u '%s'\n", ic->tag, ic->name);
		printf("%p: removing command from qwueue, we were at '%s'\n", ic, ic->current->data);
		printf("%p: removing command\n", ic);
		e_dlist_remove((EDListNode *)ic);
		e_dlist_addtail(&imap->done, (EDListNode *)ic);
		if (imap->literal == ic)
			imap->literal = NULL;
		ic->status = imap_parse_status(imap->stream);
		printf("got response code: %s\n", ic->status->text);

		/* TODO: remove this stuff and use a completion handler? */
		/* TODO: handle 'SELECT' command cleanup here */
		/* FIXME: have this use tokeniser, have this handle close/logout/select etc as well */
		/* ok response from login/authenticate, then we're in happy land */
		if ((!strcmp(ic->name, "LOGIN") || !strcmp(ic->name, "AUTHENTICATE"))
		    && ic->status->result == IMAP_OK)
			imap->state = IMAP_ENGINE_AUTH;

		if (ic->complete)
			ic->complete(imap, ic, ic->complete_data);
	} else {
		camel_exception_throw(1, "got response tag unexpectedly: %s", token);
	}
	
	if (imap->literal != NULL) {
		printf("Warning: continuation command '%s' finished with outstanding continuation\n", imap->literal->name);
		ic = imap->literal;
				/* set the command complete with a failure code? */
		e_dlist_remove((EDListNode *)ic);
		e_dlist_addtail(&imap->done, (EDListNode *)ic);
		imap->literal = NULL;
	}
	
	ic = (CamelIMAPPCommand *)e_dlist_remhead(&imap->queue);
	if (ic) {
		printf("found outstanding op, queueing\n");
		camel_imapp_engine_command_queue(imap, ic);
	}
	
	return 1;
}


/* Do work if there's any to do */
int
camel_imapp_engine_iterate(CamelIMAPPEngine *imap, CamelIMAPPCommand *icwait)
/* throws IO,PARSE exception */
{
	unsigned int len;
	unsigned char *token;
	int tok;

	if ((icwait && icwait->status != NULL) || e_dlist_empty(&imap->active))
		return 0;

	/* handle exceptions here? */

	/* lock here? */

	tok = camel_imapp_stream_token(imap->stream, &token, &len);
	if (tok == '*')
		iterate_untagged(imap);
	else if (tok == IMAP_TOK_TOKEN)
		iterate_completion(imap, token);
	else if (tok == '+')
		iterate_continuation(imap);
	else
		camel_exception_throw(1, "unexpected server response: %s", token);

	if (e_dlist_empty(&imap->active))
		return 0;

	return 1;
}

CamelIMAPPCommand *
camel_imapp_engine_command_new(CamelIMAPPEngine *imap, const char *name, const char *select, const char *fmt, ...)
{
	CamelIMAPPCommand *ic;
	va_list ap;

	ic = g_malloc0(sizeof(*ic));
	ic->tag = imap->tag++;
	ic->name = name;
	ic->mem = (CamelStreamMem *)camel_stream_mem_new();
	ic->select = g_strdup(select);
	e_dlist_init(&ic->parts);

	if (fmt && fmt[0]) {
		va_start(ap, fmt);
		imap_engine_command_addv(imap, ic, fmt, ap);
		va_end(ap);
	}

	return ic;
}

void
camel_imapp_engine_command_add(CamelIMAPPEngine *imap, CamelIMAPPCommand *ic, const char *fmt, ...)
{
	va_list ap;

	g_assert(ic->mem);	/* gets reset on queue */

	if (fmt && fmt[0]) {
		va_start(ap, fmt);
		imap_engine_command_addv(imap, ic, fmt, ap);
		va_end(ap);
	}
}

void
camel_imapp_engine_command_complete(CamelIMAPPEngine *imap, struct _CamelIMAPPCommand *ic, CamelIMAPPCommandFunc func, void *data)
{
	ic->complete = func;
	ic->complete_data = data;
}

/* FIXME: make imap command's refcounted? */
void
camel_imapp_engine_command_free (CamelIMAPPEngine *imap, CamelIMAPPCommand *ic)
{
	CamelIMAPPCommandPart *cp, *cn;

	if (ic == NULL)
		return;

	/* Note the command must not be in any queue? */

	if (ic->mem)
		camel_object_unref((CamelObject *)ic->mem);
	imap_free_status(ic->status);
	g_free(ic->select);

	while ( (cp = ((CamelIMAPPCommandPart *)e_dlist_remhead(&ic->parts))) ) {
		g_free(cp->data);
		if (cp->ob)
			camel_object_unref(cp->ob);
		g_free(cp);
	}

	g_free(ic);
}

/* FIXME: error handling */
void
camel_imapp_engine_command_queue(CamelIMAPPEngine *imap, CamelIMAPPCommand *ic)
{
	CamelIMAPPCommandPart *cp;

	g_assert(ic->msg.reply_port);

	if (ic->mem)
		imap_engine_command_complete(imap, ic);

	e_msgport_put(imap->port, (EMsg *)ic);
}

CamelIMAPPCommand *
camel_imapp_engine_command_find (CamelIMAPPEngine *imap, const char *name)
{
	CamelIMAPPCommand *ic, *in;

	ic = imap->literal;
	if (ic && strcmp(ic->name, name) == 0)
		return ic;

	/* first, try active */
	ic = (CamelIMAPPCommand *)imap->active.head;
	in = ic->msg.ln.next;
	while (in) {
		if (strcmp(ic->name, name) == 0)
			return ic;
		ic = in;
		in = in->msg.ln.next;
	}

	return NULL;
}

CamelIMAPPCommand *
camel_imapp_engine_command_find_tag(CamelIMAPPEngine *imap, unsigned int tag)
{
	CamelIMAPPCommand *ic, *in;

	ic = imap->literal;
	if (ic && ic->tag == tag)
		return ic;

	ic = (CamelIMAPPCommand *)imap->active.head;
	in = ic->msg.ln.next;
	while (in) {
		if (ic->tag == tag)
			return ic;
		ic = in;
		in = in->msg.ln.next;
	}

	return NULL;
}

/* ********************************************************************** */

CamelIMAPPSelectResponse *
camel_imapp_engine_select(CamelIMAPPEngine *imap, const char *name)
{
	CamelIMAPPSelectResponse * volatile resp;
	CamelIMAPPCommand * volatile ic = NULL;

	resp = g_malloc0(sizeof(*resp));
	imap->select_response = resp;

	CAMEL_TRY {
		ic = camel_imapp_engine_command_new(imap, "SELECT", NULL, "SELECT %s", name);
		camel_imapp_engine_command_queue(imap, ic);
		while (camel_imapp_engine_iterate(imap, ic) > 0)
			;

		if (ic->status->result != IMAP_OK)
			camel_exception_throw(1, "select failed: %s", ic->status->text);
		resp->status = ic->status;
		ic->status = NULL;
	} CAMEL_CATCH (e) {
		camel_imapp_engine_command_free(imap, ic);
		camel_imapp_engine_select_free(imap, resp);
		imap->select_response = NULL;
		camel_exception_throw_ex(e);
	} CAMEL_DONE;

	camel_imapp_engine_command_free(imap, ic);
	imap->select_response = NULL;

	return resp;
}

void
camel_imapp_engine_select_free(CamelIMAPPEngine *imap, CamelIMAPPSelectResponse *select)
{
	if (select) {
		imap_free_status(select->status);
		g_free(select);
	}
}

/* ********************************************************************** */

static void
imap_engine_command_add_part(CamelIMAPPEngine *imap, CamelIMAPPCommand *ic, camel_imapp_command_part_t type, CamelObject *ob)
{
	CamelIMAPPCommandPart *cp;
	CamelStreamNull *null;
	unsigned int ob_size = 0;
	
	switch(type & CAMEL_IMAPP_COMMAND_MASK) {
	case CAMEL_IMAPP_COMMAND_DATAWRAPPER:
	case CAMEL_IMAPP_COMMAND_STREAM:
		null = (CamelStreamNull *)camel_stream_null_new();
		if ( (type & CAMEL_IMAPP_COMMAND_MASK) == CAMEL_IMAPP_COMMAND_DATAWRAPPER) {
			camel_data_wrapper_write_to_stream((CamelDataWrapper *)ob, (CamelStream *)null);
		} else {
			camel_stream_reset((CamelStream *)ob);
			camel_stream_write_to_stream((CamelStream *)ob, (CamelStream *)null);
			camel_stream_reset((CamelStream *)ob);
		}
		type |= CAMEL_IMAPP_COMMAND_CONTINUATION;
		camel_object_ref(ob);
		ob_size = null->written;
		camel_object_unref((CamelObject *)null);
		camel_stream_printf((CamelStream *)ic->mem, "{%u}", ob_size);
		break;
	case CAMEL_IMAPP_COMMAND_AUTH:
		/* we presume we'll need to get additional data only if we're not authenticated yet */
		camel_object_ref(ob);
		camel_stream_printf((CamelStream *)ic->mem, "%s", ((CamelSasl *)ob)->mech);
		if (!camel_sasl_authenticated((CamelSasl *)ob))
			type |= CAMEL_IMAPP_COMMAND_CONTINUATION;
		break;
	default:
		ob_size = 0;
	}

	cp = g_malloc0(sizeof(*cp));
	cp->type = type;
	cp->ob_size = ob_size;
	cp->ob = ob;
	cp->data_size = ic->mem->buffer->len;
	cp->data = g_malloc(cp->data_size+1);
	memcpy(cp->data, ic->mem->buffer->data, cp->data_size);
	cp->data[cp->data_size] = 0;

	camel_stream_reset((CamelStream *)ic->mem);
	/* FIXME: hackish? */
	g_byte_array_set_size(ic->mem->buffer, 0);

	e_dlist_addtail(&ic->parts, (EDListNode *)cp);
}

#if c(!)0
static int len(EDList *list)
{
	int count = 0;
	EDListNode *n = list->head;

	while (n->next) {
		n = n->next;
		count++;
	}
	return count;
}
#endif

static void
imap_engine_command_complete(CamelIMAPPEngine *imap, CamelIMAPPCommand *ic)
{
	c(printf("completing command buffer is [%d] '%.*s'\n", ic->mem->buffer->len, (int)ic->mem->buffer->len, ic->mem->buffer->data));
	c(printf("command has %d parts\n", len(&ic->parts)));
	if (ic->mem->buffer->len > 0)
		imap_engine_command_add_part(imap, ic, CAMEL_IMAPP_COMMAND_SIMPLE, NULL);
	
	c(printf("command has %d parts\n", len(&ic->parts)));

	camel_object_unref((CamelObject *)ic->mem);
	ic->mem = NULL;
}

static void
imap_engine_command_addv(CamelIMAPPEngine *imap, CamelIMAPPCommand *ic, const char *fmt, va_list ap)
{
	const unsigned char *p, *ps, *start;
	unsigned char c;
	unsigned int width;
	char ch;
	int llong;
	int left;
	int fill;
	int zero;
	char *s;
	int d;
	long int l;
	guint32 f;
	CamelStream *S;
	CamelDataWrapper *D;
	CamelSasl *A;
	char buffer[16];

	c(printf("adding command, fmt = '%s'\n", fmt));

	p = fmt;
	ps = fmt;
	while ( ( c = *p++ ) ) {
		switch(c) {
		case '%':
			if (*p == '%') {
				camel_stream_write((CamelStream *)ic->mem, ps, p-ps);
				p++;
				ps = p;
			} else {
				camel_stream_write((CamelStream *)ic->mem, ps, p-ps-1);
				start = p-1;
				width = 0;
				left = FALSE;
				fill = FALSE;
				zero = FALSE;
				llong = FALSE;

				do {
					c = *p++;
					if (c == '0')
						zero = TRUE;
					else if ( c== '-')
						left = TRUE;
					else
						break;
				} while (c);

				do {
					if (isdigit(c))
						width = width * 10 + (c-'0');
					else
						break;
				} while ((c = *p++));

				if (c == 'l') {
					llong = TRUE;
					c = *p++;
				}

				switch(c) {
				case 'A': /* auth object - sasl auth, treat as special kind of continuation */
					A = va_arg(ap, CamelSasl *);
					imap_engine_command_add_part(imap, ic, CAMEL_IMAPP_COMMAND_AUTH, (CamelObject *)A);
					break;
				case 'S': /* stream */
					S = va_arg(ap, CamelStream *);
					c(printf("got stream '%p'\n", S));
					imap_engine_command_add_part(imap, ic, CAMEL_IMAPP_COMMAND_STREAM, (CamelObject *)S);
					break;
				case 'D': /* datawrapper */
					D = va_arg(ap, CamelDataWrapper *);
					c(printf("got data wrapper '%p'\n", D));
					imap_engine_command_add_part(imap, ic, CAMEL_IMAPP_COMMAND_DATAWRAPPER, (CamelObject *)D);
					break;
				case 't': /* token */
					s = va_arg(ap, char *);
					camel_stream_write((CamelStream *)ic->mem, s, strlen(s));
					break;
				case 's': /* simple string */
					s = va_arg(ap, char *);
					c(printf("got string '%s'\n", s));
					/* FIXME: escpae chars, convert to literal or literal+, etc */
					camel_stream_printf((CamelStream *)ic->mem, "\"%s\"", s);
					break;
				case 'f': /* imap folder name */
					s = va_arg(ap, char *);
					c(printf("got folder '%s'\n", s));
					/* FIXME: encode folder name */
					/* FIXME: namespace? */
					camel_stream_printf((CamelStream *)ic->mem, "\"%s\"", s?s:"");
					break;
				case 'F': /* IMAP flags set */
					f = va_arg(ap, guint32);
					imap_write_flags((CamelStream *)ic->mem, f);
					break;
				case 'c':
					d = va_arg(ap, int);
					ch = d;
					camel_stream_write((CamelStream *)ic->mem, &ch, 1);
					break;
				case 'd': /* int/unsigned */
				case 'u':
					if (llong) {
						l = va_arg(ap, long int);
						c(printf("got long int '%d'\n", (int)l));
						memcpy(buffer, start, p-start);
						buffer[p-start] = 0;
						camel_stream_printf((CamelStream *)ic->mem, buffer, l);
					} else {
						d = va_arg(ap, int);
						c(printf("got int '%d'\n", d));
						memcpy(buffer, start, p-start);
						buffer[p-start] = 0;
						camel_stream_printf((CamelStream *)ic->mem, buffer, d);
					}
					break;
				}

				ps = p;
			}
			break;
		case '\\':	/* only for \\ really, we dont support \n\r etc at all */
			c = *p;
			if (c) {
				g_assert(c == '\\');
				camel_stream_write((CamelStream *)ic->mem, ps, p-ps);
				p++;
				ps = p;
			}
		}
	}

	camel_stream_write((CamelStream *)ic->mem, ps, p-ps-1);
}


static void *
cie_worker(void *data)
{
	CamelIMAPPCommand *ic = data;
	CamelIMAPPEngine *imap;
	CamelIMAPPCommandPart *cp;

	/* FIXME: remove select stuff */

	/* see if we need to pre-queue a select command to select the right folder first */
	if (ic->select && (imap->last_select == NULL || strcmp(ic->select, imap->last_select) != 0)) {
		CamelIMAPPCommand *select;
		
		/* of course ... we can't do anything like store/search if we have to select
		   first, because it'll mess up all the sequence numbers ... hrm ... bugger */

		select = camel_imapp_engine_command_new(imap, "SELECT", NULL, "SELECT %s", ic->select);
		g_free(imap->last_select);
		imap->last_select = g_strdup(ic->select);
		camel_imapp_engine_command_queue(imap, select);
		/* how does it get freed? handle inside engine? */
	}
	
	/* first, check if command can be sent yet ... queue if not */
	if (imap->literal != NULL) {
		printf("%p: queueing while literal active\n", ic);
		e_dlist_addtail(&imap->queue, (EDListNode *)ic);
		return;
	}

	cp = (CamelIMAPPCommandPart *)ic->parts.head;
	g_assert(cp);
	ic->current = cp;

	/* how to handle exceptions here? */

	printf("queueing command \"%c%05u %s\"\n", imap->tagprefix, ic->tag, cp->data);
	camel_stream_printf((CamelStream *)imap->stream, "%c%05u %s\r\n", imap->tagprefix, ic->tag, cp->data);

	if (cp->type & CAMEL_IMAPP_COMMAND_CONTINUATION) {
		printf("%p: active literal\n", ic);
		g_assert(cp->next);
		imap->literal = ic;
		e_dlist_addtail(&imap->active, (EDListNode *)ic);
	} else {
		printf("%p: active non-literal\n", ic);
		g_assert(cp->next && cp->next->next == NULL);
		e_dlist_addtail(&imap->active, (EDListNode *)ic);
	}
}


/* here temporarily while its experimental */


#ifdef ENABLE_THREADS
#include <pthread.h>

static pthread_key_t handler_key = 0;

void camel_exception_setup(void)
{
	pthread_key_create(&handler_key, NULL);
}

#else
/* this is per-thread in threaded mode */
static struct _CamelExceptionEnv *handler = NULL;

void camel_exception_setup(void)
{
}
#endif

void
camel_exception_try(struct _CamelExceptionEnv *env)
{
#ifdef ENABLE_THREADS
	struct _CamelExceptionEnv *handler;

	handler = pthread_getspecific(handler_key);
#endif
	env->parent = handler;
	handler = env;
	env->ex = NULL;

#ifdef ENABLE_THREADS
	pthread_setspecific(handler_key, handler);
#endif
}

void
camel_exception_throw_ex(CamelException *ex)
{
	struct _CamelExceptionEnv *env;
#ifdef ENABLE_THREADS
	struct _CamelExceptionEnv *handler;

	handler = pthread_getspecific(handler_key);
#endif
	printf("throwing exception '%s'\n", ex->desc);

	env = handler;
	if (env != NULL) {
		env->ex = ex;
		handler = env->parent;
#ifdef ENABLE_THREADS
		pthread_setspecific(handler_key, handler);
#endif
		longjmp(env->env, ex->id);
	} else {
		g_warning("Uncaught exception: %s\n", ex->desc);
		/* we just crash and burn, this is a code problem */
		/* we dont use g_assert_not_reached() since its not a noreturn function */
		abort();
	}
}

void
camel_exception_throw(int id, char *fmt, ...)
{
	CamelException *ex;
	va_list ap;

	ex = camel_exception_new();
	ex->id = id;
	va_start(ap, fmt);
	ex->desc = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	camel_exception_throw_ex(ex);
}

void
camel_exception_drop(struct _CamelExceptionEnv *env)
{
#ifdef ENABLE_THREADS
	pthread_setspecific(handler_key, env->parent);
#else
	handler = env->parent;
#endif
}

void
camel_exception_done(struct _CamelExceptionEnv *env)
{
#ifdef ENABLE_THREADS
	pthread_setspecific(handler_key, env->parent);
#else
	handler = env->parent;
#endif
	if (env->ex != NULL) {
		camel_exception_free(env->ex);
	}
}
