

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "camel-imapp-driver.h"
#include "camel-imapp-utils.h"
#include "camel-imapp-folder.h"
#include "camel-imapp-engine.h"
#include "camel-imapp-summary.h"
#include "camel-imapp-exception.h"

#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-null.h>

#include <camel/camel-folder-summary.h>
#include <camel/camel-store.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-sasl.h>

#define d(x) x

static int driver_resp_fetch(CamelIMAPPEngine *ie, guint32 id, CamelIMAPPDriver *sdata);
static int driver_resp_expunge(CamelIMAPPEngine *ie, guint32 id, CamelIMAPPDriver *sdata);
static int driver_resp_exists(CamelIMAPPEngine *ie, guint32 id, CamelIMAPPDriver *sdata);
static int driver_resp_list(CamelIMAPPEngine *ie, guint32 id, CamelIMAPPDriver *sdata);

static void driver_status(CamelIMAPPEngine *ie, struct _status_info *sinfo, CamelIMAPPDriver *sdata);

static void
class_init(CamelIMAPPDriverClass *ieclass)
{
}

static void
object_init(CamelIMAPPDriver *ie, CamelIMAPPDriverClass *ieclass)
{
	ie->summary = g_ptr_array_new();
	e_dlist_init(&ie->body_fetch);
	e_dlist_init(&ie->body_fetch_done);
}

static void
object_finalise(CamelIMAPPDriver *ie, CamelIMAPPDriverClass *ieclass)
{
	if (ie->folder)
		camel_object_unref((CamelObject *)ie->folder);
	if (ie->engine)
		camel_object_unref((CamelObject *)ie->engine);
	if (ie->summary)
		g_ptr_array_free(ie->summary, TRUE);
}

CamelType
camel_imapp_driver_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_object_get_type (),
			"CamelIMAPPDriver",
			sizeof (CamelIMAPPDriver),
			sizeof (CamelIMAPPDriverClass),
			(CamelObjectClassInitFunc) class_init,
			NULL,
			(CamelObjectInitFunc) object_init,
			(CamelObjectFinalizeFunc) object_finalise);
	}
	
	return type;
}

CamelIMAPPDriver *
camel_imapp_driver_new(CamelIMAPPStream *stream)
{
	CamelIMAPPDriver *driver;
	CamelIMAPPEngine *ie;

	driver = CAMEL_IMAPP_DRIVER (camel_object_new (CAMEL_IMAPP_DRIVER_TYPE));
	ie = driver->engine = camel_imapp_engine_new(stream);
	
	camel_imapp_engine_add_handler(ie, "FETCH", (CamelIMAPPEngineFunc)driver_resp_fetch, driver);
	camel_imapp_engine_add_handler(ie, "EXPUNGE", (CamelIMAPPEngineFunc)driver_resp_expunge, driver);
	camel_imapp_engine_add_handler(ie, "EXISTS", (CamelIMAPPEngineFunc)driver_resp_exists, driver);
	camel_imapp_engine_add_handler(ie, "LIST", (CamelIMAPPEngineFunc)driver_resp_list, driver);
	camel_object_hook_event(ie, "status", (CamelObjectEventHookFunc)driver_status, driver);

        return driver;
}

void
camel_imapp_driver_set_sasl_factory(CamelIMAPPDriver *id, CamelIMAPPSASLFunc get_sasl, void *sasl_data)
{
	id->get_sasl = get_sasl;
	id->get_sasl_data = sasl_data;
}

void
camel_imapp_driver_set_login_query(CamelIMAPPDriver *id, CamelIMAPPLoginFunc get_login, void *login_data)
{
	id->get_login = get_login;
	id->get_login_data = login_data;
}

