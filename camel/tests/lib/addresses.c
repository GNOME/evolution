
#include "addresses.h"
#include "camel-test.h"

void
test_address_compare(CamelInternetAddress *addr, CamelInternetAddress *addr2)
{
	const char *r1, *r2, *a1, *a2;
	char *e1, *e2, *f1, *f2;
	int j;

	check(camel_address_length(CAMEL_ADDRESS(addr)) == camel_address_length(CAMEL_ADDRESS(addr2)));
	for (j=0;j<camel_address_length(CAMEL_ADDRESS(addr));j++) {
		
		check(camel_internet_address_get(addr, j, &r1, &a1) == TRUE);
		check(camel_internet_address_get(addr2, j, &r2, &a2) == TRUE);

		check(string_equal(r1, r2));
		check(strcmp(a1, a2) == 0);
	}
	check(camel_internet_address_get(addr, j, &r1, &a1) == FALSE);
	check(camel_internet_address_get(addr2, j, &r2, &a2) == FALSE);

	e1 = camel_address_encode(CAMEL_ADDRESS(addr));
	e2 = camel_address_encode(CAMEL_ADDRESS(addr2));

	if (camel_address_length(CAMEL_ADDRESS(addr)) == 0)
		check(e1 == NULL && e2 == NULL);
	else
		check(e1 != NULL && e2 != NULL);

	if (e1 != NULL) {
		check_msg(string_equal(e1, e2), "e1 = '%s' e2 = '%s'", e1, e2);
		test_free(e1);
		test_free(e2);
	}

	f1 = camel_address_format(CAMEL_ADDRESS(addr));
	f2 = camel_address_format(CAMEL_ADDRESS(addr2));

	if (camel_address_length(CAMEL_ADDRESS(addr)) == 0)
		check(f1 == NULL && f2 == NULL);
	else
		check(f1 != NULL && f2 != NULL);

	if (f1 != NULL) {
		check_msg(string_equal(f1, f2), "f1 = '%s' f2 = '%s'", f1, f2);
		test_free(f1);
		test_free(f2);
	}
}
