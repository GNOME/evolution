
#include "e-corba-utils.h"

#include "evolution-mail-store.h"

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

