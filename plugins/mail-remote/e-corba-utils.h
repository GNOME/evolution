
#ifndef _E_CORBA_UTILS_H
#define _E_CORBA_UTILS_H

#include "Evolution-DataServer-Mail.h"

struct _EvolutionMailStore;
struct _EvolutionMailFolder;
struct _CamelMessageInfo;
struct _CamelStream;

void e_mail_property_set_string(GNOME_Evolution_Mail_Property *prop, const char *name, const char *val);
void e_mail_property_set_null(GNOME_Evolution_Mail_Property *prop, const char *name);

void e_mail_storeinfo_set_store(GNOME_Evolution_Mail_StoreInfo *si, struct _EvolutionMailStore *store);
void e_mail_folderinfo_set_folder(GNOME_Evolution_Mail_FolderInfo *fi, struct _EvolutionMailFolder *emf);
void e_mail_messageinfo_set_message(GNOME_Evolution_Mail_MessageInfo *mi, struct _CamelMessageInfo *info);

int e_stream_bonobo_to_camel(Bonobo_Stream in, struct _CamelStream *out);

#endif /* !_E_CORBA_UTILS_H */
