#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <bonobo/bonobo-control.h>
#include <e-util/e-config-listener.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>
#include <libebook/e-book-async.h>

/* use this instead of e_book_load_uri everywhere where you want the
   authentication to be handled for you. */
#if 0
void       addressbook_load_uri             (EBook *book, const char *uri, EBookCallback cb, gpointer closure);
#endif
void       addressbook_load_source          (EBook *book, ESource *source, EBookCallback cb, gpointer closure);
void       addressbook_load_default_book    (EBookCallback open_response, gpointer closure);
void       addressbook_show_load_error_dialog (GtkWidget *parent, ESource *source, EBookStatus status);

BonoboControl *addressbook_new_control  (void);

#endif /* __ADDRESSBOOK_H__ */
