/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * LDIF importer.  LDIF is the file format of an exported Netscape
 * addressbook.
 *
 * Framework copied from evolution-gnomecard-importer.c
 *
 * Michael M. Morrison (mmorrison@kqcorp.com)
 *
 * Multi-line value support, mailing list support, base64 support, and
 * various fixups: Chris Toshok (toshok@ximian.com)
 */

#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <e-book.h>
#include <e-book-util.h>
#include <e-card-simple.h>
#include <e-destination.h>

#include <libgnome/gnome-init.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_Addressbook_LDIF_ImporterFactory"
#define COMPONENT_IID "OAFIID:GNOME_Evolution_Addressbook_LDIF_Importer"

static GHashTable *dn_card_hash;

typedef struct {
	char *filename;
	GList *cardlist;
	GList *iterator;
	EBook *book;
	gboolean ready;
} LDIFImporter;

static struct {
	char *ldif_attribute;
	ECardSimpleField simple_field;
#define FLAG_ADDRESS 0x01
	int flags;
}
ldif_fields[] = {
	{ "cn", E_CARD_SIMPLE_FIELD_FULL_NAME },
	{ "mail", E_CARD_SIMPLE_FIELD_EMAIL },
#if 0
	{ "givenname", E_CARD_SIMPLE_FIELD_GIVEN_NAME },
#endif
	{ "sn", E_CARD_SIMPLE_FIELD_FAMILY_NAME },
	{ "xmozillanickname", E_CARD_SIMPLE_FIELD_NICKNAME },
	{ "o", E_CARD_SIMPLE_FIELD_ORG },
	{ "locality", 0, FLAG_ADDRESS},
	{ "st", 0, FLAG_ADDRESS },
	{ "streetaddress", 0, FLAG_ADDRESS },
	{ "title", E_CARD_SIMPLE_FIELD_TITLE },
	{ "postalcode", 0, FLAG_ADDRESS },
	{ "countryname", 0, FLAG_ADDRESS },
	{ "telephonenumber", E_CARD_SIMPLE_FIELD_PHONE_BUSINESS},
	{ "homephone", E_CARD_SIMPLE_FIELD_PHONE_HOME },
	{ "facsimiletelephonenumber", E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_FAX },
	{ "ou", E_CARD_SIMPLE_FIELD_ORG_UNIT },
	{ "pagerphone", E_CARD_SIMPLE_FIELD_PHONE_PAGER },
	{ "cellphone", E_CARD_SIMPLE_FIELD_PHONE_MOBILE },
	{ "homeurl", E_CARD_SIMPLE_FIELD_URL },
	{ "description", E_CARD_SIMPLE_FIELD_NOTE },
	{ "xmozillausehtmlmail", E_CARD_SIMPLE_FIELD_WANTS_HTML }
};
static int num_ldif_fields = sizeof(ldif_fields) / sizeof (ldif_fields[0]);

