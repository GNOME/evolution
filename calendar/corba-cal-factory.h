#ifndef _CORBA_CAL_FACTORY_H_
#define _CORBA_CAL_FACTORY_H_

/* The CORBA globals */
CORBA_ORB          orb;
PortableServer_POA poa;

void init_corba_server (void);
void unregister_calendar_services (void);

#endif /* _CORBA_CAL_FACTORY_H_ */
