#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <bonobo/bonobo-control.h>
#include <e-util/e-config-listener.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>
#include <libebook/e-book.h>

guint      addressbook_load                 (EBook *book, EBookCallback cb, gpointer closure);
void       addressbook_load_cancel          (guint id);
void       addressbook_load_default_book    (EBookCallback open_response, gpointer closure);

#endif /* __ADDRESSBOOK_H__ */