static unsigned char base64_rank[256] = {
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
	 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
	255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
	255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

/**
 * base64_decode_step: decode a chunk of base64 encoded data
 * @in: input stream
 * @len: max length of data to decode
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 *
 * Decodes a chunk of base64 encoded data
 **/
static int
base64_decode_step(unsigned char *in, int len, unsigned char *out, int *state, unsigned int *save)
{
	register unsigned char *inptr, *outptr;
	unsigned char *inend, c;
	register unsigned int v;
	int i;

	inend = in+len;
	outptr = out;

	/* convert 4 base64 bytes to 3 normal bytes */
	v=*save;
	i=*state;
	inptr = in;
	while (inptr<inend) {
		c = base64_rank[*inptr++];
		if (c != 0xff) {
			v = (v<<6) | c;
			i++;
			if (i==4) {
				*outptr++ = v>>16;
				*outptr++ = v>>8;
				*outptr++ = v;
				i=0;
			}
		}
	}

	*save = v;
	*state = i;

	/* quick scan back for '=' on the end somewhere */
	/* fortunately we can drop 1 output char for each trailing = (upto 2) */
	i=2;
	while (inptr>in && i) {
		inptr--;
		if (base64_rank[*inptr] != 0xff) {
			if (*inptr == '=')
				outptr--;
			i--;
		}
	}

	/* if i!= 0 then there is a truncation error! */
	return outptr-out;
}

static int
base64_decode_simple (char *data, int len)
{
	int state = 0;
	unsigned int save = 0;

	return base64_decode_step ((unsigned char *)data, len,
				   (unsigned char *)data, &state, &save);
}

static GString *
getValue( char **src )
{
	GString *dest = g_string_new("");
	char *s = *src;
	gboolean need_base64 = (*s == ':');

 copy_line:
	while( *s != 0 && *s != '\n' && *s != '\r' )
		dest = g_string_append_c (dest, *s++);

	if (*s == '\r') s++;
	if (*s == '\n')	s++;

	/* check for continuation here */
	if (*s == ' ') {
		s++;
		goto copy_line;
	}

	if (need_base64) {
		int new_len;
		/* it's base64 encoded */
		dest = g_string_erase (dest, 0, 2);
		new_len = base64_decode_simple (dest->str, strlen (dest->str));
		dest = g_string_truncate (dest, new_len);
	}

	*src = s;

	return dest;
}

static gboolean
parseLine( ECardSimple *simple, ECardDeliveryAddress *address, char **buf )
{
	char *ptr;
	char *colon, *value;
	gboolean field_handled;
	GString *ldif_value;

	ptr = *buf;

	/* if the string is empty, return */
	if (*ptr == '\0') {
		*buf = NULL;
		return TRUE;
	}

	/* skip comment lines */
	if (*ptr == '#') {
		ptr = strchr (ptr, '\n');
		if (!ptr)
			*buf = NULL;
		else
			*buf = ptr + 1;
		return TRUE;
	}

	/* first, check for a 'continuation' line */
	if( ptr[0] == ' ' && ptr[1] != '\n' ) {
		g_warning ("unexpected continuation line");
		return FALSE;
	}

	colon = (char *)strchr( ptr, ':' );
	if (colon) {
		int i;

		*colon = 0;
		value = colon + 1;
		while ( isspace(*value) )
			value++;

		ldif_value = getValue(&value );

		field_handled = FALSE;
		for (i = 0; i < num_ldif_fields; i ++) {
			if (!g_ascii_strcasecmp (ptr, ldif_fields[i].ldif_attribute)) {
				if (ldif_fields[i].flags & FLAG_ADDRESS) {
					if (!g_ascii_strcasecmp (ptr, "locality"))
						address->city = g_strdup (ldif_value->str);
					else if (!g_ascii_strcasecmp (ptr, "countryname"))
						address->country = g_strdup (ldif_value->str);
					else if (!g_ascii_strcasecmp (ptr, "postalcode"))
						address->code = g_strdup (ldif_value->str);
					else if (!g_ascii_strcasecmp (ptr, "st"))
						address->region = g_strdup (ldif_value->str);
					else if (!g_ascii_strcasecmp (ptr, "streetaddress"))
						address->street = g_strdup (ldif_value->str);
				}
				else {
					e_card_simple_set (simple, ldif_fields[i].simple_field, ldif_value->str);
					printf ("set %s to %s\n", ptr, ldif_value->str);
				}
				field_handled = TRUE;
				break;
			}
		}

		/* handle objectclass/dn/member out here */
		if (!field_handled) {
			if (!g_ascii_strcasecmp (ptr, "dn"))
				g_hash_table_insert (dn_card_hash, g_strdup(ldif_value->str), simple->card);
			else if (!g_ascii_strcasecmp (ptr, "objectclass") && !g_ascii_strcasecmp (ldif_value->str, "groupofnames")) {
				e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_IS_LIST, "true");
			}
			else if (!g_ascii_strcasecmp (ptr, "member")) {
				EList *email;
				g_object_get (simple->card,
					      "email", &email,
					      NULL);
				e_list_append (email, ldif_value->str);
			}
		}

		/* put the colon back the way it was, just for kicks */
		*colon = ':';

		g_string_free (ldif_value, TRUE);
	}
	else {
		g_warning ("unrecognized entry %s", ptr);
		return FALSE;
	}

	*buf = value;

	return TRUE;
}