void
camel_imapp_driver_login(CamelIMAPPDriver *id)
/* throws SERVICE_CANT_AUTHENTICATE, SYSTEM_IO */
{
	CamelIMAPPCommand * volatile ic = NULL;

	/* connect? */
	/* camel_imapp_engine_connect() */
	/* or above? */

	CAMEL_TRY {
		CamelSasl *sasl;

		if (id->get_sasl
		    && (sasl = id->get_sasl(id, id->get_sasl_data))) {
			ic = camel_imapp_engine_command_new(id->engine, "AUTHENTICATE", NULL, "AUTHENTICATE %A", sasl);
			camel_object_unref(sasl);
		} else {
			char *user, *pass;

			g_assert(id->get_login);
			id->get_login(id, &user, &pass, id->get_login_data);
			ic = camel_imapp_engine_command_new(id->engine, "LOGIN", NULL, "LOGIN %s %s", user, pass);
			g_free(user);
			g_free(pass);
		}

		camel_imapp_engine_command_queue(id->engine, ic);
		while (camel_imapp_engine_iterate(id->engine, ic) > 0)
			;

		if (ic->status->result != IMAP_OK)
			camel_exception_throw(CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE, "Login failed: %s", ic->status->text);
		camel_imapp_engine_command_free(id->engine, ic);
	} CAMEL_CATCH(ex) {
		if (ic)
			camel_imapp_engine_command_free(id->engine, ic);
		camel_exception_throw_ex(ex);
	} CAMEL_DONE;
}

void
camel_imapp_driver_select(CamelIMAPPDriver *id, struct _CamelIMAPPFolder *folder)
{
	CamelIMAPPCommand * volatile ic = NULL;
	CamelIMAPPCommand * volatile ic2 = NULL;
	guint32 count;
	CamelFolderSummary *summary;

	if (id->folder) {
		if (folder == id->folder)
			return;
		camel_imapp_driver_sync(id, FALSE, id->folder);
		if (camel_folder_change_info_changed(id->folder->changes)) {
			camel_object_trigger_event(id->folder, "folder_changed", id->folder->changes);
			camel_folder_change_info_clear(id->folder->changes);
		}
		camel_object_unref(id->folder);
		id->folder = NULL;
	}

	summary = ((CamelFolder *)folder)->summary;
	
	ic = camel_imapp_engine_command_new(id->engine, "SELECT", NULL, "SELECT %t", folder->raw_name);
	camel_imapp_engine_command_queue(id->engine, ic);
	while (camel_imapp_engine_iterate(id->engine, ic)>0)
		;
	camel_imapp_engine_command_free(id->engine, ic);

	id->folder = folder;
	camel_object_ref(folder);

	count = camel_folder_summary_count(summary);
	if (count > 0 && count <= id->exists) {
		ic = camel_imapp_engine_command_new(id->engine, "FETCH", NULL,
						   "FETCH 1:%u (UID FLAGS)", count);
		camel_imapp_engine_command_queue(id->engine, ic);
		if (count < id->exists) {
			ic2 = camel_imapp_engine_command_new(id->engine, "FETCH", NULL,
							    "FETCH %u:* (UID FLAGS ENVELOPE)", count+1);
			camel_imapp_engine_command_queue(id->engine, ic2);
		} else {
			ic2 = NULL;
		}

		while (camel_imapp_engine_iterate(id->engine, ic2?ic2:ic)>0)
			;

		camel_imapp_engine_command_free(id->engine, ic);
		if (ic2)
			camel_imapp_engine_command_free(id->engine, ic2);
	} else {
		ic = camel_imapp_engine_command_new(id->engine, "FETCH", NULL,
						   "FETCH 1:* (UID FLAGS ENVELOPE)");
		camel_imapp_engine_command_queue(id->engine, ic);
		while (camel_imapp_engine_iterate(id->engine, ic)>0)
			;
		camel_imapp_engine_command_free(id->engine, ic);
	}

	/* TODO: need to set exists/etc in summary */
	folder->exists = id->exists;
	folder->uidvalidity = id->uidvalidity;

	printf("saving summary '%s'\n", summary->summary_path);
	camel_folder_summary_save(summary);

	if (camel_folder_change_info_changed(id->folder->changes)) {
		camel_object_trigger_event(id->folder, "folder_changed", id->folder->changes);
		camel_folder_change_info_clear(id->folder->changes);
	}
}

