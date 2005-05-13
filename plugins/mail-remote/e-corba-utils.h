
#ifndef _E_CORBA_UTILS_H
#define _E_CORBA_UTILS_H

#include "Evolution-DataServer-Mail.h"

void e_mail_property_set_string(GNOME_Evolution_Mail_Property *prop, const char *name, const char *val);
void e_mail_property_set_null(GNOME_Evolution_Mail_Property *prop, const char *name);

#endif /* !_E_CORBA_UTILS_H */
