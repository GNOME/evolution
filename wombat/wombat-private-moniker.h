#ifndef _MONIKER_WOMBAT_PRIVATE_H_
#define _MONIKER_WOMBAT_PRIVATE_H_

#include <bonobo/bonobo-generic-factory.h>

BonoboObject *
wombat_private_moniker_factory (BonoboGenericFactory *this,
				const char           *object_id,
				void                 *data);

#endif
