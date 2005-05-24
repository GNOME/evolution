
#include "e-corba-utils.h"

#include "evolution-mail-store.h"
#include "evolution-mail-folder.h"

#include <camel/camel-folder-summary.h>

void
e_mail_property_set_string(GNOME_Evolution_Mail_Property *prop, const char *name, const char *val)
{
	prop->value._release = CORBA_TRUE;
	prop->value._type = TC_CORBA_string;
	prop->value._value = CORBA_sequence_CORBA_string_allocbuf(1);
	((char **)prop->value._value)[0] = CORBA_string_dup(val);
	prop->name = CORBA_string_dup(name);
}

void
e_mail_property_set_null(GNOME_Evolution_Mail_Property *prop, const char *name)
{
	prop->value._release = CORBA_TRUE;
	prop->value._type = TC_null;
	prop->name = CORBA_string_dup(name);
}

void
e_mail_storeinfo_set_store(GNOME_Evolution_Mail_StoreInfo *si, EvolutionMailStore *store)
{
	si->name = CORBA_string_dup(evolution_mail_store_get_name(store));
	si->uid = CORBA_string_dup(evolution_mail_store_get_uid(store));
	si->store = CORBA_Object_duplicate(bonobo_object_corba_objref((BonoboObject *)store), NULL);
}

void
e_mail_messageinfo_set_message(GNOME_Evolution_Mail_MessageInfo *mi, CamelMessageInfo *info)
{
	const CamelTag *tag;
	const CamelFlag *flag;
	int i;

	mi->uid = CORBA_string_dup(camel_message_info_uid(info));
	mi->subject = CORBA_string_dup(camel_message_info_subject(info));
	mi->to = CORBA_string_dup(camel_message_info_to(info));
	mi->from = CORBA_string_dup(camel_message_info_from(info));
	mi->flags = camel_message_info_flags(info);

	flag = camel_message_info_user_flags(info);
	mi->userFlags._maximum = camel_flag_list_size((CamelFlag **)&flag);
	mi->userFlags._length = mi->userFlags._maximum;
	if (mi->userFlags._maximum) {
		mi->userFlags._buffer = GNOME_Evolution_Mail_UserFlags_allocbuf(mi->userFlags._maximum);
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
		mi->userTags._buffer = GNOME_Evolution_Mail_UserTags_allocbuf(mi->userTags._maximum);
		CORBA_sequence_set_release(&mi->userFlags, CORBA_TRUE);

		for (i=0;tag;tag = tag->next,i++) {
			mi->userTags._buffer[i].name = CORBA_string_dup(tag->name);
			mi->userTags._buffer[i].value = CORBA_string_dup(tag->value);
			g_assert(mi->userTags._buffer[i].name);
			g_assert(mi->userTags._buffer[i].value);
		}
	}
}

void
e_mail_folderinfo_set_folder(GNOME_Evolution_Mail_FolderInfo *fi, EvolutionMailFolder *emf)
{
	fi->name = CORBA_string_dup(emf->name);
	fi->full_name = CORBA_string_dup(emf->full_name);
	fi->folder = CORBA_Object_duplicate(bonobo_object_corba_objref((BonoboObject *)emf), NULL);
}

int
e_stream_bonobo_to_camel(Bonobo_Stream in, CamelStream *out)
{
	Bonobo_Stream_iobuf *buf;
	CORBA_Environment ev;
	int go;

	do {
		Bonobo_Stream_read(in, 4096, &buf, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("stream read failed: %s\n", ev._id);
			CORBA_exception_free(&ev);
			return -1;
		}

		go = buf->_length > 0;
		if (go && camel_stream_write(out, buf->_buffer, buf->_length) == -1) {
			CORBA_free(buf);
			return -1;
		}

		CORBA_free(buf);
	} while (go);

	camel_stream_reset(out);

	return 0;
}