static void
imapp_driver_check(CamelIMAPPDriver *id)
{
	guint32 count;
	CamelIMAPPCommand *ic;

	/* FIXME: exception handling */

	if (id->folder->exists != id->exists) {
		count = camel_folder_summary_count(((CamelFolder *)id->folder)->summary);
		if (count < id->exists) {
			printf("fetching new messages\n");
			ic = camel_imapp_engine_command_new(id->engine, "FETCH", NULL,
							    "FETCH %u:* (UID FLAGS ENVELOPE)", count+1);
			camel_imapp_engine_command_queue(id->engine, ic);
			while (camel_imapp_engine_iterate(id->engine, ic)>0)
				;
			camel_imapp_engine_command_free(id->engine, ic);
		} else if (count > id->exists) {
			printf("folder shrank with no expunge notificaitons!?  uh, dunno what to do\n");
		}
	}

	printf("checking for change info changes\n");
	if (camel_folder_change_info_changed(id->folder->changes)) {
		printf("got somechanges!  added=%d changed=%d removed=%d\n",
		       id->folder->changes->uid_added->len,
		       id->folder->changes->uid_changed->len,
		       id->folder->changes->uid_removed->len);
		camel_object_trigger_event(id->folder, "folder_changed", id->folder->changes);
		camel_folder_change_info_clear(id->folder->changes);
	}
}

void
camel_imapp_driver_update(CamelIMAPPDriver *id, CamelIMAPPFolder *folder)
{
	if (id->folder == folder) {
		CamelIMAPPCommand *ic;

		/* this will automagically update flags & expunge items */
		ic = camel_imapp_engine_command_new(id->engine, "NOOP", NULL, "NOOP");
		camel_imapp_engine_command_queue(id->engine, ic);
		while (camel_imapp_engine_iterate(id->engine, ic)>0)
			;
		camel_imapp_engine_command_free(id->engine, ic);

		imapp_driver_check(id);
	} else {
		camel_imapp_driver_select(id, folder);
	}
}

/* FIXME: this is basically a copy of the same in camel-imapp-utils.c */
static struct {
	char *name;
	guint32 flag;
} flag_table[] = {
	{ "\\ANSWERED", CAMEL_MESSAGE_ANSWERED },
	{ "\\DELETED", CAMEL_MESSAGE_DELETED },
	{ "\\DRAFT", CAMEL_MESSAGE_DRAFT },
	{ "\\FLAGGED", CAMEL_MESSAGE_FLAGGED },
	{ "\\SEEN", CAMEL_MESSAGE_SEEN },
	/* { "\\RECENT", CAMEL_IMAPP_MESSAGE_RECENT }, */
};

/*
  flags 00101000
 sflags 01001000
 ^      01100000
~flags  11010111
&       01000000

&flags  00100000
*/

static void
imapp_write_flags(CamelIMAPPDriver *id, guint32 orset, gboolean on, CamelFolderSummary *summary)
{
	guint32 i, j, count;
	CamelIMAPPMessageInfo *info;
	CamelIMAPPCommand *ic = NULL;
	struct _uidset_state ss;
	GSList *commands = NULL;

	/* FIXME: exception handling */

	count = camel_folder_summary_count(summary);
	for (j=0;j<sizeof(flag_table)/sizeof(flag_table[0]);j++) {
		int flush;
		
		if ((orset & flag_table[j].flag) == 0)
			continue;

		printf("checking/storing %s flags '%s'\n", on?"on":"off", flag_table[j].name);

		flush = 0;
		imapp_uidset_init(&ss, id->engine);
		for (i=0;i<count;i++) {
			info = (CamelIMAPPMessageInfo *)camel_folder_summary_index(summary, i);
			if (info) {
				guint32 flags = info->info.flags & CAMEL_IMAPP_SERVER_FLAGS;
				guint32 sflags = info->server_flags & CAMEL_IMAPP_SERVER_FLAGS;

				if ( (on && (((flags ^ sflags) & flags) & flag_table[j].flag))
				     || (!on && (((flags ^ sflags) & ~flags) & flag_table[j].flag))) {
					if (ic == NULL)
						ic = camel_imapp_engine_command_new(id->engine, "STORE", NULL, "UID STORE ");
					flush = imapp_uidset_add(&ss, ic, camel_message_info_uid(info));				
				}
				camel_message_info_free((CamelMessageInfo *)info);
			}

			if (i == count-1 && ic != NULL)
				flush |= imapp_uidset_done(&ss, ic);

			if (flush) {
				flush = 0;
				camel_imapp_engine_command_add(id->engine, ic, " %tFLAGS.SILENT (%t)", on?"+":"-", flag_table[j].name);
				camel_imapp_engine_command_queue(id->engine, ic);
				commands = g_slist_prepend(commands, ic);
				ic = NULL;
			}
		}
	}

	/* flush off any requests we may have outstanding */
	/* TODO: for max benefit, should have this routine do both on and off flags in one go */
	while (commands) {
		GSList *next = commands->next;

		ic = commands->data;
		g_slist_free_1(commands);
		commands = next;

		while (camel_imapp_engine_iterate(id->engine, ic)>0)
			;
		camel_imapp_engine_command_free(id->engine, ic);
	}
}

