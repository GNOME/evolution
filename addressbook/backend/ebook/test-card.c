#include <gnome.h>
#include <e-card.h>

#define TEST_VCARD                   \
"BEGIN:VCARD
"                      \
"FN:Nat
"                           \
"N:Friedman;Nat;D;Mr.
"             \
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
	if ( card->fname )
	  printf("Name : %s\n", card->fname);
	if ( card->name ) {
	  printf("Full Name:\n");
	  if ( card->name->prefix )
	    printf("  prefix     : %s\n", card->name->prefix);
	  if ( card->name->given )
	    printf("  given      : %s\n", card->name->given);
	  if ( card->name->additional )
	    printf("  additional : %s\n", card->name->additional);
	  if ( card->name->family )
	    printf("  family     : %s\n", card->name->family);
	  if ( card->name->suffix )
	    printf("  suffix     : %s\n", card->name->suffix);
	}
	if ( card->bday ) {
	  printf("BDay : %4d-%02d-%02d\n", card->bday->year, card->bday->month, card->bday->day);
	}
	if ( card->email ) {
	  GList *email = card->email;
	  for ( ; email; email = email->next ) {
	    printf("Email : %s\n", (char *) email->data);
	  }
	}
	if ( card->phone ) {
	  GList *phone = card->phone;
	  for ( ; phone; phone = phone->next ) {
	    ECardPhone *e_card_phone = (ECardPhone *) phone->data;
	    printf("Phone ; %d : %s\n", e_card_phone->flags, e_card_phone->number);
	  }
	}
	if ( card->address ) {
	  GList *address = card->address;
	  for ( ; address; address = address->next ) { 
	    ECardDeliveryAddress *del_address = (ECardDeliveryAddress *) address->data;
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
	    if ( del_address->description )
	      printf("  Description : %s\n", del_address->description);
	  }
	}
	gtk_object_unref (GTK_OBJECT (card));

	return 0;
}
