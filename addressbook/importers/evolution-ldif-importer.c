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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-init.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-control.h>

#include <libebook/e-book.h>
#include <libedataserverui/e-source-selector.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include <libebook/e-destination.h>

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_Addressbook_LDIF_ImporterFactory:" BASE_VERSION
#define COMPONENT_IID "OAFIID:GNOME_Evolution_Addressbook_LDIF_Importer:" BASE_VERSION

static GHashTable *dn_contact_hash;

typedef struct {
	ESource *primary;
	
	GList *contactlist;
	GList *iterator;
	EBook *book;
	gboolean ready;
} LDIFImporter;

static struct {
	char *ldif_attribute;
	EContactField contact_field;
#define FLAG_ADDRESS 0x01
#define FLAG_LIST 0x02
	int flags;
}
ldif_fields[] = {
	{ "cn", E_CONTACT_FULL_NAME },
	{ "mail", E_CONTACT_EMAIL, FLAG_LIST },
#if 0
	{ "givenname", E_CONTACT_GIVEN_NAME },
#endif
	{ "sn", E_CONTACT_FAMILY_NAME },
	{ "xmozillanickname", E_CONTACT_NICKNAME },
	{ "o", E_CONTACT_ORG },
	{ "locality", 0, FLAG_ADDRESS},
	{ "st", 0, FLAG_ADDRESS },
	{ "streetaddress", 0, FLAG_ADDRESS },
	{ "title", E_CONTACT_TITLE },
	{ "postalcode", 0, FLAG_ADDRESS },
	{ "countryname", 0, FLAG_ADDRESS },
	{ "telephonenumber", E_CONTACT_PHONE_BUSINESS},
	{ "homephone", E_CONTACT_PHONE_HOME },
	{ "facsimiletelephonenumber", E_CONTACT_PHONE_BUSINESS_FAX },
	{ "ou", E_CONTACT_ORG_UNIT },
	{ "pagerphone", E_CONTACT_PHONE_PAGER },
	{ "cellphone", E_CONTACT_PHONE_MOBILE },
	{ "mobile", E_CONTACT_PHONE_MOBILE },
	{ "homeurl", E_CONTACT_HOMEPAGE_URL },
	{ "description", E_CONTACT_NOTE },
	{ "xmozillausehtmlmail", E_CONTACT_WANTS_HTML }
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
parseLine (EContact *contact, EContactAddress *address, char **buf)
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
						address->locality = g_strdup (ldif_value->str);
					else if (!g_ascii_strcasecmp (ptr, "countryname"))
						address->country = g_strdup (ldif_value->str);
					else if (!g_ascii_strcasecmp (ptr, "postalcode"))
						address->code = g_strdup (ldif_value->str);
					else if (!g_ascii_strcasecmp (ptr, "st"))
						address->region = g_strdup (ldif_value->str);
					else if (!g_ascii_strcasecmp (ptr, "streetaddress"))
						address->street = g_strdup (ldif_value->str);
				}
				else if (ldif_fields[i].flags & FLAG_LIST) {
					GList *list;

					list = e_contact_get (contact, ldif_fields[i].contact_field);
					list = g_list_append (list, g_strdup (ldif_value->str));
					e_contact_set (contact, ldif_fields[i].contact_field, list);

					g_list_foreach (list, (GFunc) g_free, NULL);
					g_list_free (list);
				}
				else {
					/* FIXME is everything a string? */
					e_contact_set (contact, ldif_fields[i].contact_field, ldif_value->str);
					g_message ("set %s to %s", ptr, ldif_value->str);
				}
				field_handled = TRUE;
				break;
			}
		}

		/* handle objectclass/dn/member out here */
		if (!field_handled) {
			if (!g_ascii_strcasecmp (ptr, "dn"))
				g_hash_table_insert (dn_contact_hash, g_strdup(ldif_value->str), contact);
			else if (!g_ascii_strcasecmp (ptr, "objectclass") && !g_ascii_strcasecmp (ldif_value->str, "groupofnames")) {
				e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));
			}
			else if (!g_ascii_strcasecmp (ptr, "member")) {
				GList *email;

				email = e_contact_get (contact, E_CONTACT_EMAIL);
				email = g_list_append (email, g_strdup (ldif_value->str));
				e_contact_set (contact, E_CONTACT_EMAIL, email);

				g_list_foreach (email, (GFunc) g_free, NULL);
				g_list_free (email);
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

static EContact *
getNextLDIFEntry( FILE *f )
{
	EContact *contact;
	EContactAddress *address;
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
	contact = e_contact_new ();
	address = g_new0 (EContactAddress, 1);

	buf = str->str;
	while (buf) {
		if (!parseLine (contact, address, &buf)) {
			/* parsing error */
			g_object_unref (contact);
			return NULL;
		}
	}

	/* fill in the address */
	if (address->locality || address->country ||
	    address->code || address->region || address->street)
		e_contact_set (contact, E_CONTACT_ADDRESS_HOME, address);

	g_string_free (str, TRUE);

	return contact;
}

static void
resolve_list_card (LDIFImporter *gci, EContact *contact)
{
	GList *email, *l;
	GList *email_attrs = NULL;
	char *full_name;

	/* set file_as to full_name so we don't later try and figure
           out a first/last name for the list. */
	full_name = e_contact_get (contact, E_CONTACT_FULL_NAME);
	if (full_name)
		e_contact_set (contact, E_CONTACT_FILE_AS, full_name);
	g_free (full_name);

	/* FIMXE getting might not be implemented in ebook */
	email = e_contact_get (contact, E_CONTACT_EMAIL);
	for (l = email; l; l = l->next) {
		/* mozilla stuffs dn's in the EMAIL list for contact lists */
		char *dn = l->data;
		EContact *dn_contact = g_hash_table_lookup (dn_contact_hash, dn);

		/* break list chains here, since we don't support them just yet */
		if (dn_contact && !e_contact_get (dn_contact, E_CONTACT_IS_LIST)) {
			EDestination *dest;
			EVCardAttribute *attr = e_vcard_attribute_new (NULL, EVC_EMAIL);

			/* Hard-wired for default e-mail, since netscape only exports 1 email address */
			dest = e_destination_new ();
			e_destination_set_contact (dest, dn_contact, 0);

			e_destination_export_to_vcard_attribute (dest, attr);

			g_object_unref (dest);

			email_attrs = g_list_append (email_attrs, attr);
		}
	}		
	e_contact_set_attributes (contact, E_CONTACT_EMAIL, email_attrs);

	g_list_foreach (email, (GFunc) g_free, NULL);
	g_list_free (email);
	g_list_foreach (email_attrs, (GFunc) e_vcard_attribute_free, NULL);
	g_list_free (email_attrs);
}

static GList *
create_contacts_from_ldif (const char *filename)
{
	GList * list = NULL;
	GList * list_list = NULL;
	FILE * file;
	EContact *contact;

	if(!( file = fopen( filename, "r" ) )) {
		g_warning("Can't open .ldif file");
		return NULL;
	}

	dn_contact_hash = g_hash_table_new (g_str_hash, g_str_equal);

	while ((contact = getNextLDIFEntry (file))) {

		if (e_contact_get (contact, E_CONTACT_IS_LIST))
			list_list = g_list_append (list_list, contact);
		else
			list = g_list_append (list, contact);
	}

	fclose (file);

	list = g_list_reverse (list);
	list_list = g_list_reverse (list_list);
	list = g_list_concat (list, list_list);

	return list;
}

static void
add_to_notes (EContact *contact, EContactField field)
{
	const gchar *old_text;
	const gchar *field_text;
	gchar       *new_text;

	old_text = e_contact_get_const (contact, E_CONTACT_NOTE);
	if (old_text && strstr (old_text, e_contact_pretty_name (field)))
		return;

	field_text = e_contact_get_const (contact, field);
	if (!field_text || !*field_text)
		return;

	new_text = g_strdup_printf ("%s%s%s: %s",
				    old_text ? old_text : "",
				    old_text && *old_text &&
				    *(old_text + strlen (old_text) - 1) != '\n' ? "\n" : "",
				    e_contact_pretty_name (field), field_text);
	e_contact_set (contact, E_CONTACT_NOTE, new_text);
	g_free (new_text);
}

/* EvolutionImporter methods */
static void
process_item_fn (EvolutionImporter *importer,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	LDIFImporter *gci = (LDIFImporter *) closure;
	EContact *contact;

	if (gci->iterator == NULL)
		gci->iterator = gci->contactlist;

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

	contact = gci->iterator->data;
	if (e_contact_get (contact, E_CONTACT_IS_LIST))
		resolve_list_card (gci, contact);
	else {
		/* Work around the fact that these fields no longer show up in the UI */
		add_to_notes (contact, E_CONTACT_OFFICE);
		add_to_notes (contact, E_CONTACT_SPOUSE);
		add_to_notes (contact, E_CONTACT_BLOG_URL);
	}

	/* FIXME Error checking */
	e_book_add_contact (gci->book, contact, NULL);

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

static void
primary_selection_changed_cb (ESourceSelector *selector, gpointer data)
{
	LDIFImporter *gci = data;

	if (gci->primary)
		g_object_unref (gci->primary);
	gci->primary = g_object_ref (e_source_selector_peek_primary_selection (selector));
}

static void
create_control_fn (EvolutionImporter *importer, Bonobo_Control *control, void *closure)
{
	LDIFImporter *gci = closure;
	GtkWidget *vbox, *selector;
	ESource *primary;
	ESourceList *source_list;	
	
	vbox = gtk_vbox_new (FALSE, FALSE);
	
	/* FIXME Better error handling */
	if (!e_book_get_addressbooks (&source_list, NULL))
		return;		

	selector = e_source_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), selector, FALSE, TRUE, 6);
	
	/* FIXME What if no sources? */
	primary = e_source_list_peek_source_any (source_list);
	e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (selector), primary);
	if (!gci->primary)
		gci->primary = g_object_ref (primary);
	g_object_unref (source_list);
		
	g_signal_connect (G_OBJECT (selector), "primary_selection_changed",
			  G_CALLBACK (primary_selection_changed_cb), gci);

	gtk_widget_show_all (vbox);
	
	*control = BONOBO_OBJREF (bonobo_control_new (vbox));
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
	LDIFImporter *gci = data;

	if (gci->primary)
		g_object_unref (gci->primary);
	
	if (gci->book)
		g_object_unref (gci->book);

	g_list_foreach (gci->contactlist, (GFunc) g_object_unref, NULL);
	g_list_free (gci->contactlist);

	g_free (gci);
}