void
camel_imapp_driver_sync(CamelIMAPPDriver *id, gboolean expunge, CamelIMAPPFolder *folder)
{
	CamelFolderSummary *summary;
	guint i, count, on_orset, off_orset;
	CamelIMAPPMessageInfo *info;
	CamelIMAPPCommand *ic;

	/* FIXME: exception handling */

	camel_imapp_driver_update(id, folder);

	summary = ((CamelFolder *)folder)->summary;
	count = camel_folder_summary_count(summary);
	/* find out which flags have turned on, which have tunred off */
	off_orset = on_orset = 0;
	for (i=0;i<count;i++) {
		guint32 flags, sflags;

		info = (CamelIMAPPMessageInfo *)camel_folder_summary_index(summary, i);
		if (info == NULL)
			continue;
		flags = info->info.flags & CAMEL_IMAPP_SERVER_FLAGS;
		sflags = info->server_flags & CAMEL_IMAPP_SERVER_FLAGS;
		if (flags != sflags) {
			off_orset |= ( flags ^ sflags ) & ~flags;
			on_orset |= (flags ^ sflags) & flags;
		}
		camel_message_info_free((CamelMessageInfo *)info);
	}

	if (on_orset || off_orset) {
		/* turn on or off all messages matching */
		if (on_orset)
			imapp_write_flags(id, on_orset, TRUE, summary);
		if (off_orset)
			imapp_write_flags(id, off_orset, FALSE, summary);

		/* success (no exception), make sure we match what we're supposed to */
		for (i=0;i<count;i++) {
			info = (CamelIMAPPMessageInfo *)camel_folder_summary_index(summary, i);
			if (info == NULL)
				continue;
			info->server_flags = info->info.flags & CAMEL_IMAPP_SERVER_FLAGS;
			camel_message_info_free((CamelMessageInfo *)info);
		}
		camel_folder_summary_touch(summary);
		/* could save summary here, incase of failure? */
	}

	if (expunge) {
		ic = camel_imapp_engine_command_new(id->engine, "EXPUNGE", NULL, "EXPUNGE");
		camel_imapp_engine_command_queue(id->engine, ic);
		while (camel_imapp_engine_iterate(id->engine, ic)>0)
			;
		camel_imapp_engine_command_free(id->engine, ic);
	}

	printf("saving summary '%s'\n", summary->summary_path);
	camel_folder_summary_save(summary);

	if (camel_folder_change_info_changed(id->folder->changes)) {
		camel_object_trigger_event(id->folder, "folder_changed", id->folder->changes);
		camel_folder_change_info_clear(id->folder->changes);
	}
}

#if 0
static void
fetch_data_free(CamelIMAPPFetch *fd)
{
	if (fd->body)
		camel_object_unref(fd->body);
	camel_object_unref(fd->folder);
	g_free(fd->uid);
	g_free(fd->section);
	g_free(fd);
}
#endif

struct _CamelStream *	camel_imapp_driver_fetch(CamelIMAPPDriver *id, struct _CamelIMAPPFolder *folder, const char *uid, const char *body)
{
	return NULL;
}

