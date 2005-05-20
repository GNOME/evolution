
#ifndef _E_CORBA_UTILS_H
#define _E_CORBA_UTILS_H

#include "Evolution-DataServer-Mail.h"

struct _EvolutionMailStore;

void e_mail_property_set_string(GNOME_Evolution_Mail_Property *prop, const char *name, const char *val);
void e_mail_property_set_null(GNOME_Evolution_Mail_Property *prop, const char *name);

void e_mail_storeinfo_set_store(GNOME_Evolution_Mail_StoreInfo *si, struct _EvolutionMailStore *store);

#endif /* !_E_CORBA_UTILS_H */
