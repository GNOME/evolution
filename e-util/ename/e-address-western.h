#ifndef __E_ADDRESS_WESTERN_H__
#define __E_ADDRESS_WESTERN_H__

typedef struct {
	
	/* Public */
	char *po_box;
	char *extended;  /* I'm not sure what this is. */
	char *street;
	char *locality;  /* For example, the city or town. */
	char *region;	/* The state or province. */
	char *postal_code;
	char *country;
} EAddressWestern;

EAddressWestern *e_address_western_parse (const char *address);
void e_address_western_free (EAddressWestern *eaw);

#endif  /* ! __E_ADDRESS_WESTERN_H__ */


