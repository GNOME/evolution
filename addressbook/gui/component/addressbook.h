#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <bonobo/bonobo-control.h>

/* expand file:///foo/foo/ to file:///foo/foo/addressbook.db */
char *         addressbook_expand_uri           (const char *uri);

BonoboControl *addressbook_factory_new_control  (void);
void           addressbook_factory_init         (void);

#endif /* __ADDRESSBOOK_H__ */