#if 0
void
camel_imapp_driver_fetch(CamelIMAPPDriver *id, CamelIMAPPFolder *folder, const char *uid, const char *section, CamelIMAPPFetchFunc done, void *data)
{
	struct _fetch_data *fd;
	CamelIMAPPCommand *ic;

	fd = g_malloc0(sizeof(*fd));
	fd->folder = folder;
	camel_object_ref(folder);
	fd->uid = g_strdup(uid);
	fd->section = g_strdup(fd->section);
	fd->done = done;
	fd->data = data;

	e_dlist_addtail(&id->body_fetch, (EDListNode *)&fd);

	CAMEL_TRY {
		camel_imapp_driver_select(id, folder);

		ic = camel_imapp_engine_command_new(id->engine, "FETCH", NULL, "UID FETCH %t (BODY.PEEK[%t])", uid, section);
		camel_imapp_engine_command_queue(id->engine, ic);
		while (camel_imapp_engine_iterate(id->engine, ic)>0)
			;
		camel_imapp_engine_command_free(id->engine, ic);
		imapp_driver_check(id);
	} CAMEL_CATCH(e) {
		/* FIXME: do exception properly */
	} CAMEL_DONE;

	e_dlist_remove((EDListNode *)&fd);

	return fd.data;
}
#endif

GPtrArray *
camel_imapp_driver_list(CamelIMAPPDriver *id, const char *name, guint32 flags)
{
	CamelIMAPPCommand * volatile ic;
	GPtrArray *res;

	g_assert(id->list_commands == NULL);
	g_assert(id->list_result == NULL);

	/* FIXME: make sure we only have a single list running at a time */
	/* sem_wait(id->list_sem); */

	/* FIXME: namespace stuff (done in store code?) */

	/* FIXME: if name != "", we need to also do list "name.%" (. == sep) */

	id->list_result = g_ptr_array_new();
	id->list_flags = flags;
	CAMEL_TRY {
		ic = camel_imapp_engine_command_new(id->engine, "LIST", NULL, "LIST \"\" %f", name[0]?name:"%");
		camel_imapp_engine_command_queue(id->engine, ic);
		while (ic) {
			while (camel_imapp_engine_iterate(id->engine, ic)>0)
				;
			camel_imapp_engine_command_free(id->engine, ic);

			if (id->list_commands) {
				GSList *top = id->list_commands;
				
				id->list_commands = top->next;
				ic = top->data;
				g_slist_free_1(top);
			} else {
				ic = NULL;
			}
		}
	} CAMEL_CATCH(e) {
		GSList *top = id->list_commands;
		int i;

		camel_imapp_engine_command_free(id->engine, ic);

		while (top) {
			GSList *topn = top->next;

			camel_imapp_engine_command_free(id->engine, ic);
			g_slist_free_1(top);
			top = topn;
		}
		id->list_commands = NULL;

		res = id->list_result;
		for (i=0;i<res->len;i++)
			imap_free_list(res->pdata[i]);
		g_ptr_array_free(res, TRUE);
		id->list_result = NULL;

		camel_exception_throw_ex(e);
	} CAMEL_DONE;

	res = id->list_result;
	id->list_result = NULL;

	/* sem_post(id->list_sem); */

	return res;
}

static int
driver_resp_list(CamelIMAPPEngine *ie, guint32 idx, CamelIMAPPDriver *id)
{
	struct _list_info *linfo;

	/* FIXME: exceptions */

	linfo = imap_parse_list(ie->stream);
	printf("store list:  '%s' ('%c')\n", linfo->name, linfo->separator);
	if (id->list_result) {
		if ((linfo->flags & CAMEL_FOLDER_NOINFERIORS) == 0
		    && (id->list_flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE)
		    && linfo->separator) {
			int depth = 0;
			char *p = linfo->name;
			char c = linfo->separator;

			/* this is expensive ... but if we've listed this deep we're going slow anyway */
			while (*p && depth < 10) {
				if (*p == c)
					depth++;
				p++;
			}

			if (depth < 10
			    && (linfo->name[0] == 0 || linfo->name[strlen(linfo->name)-1] != c)) {
				CamelIMAPPCommand *ic;
				
				ic = camel_imapp_engine_command_new(id->engine, "LIST", NULL, "LIST \"\" %t%c%%", linfo->name, c);
				id->list_commands = g_slist_prepend(id->list_commands, ic);
				camel_imapp_engine_command_queue(id->engine, ic);
			}
		}
		/* FIXME: dont add to list if name ends in separator */
		g_ptr_array_add(id->list_result, linfo);
	} else {
		g_warning("unexpected list response\n");
		imap_free_list(linfo);
	}

	return camel_imapp_engine_skip(ie);
}

/* ********************************************************************** */

