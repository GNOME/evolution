/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>
#include <stdio.h>
#include <string.h>

#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>

#include <e-book.h>
#include <e-book-util.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#include <e-util/e-path.h>

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_Addressbook_VCard_ImporterFactory"
#define COMPONENT_IID "OAFIID:GNOME_Evolution_Addressbook_VCard_Importer"

typedef struct {
	char *filename;
	GList *cardlist;
	GList *iterator;
	EBook *book;
	gboolean ready;
} VCardImporter;

static void
add_card_cb (EBook *book, EBookStatus status, const gchar *id, gpointer closure)
{
	ECard *card = E_CARD(closure);
	g_object_unref(card);
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	VCardImporter *gci = (VCardImporter *) closure;

	gci->cardlist = e_card_load_cards_from_file_with_default_charset(gci->filename, "ISO-8859-1");
	gci->ready = TRUE;
}

static void
ebook_open (VCardImporter *gci, const char *uri)
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
	VCardImporter *gci = (VCardImporter *) closure;
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

static char *supported_extensions[3] = {
	".vcf",
	".gcrd",
	NULL
};

/* Actually check the contents of this file */
static gboolean
check_file_is_vcard (const char *filename)
{
	FILE *handle;
	char line[4096];
	gboolean result;

	handle = fopen (filename, "r");
	if (handle == NULL) {
		g_print ("\n");
		return FALSE;
	}
		
	fgets (line, 4096, handle);
	if (line == NULL) {
		fclose (handle);
		g_print ("\n");
		return FALSE;
	}

	if (g_ascii_strncasecmp (line, "BEGIN:VCARD", 11) == 0) {
		result = TRUE;
	} else {
		result = FALSE;
	}

	fclose (handle);
	return result;
}

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	char *ext;
	int i;

	ext = strrchr (filename, '.');
	if (ext == NULL) {
		return check_file_is_vcard (filename);
	}
	for (i = 0; supported_extensions[i] != NULL; i++) {
		if (g_ascii_strcasecmp (supported_extensions[i], ext) == 0)
			return check_file_is_vcard (filename);
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
	VCardImporter *gci;

	if (check_file_is_vcard (filename) == FALSE) {
		return FALSE;
	}

	gci = (VCardImporter *) closure;
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
	VCardImporter *gci;

	if (!strcmp (component_id, COMPONENT_IID)) {
		gci = g_new (VCardImporter, 1);
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

BONOBO_ACTIVATION_FACTORY (COMPONENT_FACTORY_IID, "Evolution VCard Importer Factory", VERSION, factory_fn, NULL);
