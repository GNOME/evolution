/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>
#include <stdio.h>

#include <liboaf/liboaf.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>

#include <e-book.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#include <e-util/e-path.h>

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_Addressbook_VCard_ImporterFactory"

static BonoboGenericFactory *factory = NULL;

typedef struct {
	char *filename;
	char *folderpath;
	GList *cardlist;
	GList *iterator;
	EBook *book;
	gboolean ready;
} VCardImporter;

static void
add_card_cb (EBook *book, EBookStatus status, const gchar *id, gpointer closure)
{
	ECard *card = E_CARD(closure);
	gtk_object_unref(GTK_OBJECT(card));
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	VCardImporter *gci = (VCardImporter *) closure;

	gci->cardlist = e_card_load_cards_from_file_with_default_charset(gci->filename, "ISO-8859-1");
	gci->ready = TRUE;
}

static void
ebook_create (VCardImporter *gci)
{
	gchar *path, *uri;
	gchar *epath;
	
	gci->book = e_book_new ();

	if (!gci->book) {
		printf ("%s: %s(): Couldn't create EBook, bailing.\n",
			__FILE__,
			__FUNCTION__);
		return;
	}

#if 0
	path = g_concat_dir_and_file (g_get_home_dir (), "evolution/local");
	uri = g_strdup_printf ("file://%s", path);
	g_free (path);

	epath = e_path_to_physical (uri, gci->folderpath);
	g_free (uri);
	uri = g_strdup_printf ("%s/addressbook.db", epath);
	g_free (epath);

	if (! e_book_load_uri (gci->book, uri, book_open_cb, gci)) {
		printf ("error calling load_uri!\n");
	}
	g_free(uri);
#endif

	if (! e_book_load_default_book (gci->book, book_open_cb, gci)) {
		g_warning ("Error calling load_default_book");
	}
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

	if (g_strncasecmp (line, "BEGIN:VCARD", 11) == 0) {
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
		if (g_strcasecmp (supported_extensions[i], ext) == 0)
			return check_file_is_vcard (filename);
	}

	return FALSE;
}

static void
importer_destroy_cb (GtkObject *object,
		     VCardImporter *gci)
{
	gtk_main_quit ();
}

static gboolean
load_file_fn (EvolutionImporter *importer,
	      const char *filename,
	      const char *folderpath,
	      void *closure)
{
	VCardImporter *gci;

	if (check_file_is_vcard (filename) == FALSE) {
		return FALSE;
	}

	gci = (VCardImporter *) closure;
	gci->filename = g_strdup (filename);
	gci->folderpath = g_strdup (folderpath);
	gci->cardlist = NULL;
	gci->iterator = NULL;
	gci->ready = FALSE;
	ebook_create (gci);

	return TRUE;
}
					   
static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    void *closure)
{
	EvolutionImporter *importer;
	VCardImporter *gci;

	gci = g_new (VCardImporter, 1);
	importer = evolution_importer_new (support_format_fn, load_file_fn, 
					   process_item_fn, NULL, gci);
	
	gtk_signal_connect (GTK_OBJECT (importer), "destroy",
			    GTK_SIGNAL_FUNC (importer_destroy_cb), gci);
	
	return BONOBO_OBJECT (importer);
}

static void
importer_init (void)
{
	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_IID, 
					      factory_fn, NULL);

	if (factory == NULL) {
		g_error ("Unable to create factory");
	}

	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
}

int
main (int argc,
      char **argv)
{
	CORBA_ORB orb;
	
	gnome_init_with_popt_table ("Evolution-VCard-Importer",
				    PACKAGE, argc, argv, oaf_popt_options, 0,
				    NULL);
	orb = oaf_init (argc, argv);
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error ("Could not initialize Bonobo.");
	}

	importer_init ();
	bonobo_main ();

	return 0;
}


