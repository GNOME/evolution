#include <gnome.h>
#include <e-card.h>

#define TEST_VCARD                   \
"BEGIN:VCARD
"                      \
"FN:Nat
"                           \
"N:Friedman;Nat;D;Mr.
"             \
"TITLE:Head Geek
"                  \
"BDAY:1977-08-06
"                  \
"TEL;WORK:617 679 1984
"            \
"TEL;CELL:123 456 7890
"            \
"EMAIL;INTERNET:nat@nat.org
"       \
"EMAIL;INTERNET:nat@helixcode.com
" \
"ADR;WORK;POSTAL:P.O. Box 101;;;Any Town;CA;91921-1234;
" \
"ADR;HOME;POSTAL;INTL:P.O. Box 202;;;Any Town 2;MI;12344-4321;USA
" \
"END:VCARD
"                        \
"
"

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
        char *title;
	ECardName *name;
	ECardList *address;
	ECardList *phone;
	ECardList *email;
	ECardIterator *iterator;
	ECardDate *bday;

	gnome_init ("TestCard", "0.0", argc, argv);

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
	  
	    gtk_object_unref (GTK_OBJECT (card));
	  }
	}
#endif
	card = e_card_new (cardstr);
	gtk_object_get(GTK_OBJECT(card),
		       "full_name",  &fname,
		       "name",       &name,
		       "address",    &address,
		       "phone",      &phone,
		       "email",      &email,
		       "title",      &title,
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
	if ( title ) {
	  printf("Title : %s\n", title);
        }
	if ( bday ) {
	  printf("BDay : %4d-%02d-%02d\n", bday->year, bday->month, bday->day);
	}
	if ( email ) {
	  iterator = e_card_list_get_iterator(address);
	  for (; e_card_iterator_is_valid(iterator); e_card_iterator_next(iterator)) {
	    printf("Email : %s\n", (char *) e_card_iterator_get(iterator));
	  }
	  gtk_object_unref(GTK_OBJECT(iterator));
	}
	if ( phone ) {
	  iterator = e_card_list_get_iterator(address);
	  for (; e_card_iterator_is_valid(iterator); e_card_iterator_next(iterator)) {
	    ECardPhone *e_card_phone = (ECardPhone *) e_card_iterator_get(iterator);
	    printf("Phone ; %d : %s\n", e_card_phone->flags, e_card_phone->number);
	  }
	  gtk_object_unref(GTK_OBJECT(iterator));
	}
	if ( address ) {
	  iterator = e_card_list_get_iterator(address);
	  for (; e_card_iterator_is_valid(iterator); e_card_iterator_next(iterator)) {
	    ECardDeliveryAddress *del_address = (ECardDeliveryAddress *) e_card_iterator_get(iterator);
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
	  gtk_object_unref(GTK_OBJECT(iterator));
	}
	printf("%s", e_card_get_vcard(card));
	gtk_object_unref (GTK_OBJECT (card));

	return 0;
}