static ECard *
getNextLDIFEntry( FILE *f )
{
	ECard *card;
	ECardAddrLabel *label;
	ECardSimple *simple;
	ECardDeliveryAddress *address;
	GString *str;
	char line[1024];
	char *buf;

	str = g_string_new ("");
	/* read from the file until we get to a blank line (or eof) */
	while (!feof (f)) {
		if (!fgets (line, sizeof(line), f))
			break;
		if (line[0] == '\n' || (line[0] == '\r' && line[1] == '\n'))
			break;
		str = g_string_append (str, line);
	}

	if (strlen (str->str) == 0) {
		g_string_free (str, TRUE);
		return NULL;
	}

	/* now parse that entry */
	card = e_card_new ("");
	simple = e_card_simple_new (card);
	address = e_card_delivery_address_new ();

	buf = str->str;
	while (buf) {
		if (!parseLine (simple, address, &buf)) {
			/* parsing error */
			g_object_unref (simple);
			e_card_delivery_address_unref (address);
			return NULL;
		}
	}


	/* fill in the address */
	address->flags = E_CARD_ADDR_HOME;

	label = e_card_delivery_address_to_label (address);
	e_card_delivery_address_unref (address);

	e_card_simple_set_address (simple, E_CARD_SIMPLE_ADDRESS_ID_HOME, label);

	e_card_address_label_unref (label);

	e_card_simple_sync_card (simple);

	g_string_free (str, TRUE);

	return card;
}

static void
add_card_cb (EBook *book, EBookStatus status, const gchar *id, gpointer closure)
{
	ECard *card = E_CARD(closure);
	char *vcard;

	e_card_set_id (card, id);

	vcard = e_card_get_vcard(card);	

	g_print ("Saved card: %s\n", vcard);
	g_free(vcard);
}

static void
resolve_list_card (LDIFImporter *gci, ECard *card)
{
	EList *email;
	EIterator *email_iter;
	char *full_name;

	if (!e_card_evolution_list (card))
		return;

	g_object_get (card,
		      "email", &email,
		      "full_name", &full_name,
		      NULL);

	/* set file_as to full_name so we don't later try and figure
           out a first/last name for the list. */
	if (full_name)
		g_object_set (card,
			      "file_as", full_name,
			      NULL);

	email_iter = e_list_get_iterator (email);
	while (e_iterator_is_valid (email_iter)) {
		const char *dn = e_iterator_get (email_iter);
		ECard *dn_card = g_hash_table_lookup (dn_card_hash, dn);

		/* break list chains here, since we don't support them just yet */
		if (dn_card && !e_card_evolution_list (dn_card)) {
			EDestination *dest = e_destination_new ();
			gchar *dest_xml;
			e_destination_set_card (dest, dn_card, 0);  /* Hard-wired for default e-mail, since netscape only exports 1 email address */
			dest_xml = e_destination_export (dest);
			g_object_unref (dest);
			if (dest_xml) {
				e_iterator_set (email_iter, dest_xml);
				g_free (dest_xml);
				e_iterator_next (email_iter);
			}
			else {
				e_iterator_delete (email_iter);
			}
		}
		else {
			e_iterator_delete (email_iter);
		}
	}
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	LDIFImporter *gci = (LDIFImporter *) closure;
	GList * list = NULL;
	GList * list_list = NULL;
	FILE * file;
	ECard *card;

	if(!( file = fopen( gci->filename, "r" ) )) {
		g_warning("!!!Can't open .ldif file");
		return;
	}

	dn_card_hash = g_hash_table_new (g_str_hash, g_str_equal);

	while ((card = getNextLDIFEntry (file))) {

		if (e_card_evolution_list (card))
			list_list = g_list_append (list_list, card);
		else
			list = g_list_append (list, card);
	}

	fclose( file );

	list = g_list_reverse( list );
	list_list = g_list_reverse (list_list);
	list = g_list_concat (list, list_list);

	gci->cardlist = list;
	gci->ready = TRUE;
}

