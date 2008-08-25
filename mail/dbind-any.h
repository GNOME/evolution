#ifndef _DBIND_ANY_H_
#define _DBIND_ANY_H_

#include <dbus/dbus.h>

size_t dbind_gather_alloc_info (char            *type);
void   dbind_any_marshal       (DBusMessageIter *iter,
                                char           **type,
                                void           **val);
void   dbind_any_demarshal     (DBusMessageIter *iter,
                                char           **type,
                                void           **val);
void   dbind_any_free          (char            *type,
                                void            *ptr_to_ptr);
void   dbind_any_free_ptr      (char            *type,
                                void            *ptr);

#endif /* _DBIND_ANY_H_ */
