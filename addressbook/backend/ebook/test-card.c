#include <string.h>
#include "e-card.h"
#include <libgnome/gnome-init.h>

#define TEST_VCARD                     \
"BEGIN:VCARD\r\n"                      \
"FN:Nat\r\n"                           \
"N:Friedman;Nat;D;Mr.\r\n"             \
"ORG:Ximian, Inc.\r\n"                 \
"TITLE:Head Geek\r\n"                  \
"ROLE:Programmer/Executive\r\n"        \
"BDAY:1977-08-06\r\n"                  \
"TEL;WORK:617 679 1984\r\n"            \
"TEL;CELL:123 456 7890\r\n"            \
"EMAIL;INTERNET:nat@nat.org\r\n"       \
"EMAIL;INTERNET:nat@ximian.com\r\n"    \
"ADR;WORK;POSTAL:P.O. Box 101;;;Any Town;CA;91921-1234;\r\n" \
"ADR;HOME;POSTAL;INTL:P.O. Box 202;;;Any Town 2;MI;12344-4321;USA\r\n" \
"END:VCARD\r\n"                        \
"\r\n"

static char *
read_file (char *name)
{
	int  len;
	char buff[65536];
	char line[1024];
	FILE *f;

	f = fopen (name, "r");
	if (f == NULL)
		g_error ("Unable to open %s!\n", name);

	len  = 0;
	while (fgets (line, sizeof (line), f) != NULL) {
		strcpy (buff + len, line);
		len += strlen (line);
	}

	fclose (f);

	return g_strdup (buff);
}



int
main (int argc, char **argv)
{
	char  *cardstr;
	ECard *card;

	/* Fields */
	char *fname;
	char *org;
	char *org_unit;
        char *title;
	char *role;
	char *nickname;
	char *fburl;
	ECardName *name;
	EList *address;
	EList *phone;
	EList *email;
	EList *arbitrary;
	EIterator *iterator;
	ECardDate *bday;

	gnome_program_init("test-card", "0.0", LIBGNOME_MODULE, argc, argv, NULL);

	cardstr = NULL;
	if (argc == 2)
		cardstr = read_file (argv [1]);

	if (cardstr == NULL)
		cardstr = TEST_VCARD;
#if 0
	{
	  int i;
	  for ( i = 0; i < 100000; i++ ) {
	    card = e_card_new (cardstr);
	  
	    g_object_unref (card);
	  }
	}
#endif
	card = e_card_new_with_default_charset (cardstr, "ISO-8859-1");
	g_object_get(card,
		     "full_name",  &fname,
		     "name",       &name,
		     "address",    &address,
		     "phone",      &phone,
		     "email",      &email,
		     "org",        &org,
		     "org_unit",   &org_unit,
		     "title",      &title,
		     "role",       &role,
		     "nickname",   &nickname,
		     "fburl",      &fburl,
		     "arbitrary",  &arbitrary,
		     "birth_date", &bday,
		     NULL);
	if ( fname ) {
	  printf("Name : %s\n", fname);
	  g_free(fname);
	}
	if ( name ) {
	  printf("Full Name:\n");
	  if ( name->prefix )
	    printf("  prefix     : %s\n", name->prefix);
	  if ( name->given )
	    printf("  given      : %s\n", name->given);
	  if ( name->additional )
	    printf("  additional : %s\n", name->additional);
	  if ( name->family )
	    printf("  family     : %s\n", name->family);
	  if ( name->suffix )
	    printf("  suffix     : %s\n", name->suffix);
	}
	if ( org ) {
	  printf("Company : %s\n", org);
        }
	if ( org_unit ) {
	  printf("Department : %s\n", org_unit);
        }
	if ( title ) {
	  printf("Title : %s\n", title);
        }
	if ( role ) {
	  printf("Profession : %s\n", role);
        }
	if ( nickname ) {
	  printf("Nickname : %s\n", nickname);
        }
	if ( fburl ) {
	  printf("Free Busy URL : %s\n", fburl);
	}
	if ( arbitrary ) {
	  iterator = e_list_get_iterator(arbitrary);
	  for (; e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
            ECardArbitrary *arbitrary = (ECardArbitrary *) e_iterator_get(iterator);
	    printf("Arbitrary : %s, %s\n", arbitrary->key, arbitrary->value);
	  }
	  g_object_unref(iterator);
	}
	if ( bday ) {
	  printf("BDay : %4d-%02d-%02d\n", bday->year, bday->month, bday->day);
	}
	if ( email ) {
	  iterator = e_list_get_iterator(address);
	  for (; e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
	    printf("Email : %s\n", (char *) e_iterator_get(iterator));
	  }
	  g_object_unref(iterator);
	  g_object_unref(email);
	}
	if ( phone ) {
	  iterator = e_list_get_iterator(address);
	  for (; e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
	    ECardPhone *e_card_phone = (ECardPhone *) e_iterator_get(iterator);
	    printf("Phone ; %d : %s\n", e_card_phone->flags, e_card_phone->number);
	  }
	  g_object_unref(iterator);
	  g_object_unref(phone);
	}
	if ( address ) {
	  iterator = e_list_get_iterator(address);
	  for (; e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
	    ECardDeliveryAddress *del_address = (ECardDeliveryAddress *) e_iterator_get(iterator);
	    printf("Address ; %d:\n", del_address->flags);
	    if ( del_address->po )
	      printf("  Po      : %s\n", del_address->po);
	    if ( del_address->ext )
	      printf("  Ext     : %s\n", del_address->ext);
	    if ( del_address->street )
	      printf("  Street  : %s\n", del_address->street);
	    if ( del_address->city )
	      printf("  City    : %s\n", del_address->city);
	    if ( del_address->region )
	      printf("  Region  : %s\n", del_address->region);
	    if ( del_address->code )
	      printf("  Code    : %s\n", del_address->code);
	    if ( del_address->country )
	      printf("  Country : %s\n", del_address->country);
	  }
	  g_object_unref(iterator);
	  g_object_unref(address);
	}
	printf("%s", e_card_get_vcard_assume_utf8(card));
	g_object_unref (card);

	return 0;
}