static gboolean
load_file_fn (EvolutionImporter *importer,
	      const char *filename,
	      void *closure)
{
	LDIFImporter *gci;

	gci = (LDIFImporter *) closure;
	gci->contactlist = NULL;
	gci->iterator = NULL;
	gci->ready = FALSE;

	/* Load the book and the cards */
	gci->book = e_book_new (gci->primary, NULL);
	if (!gci->book) {
		g_message (G_STRLOC ":Couldn't create EBook.");
		return FALSE;
	}
	e_book_open (gci->book, TRUE, NULL);
	gci->contactlist = create_contacts_from_ldif (filename);
	gci->ready = TRUE;

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
		gci = g_new0 (LDIFImporter, 1);
		importer = evolution_importer_new (create_control_fn, support_format_fn, 
						   load_file_fn, process_item_fn, NULL, gci);
	
		g_object_weak_ref (G_OBJECT (importer),
				   importer_destroy_cb, gci);
	
		return BONOBO_OBJECT (importer);
	}
	else {
		g_warning (COMPONENT_FACTORY_IID ": Don't know what to do with %s", component_id);
		return NULL;
	}
}

BONOBO_ACTIVATION_SHLIB_FACTORY (COMPONENT_FACTORY_IID, "Evolution LDIF importer Factory", factory_fn, NULL)
