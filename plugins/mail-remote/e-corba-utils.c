
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>

#include "e-corba-utils.h"

#include "evolution-mail-store.h"
#include "evolution-mail-folder.h"
#include "evolution-mail-messagestream.h"

#include "em-message-stream.h"

#include <camel/camel-folder-summary.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-mime-message.h>

#include <bonobo/bonobo-stream-memory.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>

#include <libedataserver/e-msgport.h>

static CORBA_char *
e_corba_strdup(const char *v)
{
	if (v)
		return CORBA_string_dup(v);
	else
		return CORBA_string_dup("");
}

void
e_mail_property_set_string(Evolution_Mail_Property *prop, const char *name, const char *val)
{
	prop->value._release = CORBA_TRUE;
	prop->value._type = TC_CORBA_string;
	prop->value._value = CORBA_sequence_CORBA_string_allocbuf(1);
	((char **)prop->value._value)[0] = CORBA_string_dup(val);
	prop->name = CORBA_string_dup(name);
}

void
e_mail_property_set_null(Evolution_Mail_Property *prop, const char *name)
{
	prop->value._release = CORBA_TRUE;
	prop->value._type = TC_null;
	prop->name = CORBA_string_dup(name);
}

void
e_mail_storeinfo_set_store(Evolution_Mail_StoreInfo *si, EvolutionMailStore *store)
{
	si->name = CORBA_string_dup(evolution_mail_store_get_name(store));
	si->uid = CORBA_string_dup(evolution_mail_store_get_uid(store));
	si->store = CORBA_Object_duplicate(bonobo_object_corba_objref((BonoboObject *)store), NULL);
}

void
e_mail_messageinfo_set_message(Evolution_Mail_MessageInfo *mi, CamelMessageInfo *info)
{
	const CamelTag *tag;
	const CamelFlag *flag;
	int i;

	mi->uid = CORBA_string_dup(camel_message_info_uid(info));
	mi->subject = e_corba_strdup(camel_message_info_subject(info));
	mi->to = e_corba_strdup(camel_message_info_to(info));
	mi->from = e_corba_strdup(camel_message_info_from(info));
	mi->flags = camel_message_info_flags(info);

	flag = camel_message_info_user_flags(info);
	mi->userFlags._maximum = camel_flag_list_size((CamelFlag **)&flag);
	mi->userFlags._length = mi->userFlags._maximum;
	if (mi->userFlags._maximum) {
		mi->userFlags._buffer = Evolution_Mail_UserFlags_allocbuf(mi->userFlags._maximum);
		CORBA_sequence_set_release(&mi->userFlags, CORBA_TRUE);

		for (i=0;flag;flag = flag->next,i++) {
			mi->userFlags._buffer[i] = CORBA_string_dup(flag->name);
			g_assert(mi->userFlags._buffer[i]);
		}
	}

	tag = camel_message_info_user_tags(info);
	mi->userTags._maximum = camel_tag_list_size((CamelTag **)&tag);
	mi->userTags._length = mi->userTags._maximum;
	if (mi->userTags._maximum) {
		mi->userTags._buffer = Evolution_Mail_UserTags_allocbuf(mi->userTags._maximum);
		CORBA_sequence_set_release(&mi->userFlags, CORBA_TRUE);

		for (i=0;tag;tag = tag->next,i++) {
			mi->userTags._buffer[i].name = CORBA_string_dup(tag->name);
			mi->userTags._buffer[i].value = CORBA_string_dup(tag->value);
			g_assert(mi->userTags._buffer[i].name);
			g_assert(mi->userTags._buffer[i].value);
		}
	}
}

CamelMessageInfo *
e_mail_messageinfoset_to_info(const Evolution_Mail_MessageInfoSet *mi)
{
	CamelMessageInfo *info;
	int i;

	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, mi->flagSet, mi->flagMask);
	for (i=0;i<mi->userFlagSet._length;i++)
		camel_message_info_set_user_flag(info, mi->userFlagSet._buffer[i], TRUE);
	for (i=0;i<mi->userTags._length;i++)
		camel_message_info_set_user_tag(info, mi->userTags._buffer[i].name, mi->userTags._buffer[i].value);

	return info;
}

void
e_mail_folderinfo_set_folder(Evolution_Mail_FolderInfo *fi, EvolutionMailFolder *emf)
{
	fi->name = CORBA_string_dup(emf->name);
	fi->full_name = CORBA_string_dup(emf->full_name);
	fi->folder = CORBA_Object_duplicate(bonobo_object_corba_objref((BonoboObject *)emf), NULL);
}

