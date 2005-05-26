
#ifndef _E_CORBA_UTILS_H
#define _E_CORBA_UTILS_H

#include "Evolution-DataServer-Mail.h"

struct _EvolutionMailStore;
struct _EvolutionMailFolder;
struct _CamelMessageInfo;
struct _CamelStream;
struct _CamelMimeMessage;

void e_mail_property_set_string(Evolution_Mail_Property *prop, const char *name, const char *val);
void e_mail_property_set_null(Evolution_Mail_Property *prop, const char *name);

void e_mail_storeinfo_set_store(Evolution_Mail_StoreInfo *si, struct _EvolutionMailStore *store);
void e_mail_folderinfo_set_folder(Evolution_Mail_FolderInfo *fi, struct _EvolutionMailFolder *emf);

void e_mail_messageinfo_set_message(Evolution_Mail_MessageInfo *mi, struct _CamelMessageInfo *info);
struct _CamelMessageInfo *e_mail_messageinfoset_to_info(const Evolution_Mail_MessageInfoSet *mi);

int e_stream_bonobo_to_camel(Bonobo_Stream in, struct _CamelStream *out);
struct _CamelMimeMessage *e_stream_bonobo_to_message(Bonobo_Stream in);
Bonobo_Stream e_stream_message_to_bonobo(struct _CamelMimeMessage *msg);

struct _EDList;

typedef void (*EMailListenerChanged)(CORBA_Object, CORBA_Object, void *changes, CORBA_Environment *);

void e_mail_listener_add(struct _EDList *list, CORBA_Object listener);
gboolean e_mail_listener_remove(struct _EDList *list, CORBA_Object listener);
gboolean e_mail_listener_emit(struct _EDList *list, EMailListenerChanged emit, CORBA_Object source, void *changes);
void e_mail_listener_free(struct _EDList *list);

#endif /* !_E_CORBA_UTILS_H */
