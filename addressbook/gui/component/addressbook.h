#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <bonobo/bonobo-control.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>
#include <ebook/e-book.h>

Bonobo_ConfigDatabase addressbook_config_database (CORBA_Environment *ev);

/* expand file:///foo/foo/ to file:///foo/foo/addressbook.db */
char *         addressbook_expand_uri           (const char *uri);

/* use this instead of e_book_load_uri everywhere where you want the
   authentication to be handled for you. */
gboolean       addressbook_load_uri             (EBook *book, const char *uri, EBookCallback cb, gpointer closure);
gboolean       addressbook_load_default_book    (EBook *book, EBookCallback open_response, gpointer closure);

BonoboControl *addressbook_factory_new_control  (void);
void           addressbook_factory_init         (void);

#endif /* __ADDRESSBOOK_H__ */