CamelMimeMessage *
e_messagestream_to_message(const Evolution_Mail_MessageStream in, CORBA_Environment *ev)
{
	CamelStream *emms;
	CamelMimeMessage *msg;

	emms = em_message_stream_new(in);
	if (emms == NULL) {
		e_mail_exception_set(ev, Evolution_Mail_FAILED, _("Unknown reason"));
		return NULL;
	}

	msg = camel_mime_message_new();
	if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)msg, emms) == -1) {
		e_mail_exception_set(ev, Evolution_Mail_SYSTEM_ERROR, g_strerror(errno));
		camel_object_unref(msg);
		msg = NULL;
	}
	camel_object_unref(emms);

	return msg;
}

Evolution_Mail_MessageStream
e_messagestream_from_message(CamelMimeMessage *msg, CORBA_Environment *ev)
{
	CamelStreamMem *mem;
	EvolutionMailMessageStream *emms;
	Evolution_Mail_MessageStream out;

	/* didn't say it was going to be efficient ... */

	mem = (CamelStreamMem *)camel_stream_mem_new();
	if (camel_data_wrapper_write_to_stream((CamelDataWrapper *)msg, (CamelStream *)mem) == -1) {
		e_mail_exception_set(ev, Evolution_Mail_SYSTEM_ERROR, g_strerror(errno));
		out = CORBA_OBJECT_NIL;
	} else {
		camel_stream_reset((CamelStream *)mem);
		emms = evolution_mail_messagestream_new((CamelStream *)mem);
		out = CORBA_Object_duplicate(bonobo_object_corba_objref((BonoboObject *)emms), NULL);
	}
	camel_object_unref(mem);

	return out;
}

struct _e_mail_listener {
	struct _e_mail_listener *next;
	struct _e_mail_listener *prev;

	CORBA_Object listener;
};

static struct _e_mail_listener *
eml_find(struct _EDList *list, CORBA_Object listener)
{
	struct _e_mail_listener *l, *n;

	l = (struct _e_mail_listener *)list->head;
	n = l->next;
	while (n) {
		if (l->listener == listener)
			return l;
		l = n;
		n = n->next;
	}

	return NULL;
}

static void
eml_remove(struct _e_mail_listener *l)
{
	CORBA_Environment ev = { 0 };

	e_dlist_remove((EDListNode *)l);
	CORBA_Object_release(l->listener, &ev);
	g_free(l);

	if (ev._major != CORBA_NO_EXCEPTION)
		CORBA_exception_free(&ev);
}

void e_mail_listener_add(struct _EDList *list, CORBA_Object listener)
{
	struct _e_mail_listener *l;
	CORBA_Environment ev = { 0 };

	if (eml_find(list, listener) != NULL)
		return;

	listener = CORBA_Object_duplicate(listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free(&ev);
	} else {
		l = g_malloc(sizeof(*l));
		l->listener = listener;
		e_dlist_addtail(list, (EDListNode *)l);
	}
}

gboolean e_mail_listener_remove(struct _EDList *list, CORBA_Object listener)
{
	struct _e_mail_listener *l;

	l = eml_find(list, listener);
	if (l)
		eml_remove(l);

	return !e_dlist_empty(list);
}

gboolean e_mail_listener_emit(struct _EDList *list, EMailListenerChanged emit, CORBA_Object source, void *changes)
{
	struct _e_mail_listener *l, *n;
	CORBA_Environment ev = { 0 };

	l = (struct _e_mail_listener *)list->head;
	n = l->next;
	while (n) {
		emit(l->listener, source, changes, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("emit changed failed '%s', removing listener\n", ev._id);
			CORBA_exception_free(&ev);
			eml_remove(l);
		}
		l = n;
		n = n->next;
	}

	return !e_dlist_empty(list);
}

void e_mail_listener_free(struct _EDList *list)
{
	struct _e_mail_listener *l, *n;

	l = (struct _e_mail_listener *)list->head;
	n = l->next;
	while (n) {
		eml_remove(l);

		l = n;
		n = n->next;
	}
}

void e_mail_exception_set(CORBA_Environment *ev, Evolution_Mail_ErrorType id, const char *desc)
{
	Evolution_Mail_MailException *x;

	x = Evolution_Mail_MailException__alloc();
	x->id = id;
	x->desc = CORBA_string_dup(desc);
	CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_Evolution_Mail_MailException, x);
}

void e_mail_exception_xfer_camel(CORBA_Environment *ev, CamelException *ex)
{
	e_mail_exception_set(ev, Evolution_Mail_CAMEL_ERROR, ex && ex->desc ? ex->desc:"");
	camel_exception_clear(ex);
}
