#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <bonobo/bonobo-control.h>

BonoboControl *addressbook_factory_new_control  (void);
void           addressbook_factory_init         (void);

#endif /* __ADDRESSBOOK_H__ */