static void
driver_status(CamelIMAPPEngine *ie, struct _status_info *sinfo, CamelIMAPPDriver *sdata)
{
	printf("got status response ...\n");
	switch(sinfo->condition) {
	case IMAP_READ_WRITE:
		printf("folder is read-write\n");
		break;
	case IMAP_READ_ONLY:
		printf("folder is read-only\n");
		break;
	case IMAP_UIDVALIDITY:
		sdata->uidvalidity = sinfo->u.uidvalidity;
		break;
#if 0	
			/* not defined yet ... */
	case IMAP_UIDNEXT:
		printf("got uidnext for folder: %d\n", sinfo->u.uidnext);
		break;
#endif	
	case IMAP_UNSEEN:
		sdata->unseen = sinfo->u.unseen;
		break;
	case IMAP_PERMANENTFLAGS:
		sdata->permanentflags = sinfo->u.permanentflags;
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
}

static int
driver_resp_exists(CamelIMAPPEngine *ie, guint32 id, CamelIMAPPDriver *sdata)
{
	/* should this be an event instead? */

	sdata->exists = id;

	return camel_imapp_engine_skip(ie);
}

static int
driver_resp_expunge(CamelIMAPPEngine *ie, guint32 id, CamelIMAPPDriver *sdata)
{
	printf("got expunge response %u\n", id);
	if (sdata->folder != NULL) {
		CamelMessageInfo *info;
		CamelFolderSummary *summary = ((CamelFolder *)sdata->folder)->summary;

		info = camel_folder_summary_index(summary, id-1);
		if (info) {
			printf("expunging msg %d\n", id);
			camel_folder_summary_remove(summary, info);
			camel_message_info_free(info);
			camel_folder_change_info_remove_uid(sdata->folder->changes, camel_message_info_uid(info));
		} else {
			printf("can not find msg %u from expunge\n", id);
		}
	}

	return camel_imapp_engine_skip(ie);
}

static int
driver_resp_fetch(CamelIMAPPEngine *ie, guint32 id, CamelIMAPPDriver *sdata)
{
	struct _fetch_info *finfo = NULL;
	CamelIMAPPMessageInfo *info, *uinfo;
	unsigned int i;
	CamelFolderSummary *summary;

	printf("got fetch response %d\n", id);

	if (sdata->folder == NULL)
		goto done;

	summary = ((CamelFolder *)sdata->folder)->summary;

	finfo = imap_parse_fetch(ie->stream);
	imap_dump_fetch(finfo);

	info = (CamelIMAPPMessageInfo *)camel_folder_summary_index(summary, id-1);
	if (info == NULL) {
		if (finfo->uid == NULL) {
			printf("got fetch response for currently unknown message %u\n", id);
			goto done;
		}
		uinfo = (CamelIMAPPMessageInfo *)camel_folder_summary_uid(summary, finfo->uid);
		if (uinfo) {
			/* we have a problem ... index mismatch */
			printf("index mismatch, uid '%s' not at index '%u'\n",
			       finfo->uid, id);
			camel_message_info_free(uinfo);
		}
		/* pad out the summary till we have enough indexes */
		for (i=camel_folder_summary_count(summary);i<id;i++) {
			info = camel_message_info_new(summary);
			if (i == id-1) {
				printf("inserting new info @ %u\n", i);
				info->info.uid = g_strdup(finfo->uid);
			} else {
				char uidtmp[32];

				sprintf(uidtmp, "blank-%u", i);
				info->info.uid = g_strdup(uidtmp);
				printf("inserting empty uid %s\n", uidtmp);
			}
		
			camel_folder_summary_add(summary, (CamelMessageInfo *)info);
		}
		info = (CamelIMAPPMessageInfo *)camel_folder_summary_index(summary, id-1);
		g_assert(info != NULL);
	} else {
		if (finfo->uid) {
			/* FIXME: need to handle blank-* uids, somehow */
			while (info && strcmp(camel_message_info_uid(info), finfo->uid) != 0) {
				printf("index mismatch, uid '%s' not at index '%u', got '%s' instead (removing)\n",
				       finfo->uid, id, camel_message_info_uid(info));

				camel_folder_change_info_remove_uid(sdata->folder->changes, camel_message_info_uid(info));
				camel_folder_summary_remove(summary, (CamelMessageInfo *)info);
				camel_message_info_free(info);
				info = (CamelIMAPPMessageInfo *)camel_folder_summary_index(summary, id-1);
			}
		} else {
			printf("got info for unknown message %u\n", id);
		}
	}

	if (info) {
		if (finfo->got & FETCH_MINFO) {
			/* if we only use ENVELOPE? */
			info->info.subject = g_strdup(camel_message_info_subject(finfo->minfo));
			info->info.from = g_strdup(camel_message_info_from(finfo->minfo));
			info->info.to = g_strdup(camel_message_info_to(finfo->minfo));
			info->info.cc = g_strdup(camel_message_info_cc(finfo->minfo));
			info->info.date_sent = camel_message_info_date_sent(finfo->minfo);
			camel_folder_change_info_add_uid(sdata->folder->changes, camel_message_info_uid(info));
			printf("adding change info uid '%s'\n", camel_message_info_uid(info));
		}

		if (finfo->got & FETCH_FLAGS) {
			if ((info->info.flags & CAMEL_IMAPP_SERVER_FLAGS) != (camel_message_info_flags(finfo) & CAMEL_IMAPP_SERVER_FLAGS)) {
				camel_folder_change_info_change_uid(sdata->folder->changes, camel_message_info_uid(info));
				info->info.flags = (info->info.flags & ~(CAMEL_IMAPP_SERVER_FLAGS)) | (camel_message_info_flags(finfo) & CAMEL_IMAPP_SERVER_FLAGS);
				camel_folder_summary_touch(summary);
			}
			((CamelIMAPPMessageInfo *)info)->server_flags = finfo->flags & CAMEL_IMAPP_SERVER_FLAGS;
		}

		if ((finfo->got & (FETCH_BODY|FETCH_UID)) == (FETCH_BODY|FETCH_UID)) {
			CamelIMAPPFetch *fd, *fn;

			fd = (CamelIMAPPFetch *)sdata->body_fetch.head;
			fn = fd->next;
			while (fn) {
				if (!strcmp(finfo->uid, fd->uid) && !strcmp(finfo->section, fd->section)) {
					fd->done(sdata, fd);
					e_dlist_remove((EDListNode *)fd);
					e_dlist_addtail(&sdata->body_fetch_done, (EDListNode *)fd);
					break;
				}
				fd = fn;
				fn = fn->next;
			}
		}

		camel_message_info_free(info);
	} else {
		printf("dont know what to do with message\n");
	}
 done:
	imap_free_fetch(finfo);

	return camel_imapp_engine_skip(ie);
}


/* This code is for the separate thread per server idea */

typedef enum {
	CAMEL_IMAPP_MSG_FETCH,
	CAMEL_IMAPP_MSG_LIST,
	CAMEL_IMAPP_MSG_QUIT,
	CAMEL_IMAPP_MSG_SEARCH,
	CAMEL_IMAPP_MSG_SYNC,
	CAMEL_IMAPP_MSG_UPDATE,
} camel_imapp_msg_t;

typedef struct _CamelIMAPPMsg CamelIMAPPMsg;

struct _CamelIMAPPMsg {
	EMsg msg;
	CamelOperation *cancel;
	CamelException *ex;
	camel_imapp_msg_t type;
	union {
		struct {
			struct _CamelIMAPPFolder *folder;
			char *uid;
			char *section;
			struct _CamelStream *body;
			struct _CamelIMAPPCommand *ic;
		} fetch;
		struct {
			char *name;
			guint32 flags;
			GPtrArray *result;
			GSList *ics;
		} list;
		struct {
			guint32 flags;
		} quit;
		struct {
			struct _CamelIMAPPFolder *folder;
			char *search;
			GPtrArray *results;
		} search;
		struct {
			struct _CamelIMAPPFolder *folder;
			guint32 flags;
		} sync;
		struct {
			struct _CamelIMAPPFolder *folder;
		} update;
	} data;
};

CamelIMAPPMsg *camel_imapp_msg_new(camel_imapp_msg_t type, struct _CamelException *ex, struct _CamelOperation *cancel, ...);
void camel_imapp_msg_free(CamelIMAPPMsg *m);
void camel_imapp_driver_worker(CamelIMAPPDriver *id);

CamelIMAPPMsg *
camel_imapp_msg_new(camel_imapp_msg_t type, struct _CamelException *ex, struct _CamelOperation *cancel, ...)
{
	CamelIMAPPMsg *m;
	va_list ap;

	m = g_malloc0(sizeof(*m));
	m->type = type;
	m->cancel = cancel;
	camel_operation_ref(cancel);
	m->ex = ex;

	va_start(ap, cancel);
	switch (type) {
	case CAMEL_IMAPP_MSG_FETCH:
		m->data.fetch.folder = va_arg(ap, struct _CamelIMAPPFolder *);
		camel_object_ref(m->data.fetch.folder);
		m->data.fetch.uid = g_strdup(va_arg(ap, char *));
		m->data.fetch.section = g_strdup(va_arg(ap, char *));
		break;
	case CAMEL_IMAPP_MSG_LIST:
		m->data.list.name = g_strdup(va_arg(ap, char *));
		m->data.list.flags = va_arg(ap, guint32);
		break;
	case CAMEL_IMAPP_MSG_QUIT:
		m->data.quit.flags = va_arg(ap, guint32);
		break;
	case CAMEL_IMAPP_MSG_SEARCH:
		m->data.search.folder = va_arg(ap, struct _CamelIMAPPFolder *);
		camel_object_ref(m->data.search.folder);
		m->data.search.search = g_strdup(va_arg(ap, char *));
		break;
	case CAMEL_IMAPP_MSG_SYNC:
		m->data.sync.folder = va_arg(ap, struct _CamelIMAPPFolder *);
		camel_object_ref(m->data.sync.folder);
		m->data.sync.flags = va_arg(ap, guint32);
		break;
	case CAMEL_IMAPP_MSG_UPDATE:
		m->data.update.folder = va_arg(ap, struct _CamelIMAPPFolder *);
		camel_object_ref(m->data.update.folder);
		break;
	}
	va_end(ap);

	return m;
}

void
camel_imapp_msg_free(CamelIMAPPMsg *m)
{
	switch (m->type) {
	case CAMEL_IMAPP_MSG_FETCH:
		camel_object_unref(m->data.fetch.folder);
		g_free(m->data.fetch.uid);
		g_free(m->data.fetch.section);

		if (m->data.fetch.body)
			camel_object_unref(m->data.fetch.body);
		break;
	case CAMEL_IMAPP_MSG_LIST:
		g_free(m->data.list.name);
		if (m->data.list.result)
			/* FIXME: free list data ... */
			g_ptr_array_free(m->data.list.result, TRUE);
		break;
	case CAMEL_IMAPP_MSG_QUIT:
		break;
	case CAMEL_IMAPP_MSG_SEARCH:
		camel_object_unref(m->data.search.folder);
		g_free(m->data.search.search);
		if (m->data.search.results)
			/* FIXME: free search data */
			g_ptr_array_free(m->data.search.results, TRUE);
		break;
	case CAMEL_IMAPP_MSG_SYNC:
		camel_object_unref(m->data.sync.folder);
		break;
	case CAMEL_IMAPP_MSG_UPDATE:
		camel_object_unref(m->data.update.folder);
		break;
	}

	camel_operation_unref(m->cancel);
	g_free(m);
}

void
camel_imapp_driver_worker(CamelIMAPPDriver *id)
{
	CamelIMAPPMsg *m;
	int go = TRUE;

	do {
		/*m = (CamelIMAPPMsg *)e_msgport_get(id->queue);*/
		switch (m->type) {
		case CAMEL_IMAPP_MSG_FETCH:
			/*e_dlist_addtail(&id->fetch_queue, (EDListNode *)m);*/
			camel_imapp_driver_select(id, m->data.fetch.folder);
			break;
		case CAMEL_IMAPP_MSG_LIST:
			m->data.list.result = camel_imapp_driver_list(id, m->data.list.name, m->data.list.flags);
			break;
		case CAMEL_IMAPP_MSG_QUIT:
			camel_imapp_msg_free(m);
			go = FALSE;
			break;
		case CAMEL_IMAPP_MSG_SEARCH:
			break;
		case CAMEL_IMAPP_MSG_SYNC:
			break;
		case CAMEL_IMAPP_MSG_UPDATE:
			break;
		}
	} while (go);
}