static void
ebook_open (LDIFImporter *gci, const char *uri)
{
	gci->book = e_book_new ();

	if (!gci->book) {
		printf ("%s: %s(): Couldn't create EBook, bailing.\n",
			__FILE__,
			G_GNUC_FUNCTION);
		return;
	}

	e_book_load_address_book_by_uri (gci->book, uri, book_open_cb, gci);
}

/* EvolutionImporter methods */
static void
process_item_fn (EvolutionImporter *importer,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	LDIFImporter *gci = (LDIFImporter *) closure;
	ECard *card;

	if (gci->iterator == NULL)
		gci->iterator = gci->cardlist;

	if (gci->ready == FALSE) {
		GNOME_Evolution_ImporterListener_notifyResult (listener,
							       GNOME_Evolution_ImporterListener_NOT_READY,
							       gci->iterator ? TRUE : FALSE, 
							       ev);
		return;
	}
	
	if (gci->iterator == NULL) {
		GNOME_Evolution_ImporterListener_notifyResult (listener,
							       GNOME_Evolution_ImporterListener_UNSUPPORTED_OPERATION,
							       FALSE, ev);
		return;
	}

	card = gci->iterator->data;
	if (e_card_evolution_list (card))
		resolve_list_card (gci, card);
	e_book_add_card (gci->book, card, add_card_cb, card);

	gci->iterator = gci->iterator->next;

	GNOME_Evolution_ImporterListener_notifyResult (listener,
						       GNOME_Evolution_ImporterListener_OK,
						       gci->iterator ? TRUE : FALSE, 
						       ev);
	if (ev->_major != CORBA_NO_EXCEPTION) {
		g_warning ("Error notifying listeners.");
	}
	
	return;
}

static char *supported_extensions[2] = {
	".ldif", NULL
};

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	char *ext;
	int i;

	ext = strrchr (filename, '.');
	if (ext == NULL) {
		return FALSE;
	}

	for (i = 0; supported_extensions[i] != NULL; i++) {
		if (strcmp (supported_extensions[i], ext) == 0)
			return TRUE;
	}

	return FALSE;
}

static void
importer_destroy_cb (gpointer data,
		     GObject *where_object_was)
{
	bonobo_main_quit ();
}

static gboolean
load_file_fn (EvolutionImporter *importer,
	      const char *filename,
	      const char *uri,
	      const char *folder_type,
	      void *closure)
{
	LDIFImporter *gci;

	gci = (LDIFImporter *) closure;
	gci->filename = g_strdup (filename);
	gci->cardlist = NULL;
	gci->iterator = NULL;
	gci->ready = FALSE;
	ebook_open (gci, uri);

	return TRUE;
}
					   
static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    const char *component_id,
	    void *closure)
{
	EvolutionImporter *importer;
	LDIFImporter *gci;

	if (!strcmp (component_id, COMPONENT_IID)) {
		gci = g_new (LDIFImporter, 1);
		importer = evolution_importer_new (support_format_fn, load_file_fn, 
						   process_item_fn, NULL, gci);
	
		g_object_weak_ref (G_OBJECT (importer),
				   importer_destroy_cb, gci);
	
		return BONOBO_OBJECT (importer);
	}
	else {
		g_warning (COMPONENT_FACTORY_IID ": Don't know what to do with %s", component_id);
		return NULL;
	}
}

BONOBO_ACTIVATION_FACTORY (COMPONENT_FACTORY_IID, "Evolution LDIF Importer Factory", VERSION, factory_fn, NULL);
