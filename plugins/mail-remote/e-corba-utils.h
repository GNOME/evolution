
#ifndef _E_CORBA_UTILS_H
#define _E_CORBA_UTILS_H

#include "Evolution-DataServer-Mail.h"

/* Debug, warning debug, error debug, global for whole plugin to make it easier to enable/disable */
#define d(x)
#define w(x)
#define e(x)

struct _EvolutionMailStore;
struct _EvolutionMailFolder;
struct _CamelMessageInfo;
struct _CamelStream;
struct _CamelMimeMessage;
struct _CamelException;

void e_mail_property_set_string(Evolution_Mail_Property *prop, const char *name, const char *val);
void e_mail_property_set_null(Evolution_Mail_Property *prop, const char *name);

void e_mail_storeinfo_set_store(Evolution_Mail_StoreInfo *si, struct _EvolutionMailStore *store);
void e_mail_folderinfo_set_folder(Evolution_Mail_FolderInfo *fi, struct _EvolutionMailFolder *emf);

void e_mail_messageinfo_set_message(Evolution_Mail_MessageInfo *mi, struct _CamelMessageInfo *info);
struct _CamelMessageInfo *e_mail_messageinfoset_to_info(const Evolution_Mail_MessageInfoSet *mi);

struct _CamelMimeMessage *e_messagestream_to_message(const Evolution_Mail_MessageStream in, CORBA_Environment *ev);
Evolution_Mail_MessageStream e_messagestream_from_message(struct _CamelMimeMessage *msg, CORBA_Environment *ev);

struct _EDList;

typedef void (*EMailListenerChanged)(CORBA_Object, CORBA_Object, void *changes, CORBA_Environment *);

void e_mail_listener_add(struct _EDList *list, CORBA_Object listener);
gboolean e_mail_listener_remove(struct _EDList *list, CORBA_Object listener);
gboolean e_mail_listener_emit(struct _EDList *list, EMailListenerChanged emit, CORBA_Object source, void *changes);
void e_mail_listener_free(struct _EDList *list);

/* raise an exception */
void e_mail_exception_set(CORBA_Environment *ev, Evolution_Mail_ErrorType id, const char *desc);
void e_mail_exception_xfer_camel(CORBA_Environment *ev, struct _CamelException *ex);

#endif /* !_E_CORBA_UTILS_H */
