/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution addressbook - Address Conduit
 *
 * Copyright (C) 1998 Free Software Foundation
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Eskil Heyn Olsen <deity@eskil.dk> 
 *          JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#define G_LOG_DOMAIN "eaddrconduit"

#include <bonobo.h>
#include <libxml/parser.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-address.h>
#include <ebook/e-book.h>
#include <ebook/e-book-util.h>
#include <ebook/e-card-types.h>
#include <ebook/e-card-cursor.h>
#include <ebook/e-card.h>
#include <ebook/e-card-simple.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-sync-abs.h>
#include <libgpilotdCM/gnome-pilot-conduit-management.h>
#include <libgpilotdCM/gnome-pilot-conduit-config.h>
#include <e-dialog-widgets.h>
#include <e-pilot-map.h>
#include <e-pilot-settings.h>
#include <e-pilot-util.h>

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);

#define CONDUIT_VERSION "0.1.2"

#define DEBUG_CONDUIT 1
/* #undef DEBUG_CONDUIT */

#ifdef DEBUG_CONDUIT
#define LOG(x) x
#else
#define LOG(x)
#endif 

#define WARN g_warning
#define INFO g_message

typedef struct {
	EBookStatus status;
	char *id;
} CardObjectChangeStatus;

typedef enum {
	CARD_ADDED,
	CARD_MODIFIED,
	CARD_DELETED
} CardObjectChangeType;

typedef struct 
{
	ECard *card;
	CardObjectChangeType type;
} CardObjectChange;

enum {
	LABEL_WORK,
	LABEL_HOME,
	LABEL_FAX,
	LABEL_OTHER,
	LABEL_EMAIL,
	LABEL_MAIN,
	LABEL_PAGER,
	LABEL_MOBILE
};

static ECardSimpleField priority [] = {
	E_CARD_SIMPLE_FIELD_PHONE_BUSINESS,
	E_CARD_SIMPLE_FIELD_PHONE_HOME,
	E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_FAX,
	E_CARD_SIMPLE_FIELD_EMAIL,
	E_CARD_SIMPLE_FIELD_PHONE_PAGER,
	E_CARD_SIMPLE_FIELD_PHONE_MOBILE,
	E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_2,
	E_CARD_SIMPLE_FIELD_PHONE_HOME_2,
	E_CARD_SIMPLE_FIELD_PHONE_HOME_FAX,
	E_CARD_SIMPLE_FIELD_EMAIL_2,
	E_CARD_SIMPLE_FIELD_PHONE_OTHER,
	E_CARD_SIMPLE_FIELD_PHONE_PRIMARY,
	E_CARD_SIMPLE_FIELD_PHONE_OTHER_FAX,
	E_CARD_SIMPLE_FIELD_EMAIL_3,
	E_CARD_SIMPLE_FIELD_LAST
};

static int priority_label [] = {
	LABEL_WORK,
	LABEL_HOME,
	LABEL_FAX,
	LABEL_EMAIL,
	LABEL_PAGER,
	LABEL_MOBILE,
	LABEL_WORK,
	LABEL_HOME,
	LABEL_FAX,
	LABEL_EMAIL,
	LABEL_OTHER,
	LABEL_MAIN,
	LABEL_FAX,
	LABEL_EMAIL,
	-1
};

typedef struct _EAddrLocalRecord EAddrLocalRecord;
typedef struct _EAddrConduitCfg EAddrConduitCfg;
typedef struct _EAddrConduitGui EAddrConduitGui;
typedef struct _EAddrConduitContext EAddrConduitContext;

/* Local Record */
struct _EAddrLocalRecord {
	/* The stuff from gnome-pilot-conduit-standard-abs.h
	   Must be first in the structure, or instances of this
	   structure cannot be used by gnome-pilot-conduit-standard-abs.
	*/
	GnomePilotDesktopRecord local;

	/* The corresponding ECard object */
	ECard *ecard;

        /* pilot-link address structure, used for implementing Transmit. */
	struct Address *addr;
};


static void
addrconduit_destroy_record (EAddrLocalRecord *local) 
{
	g_object_unref (local->ecard);
	free_Address (local->addr);
	g_free (local->addr);
	g_free (local);
}

/* Configuration */
struct _EAddrConduitCfg {
	guint32 pilot_id;
	GnomePilotConduitSyncType  sync_type;

	gboolean secret;
	ECardSimpleAddressId default_address;

	gchar *last_uri;
};

static EAddrConduitCfg *
addrconduit_load_configuration (guint32 pilot_id) 
{
	EAddrConduitCfg *c;
	GnomePilotConduitManagement *management;
	GnomePilotConduitConfig *config;
	gchar *address, prefix[256];
	g_snprintf (prefix, 255, "/gnome-pilot.d/e-address-conduit/Pilot_%u/",
		    pilot_id);
	
	c = g_new0 (EAddrConduitCfg,1);
	g_assert (c != NULL);

	c->pilot_id = pilot_id;
	management = gnome_pilot_conduit_management_new ("e_address_conduit", GNOME_PILOT_CONDUIT_MGMT_ID);
	gtk_object_ref (GTK_OBJECT (management));
	gtk_object_sink (GTK_OBJECT (management));
	config = gnome_pilot_conduit_config_new (management, pilot_id);
	gtk_object_ref (GTK_OBJECT (config));
	gtk_object_sink (GTK_OBJECT (config));
	if (!gnome_pilot_conduit_config_is_enabled (config, &c->sync_type))
		c->sync_type = GnomePilotConduitSyncTypeNotSet;
	gtk_object_unref (GTK_OBJECT (config));
	gtk_object_unref (GTK_OBJECT (management));

	/* Custom settings */
	gnome_config_push_prefix (prefix);

	c->secret = gnome_config_get_bool ("secret=FALSE");
	address = gnome_config_get_string ("default_address=business");
	if (!strcmp (address, "business"))
		c->default_address = E_CARD_SIMPLE_ADDRESS_ID_BUSINESS;
	else if (!strcmp (address, "home"))
		c->default_address = E_CARD_SIMPLE_ADDRESS_ID_HOME;
	else if (!strcmp (address, "other"))
		c->default_address = E_CARD_SIMPLE_ADDRESS_ID_OTHER;
	g_free (address);
	c->last_uri = gnome_config_get_string ("last_uri");

	gnome_config_pop_prefix ();

	return c;
}

static void
addrconduit_save_configuration (EAddrConduitCfg *c) 
{
	gchar prefix[256];

	g_snprintf (prefix, 255, "/gnome-pilot.d/e-address-conduit/Pilot_%u/",
		    c->pilot_id);

	gnome_config_push_prefix (prefix);
	gnome_config_set_bool ("secret", c->secret);
	switch (c->default_address) {
	case E_CARD_SIMPLE_ADDRESS_ID_BUSINESS:
		gnome_config_set_string ("default_address", "business");
		break;
	case E_CARD_SIMPLE_ADDRESS_ID_HOME:
		gnome_config_set_string ("default_address", "home");
		break;
	case E_CARD_SIMPLE_ADDRESS_ID_OTHER:
		gnome_config_set_string ("default_address", "other");
		break;
	default:
		g_warning ("Unknown default_address value");
	}
	gnome_config_set_string ("last_uri", c->last_uri);
	gnome_config_pop_prefix ();

	gnome_config_sync ();
	gnome_config_drop_all ();
}

static EAddrConduitCfg*
addrconduit_dupe_configuration (EAddrConduitCfg *c) 
{
	EAddrConduitCfg *retval;

	g_return_val_if_fail (c != NULL, NULL);

	retval = g_new0 (EAddrConduitCfg, 1);
	retval->sync_type = c->sync_type;
	retval->pilot_id = c->pilot_id;

	retval->secret = c->secret;
	retval->default_address = c->default_address;
	retval->last_uri = g_strdup (c->last_uri);

	return retval;
}

static void 
addrconduit_destroy_configuration (EAddrConduitCfg *c) 
{
	g_return_if_fail (c != NULL);

	g_free (c->last_uri);
	g_free (c);
}

/* Gui */
struct _EAddrConduitGui {
	GtkWidget *default_address;
};

static EAddrConduitGui *
e_addr_gui_new (EPilotSettings *ps) 
{
	EAddrConduitGui *gui;
	GtkWidget *lbl, *menu;
	gint rows, i;
	static const char *items[] = {"Business", "Home", "Other", NULL};
	
	g_return_val_if_fail (ps != NULL, NULL);
	g_return_val_if_fail (E_IS_PILOT_SETTINGS (ps), NULL);

	gtk_table_resize (GTK_TABLE (ps), E_PILOT_SETTINGS_TABLE_ROWS + 1, 
			  E_PILOT_SETTINGS_TABLE_COLS);

	gui = g_new0 (EAddrConduitGui, 1);

	rows = E_PILOT_SETTINGS_TABLE_ROWS;
	lbl = gtk_label_new (_("Default Sync Address:"));
	gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
	gui->default_address = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	for (i = 0; items[i] != NULL; i++) {
		GtkWidget *item;

		item = gtk_menu_item_new_with_label (items[i]);
		gtk_widget_show (item);
		
		gtk_menu_append (GTK_MENU (menu), item);
	}
	gtk_widget_show (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (gui->default_address), menu);
	gtk_table_attach_defaults (GTK_TABLE (ps), lbl, 0, 1, rows, rows + 1);
        gtk_table_attach_defaults (GTK_TABLE (ps), gui->default_address, 1, 2, rows, rows + 1);
	gtk_widget_show (lbl);
	gtk_widget_show (gui->default_address);
	
	return gui;
}

static const int default_address_map[] = {
	E_CARD_SIMPLE_ADDRESS_ID_BUSINESS,
	E_CARD_SIMPLE_ADDRESS_ID_HOME,
	E_CARD_SIMPLE_ADDRESS_ID_OTHER,
	-1
};

static void
e_addr_gui_fill_widgets (EAddrConduitGui *gui, EAddrConduitCfg *cfg) 
{
	g_return_if_fail (gui != NULL);
	g_return_if_fail (cfg != NULL);

	e_dialog_option_menu_set (gui->default_address, 
				  cfg->default_address, 
				  default_address_map);
}

static void
e_addr_gui_fill_config (EAddrConduitGui *gui, EAddrConduitCfg *cfg) 
{
	g_return_if_fail (gui != NULL);
	g_return_if_fail (cfg != NULL);

	cfg->default_address = e_dialog_option_menu_get (gui->default_address, 
							 default_address_map);
}

static void
e_addr_gui_destroy (EAddrConduitGui *gui) 
{
	g_free (gui);
}

/* Context */
struct _EAddrConduitContext {
	GnomePilotDBInfo *dbi;

	EAddrConduitCfg *cfg;
	EAddrConduitCfg *new_cfg;
	EAddrConduitGui *gui;
	GtkWidget *ps;
	
	struct AddressAppInfo ai;

	EBook *ebook;
	GList *cards;
	GList *changed;
	GHashTable *changed_hash;
	GList *locals;
	
	gboolean address_load_tried;
	gboolean address_load_success;

	EPilotMap *map;
};

static EAddrConduitContext *
e_addr_context_new (guint32 pilot_id) 
{
	EAddrConduitContext *ctxt = g_new0 (EAddrConduitContext, 1);

	ctxt->cfg = addrconduit_load_configuration (pilot_id);
	ctxt->new_cfg = addrconduit_dupe_configuration (ctxt->cfg);
	ctxt->gui = NULL;
	ctxt->ps = NULL;
	ctxt->ebook = NULL;
	ctxt->cards = NULL;
	ctxt->changed_hash = NULL;
	ctxt->changed = NULL;
	ctxt->locals = NULL;
	ctxt->map = NULL;

	return ctxt;
}

static void
e_addr_context_destroy (EAddrConduitContext *ctxt)
{
	GList *l;
	
	g_return_if_fail (ctxt != NULL);

	if (ctxt->cfg != NULL)
		addrconduit_destroy_configuration (ctxt->cfg);
	if (ctxt->new_cfg != NULL)
		addrconduit_destroy_configuration (ctxt->new_cfg);
	if (ctxt->gui != NULL)
		e_addr_gui_destroy (ctxt->gui);
	
	if (ctxt->ebook != NULL)
		g_object_unref (ctxt->ebook);

	if (ctxt->cards != NULL) {
		for (l = ctxt->cards; l != NULL; l = l->next)
			g_object_unref (l->data);
		g_list_free (ctxt->cards);
	}
	
	if (ctxt->changed_hash != NULL)
		g_hash_table_destroy (ctxt->changed_hash);
	
	if (ctxt->changed != NULL) {
		CardObjectChange *coc;
		
		for (l = ctxt->changed; l != NULL; l = l->next) {
			coc = l->data;

			g_object_unref (coc->card);
			g_free (coc);
		}
		g_list_free (ctxt->changed);
	}
	
	if (ctxt->locals != NULL) {
		for (l = ctxt->locals; l != NULL; l = l->next)
			addrconduit_destroy_record (l->data);
		g_list_free (ctxt->locals);
	}

	if (ctxt->map != NULL)
		e_pilot_map_destroy (ctxt->map);

	g_free (ctxt);
}

/* Debug routines */
static char *
print_local (EAddrLocalRecord *local)
{
	static char buff[ 4096 ];

	if (local == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	if (local->addr) {
		g_snprintf (buff, 4096, "['%s' '%s' '%s']",
			    local->addr->entry[entryLastname] ?
			    local->addr->entry[entryLastname] : "",
			    local->addr->entry[entryFirstname] ?
			    local->addr->entry[entryFirstname] : "",
			    local->addr->entry[entryCompany] ?
			    local->addr->entry[entryCompany] : "");
		return buff;
	}

	return "";
}

static char *print_remote (GnomePilotRecord *remote)
{
	static char buff[ 4096 ];
	struct Address addr;

	if (remote == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	memset (&addr, 0, sizeof (struct Address));
	unpack_Address (&addr, remote->record, remote->length);

	g_snprintf (buff, 4096, "['%s' '%s' '%s']",
		    addr.entry[entryLastname] ?
		    addr.entry[entryLastname] : "",
		    addr.entry[entryFirstname] ?
		    addr.entry[entryFirstname] : "",
		    addr.entry[entryCompany] ?
		    addr.entry[entryCompany] : "");

	free_Address (&addr);

	return buff;
}

/* Addressbok Server routines */
static void
add_card_cb (EBook *ebook, EBookStatus status, const char *id, gpointer closure)
{
	CardObjectChangeStatus *cons = closure;

	cons->status = status;
	cons->id = g_strdup (id);
	
	gtk_main_quit();
}

static void
status_cb (EBook *ebook, EBookStatus status, gpointer closure)
{
	(*(EBookStatus*)closure) = status;
	gtk_main_quit();
}

static void
cursor_cb (EBook *book, EBookStatus status, ECardCursor *cursor, gpointer closure)
{
	EAddrConduitContext *ctxt = (EAddrConduitContext*)closure;

	if (status == E_BOOK_STATUS_SUCCESS) {
		long length;
		int i;

		ctxt->address_load_success = TRUE;

		length = e_card_cursor_get_length (cursor);
		ctxt->cards = NULL;
		for (i = 0; i < length; i ++) {
			ECard *card = e_card_cursor_get_nth (cursor, i);
			
			if (e_card_evolution_list (card))
				continue;

			ctxt->cards = g_list_append (ctxt->cards, card);
		}

		gtk_main_quit(); /* end the sub event loop */
	}
	else {
		WARN (_("Cursor could not be loaded\n"));
		gtk_main_quit(); /* end the sub event loop */
	}
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	EAddrConduitContext *ctxt = (EAddrConduitContext*)closure;

	if (status == E_BOOK_STATUS_SUCCESS) {
		e_book_get_cursor (book, "(contains \"full_name\" \"\")", cursor_cb, ctxt);
	} else {
		WARN (_("EBook not loaded\n"));
		gtk_main_quit(); /* end the sub event loop */
	}
}

static int
start_addressbook_server (EAddrConduitContext *ctxt)
{
	g_return_val_if_fail(ctxt!=NULL,-2);

	ctxt->ebook = e_book_new ();

	e_book_load_default_book (ctxt->ebook, book_open_cb, ctxt);
	
	/* run a sub event loop to turn ebook's async loading into a
           synchronous call */
	gtk_main ();

	if (ctxt->address_load_success)
		return 0;

	return -1;
}

/* Utility routines */
static char *
map_name (EAddrConduitContext *ctxt) 
{
	char *filename = NULL;
	
	filename = g_strdup_printf ("%s/evolution/local/Contacts/pilot-map-%d.xml", g_get_home_dir (), ctxt->cfg->pilot_id);

	return filename;
}

static GList *
next_changed_item (EAddrConduitContext *ctxt, GList *changes) 
{
	CardObjectChange *coc;
	GList *l;
	
	for (l = changes; l != NULL; l = l->next) {
		coc = l->data;
		
		if (g_hash_table_lookup (ctxt->changed_hash, e_card_get_id (coc->card)))
			return l;
	}
	
	return NULL;
}

static ECardSimpleField
get_next_mail (ECardSimpleField *field)
{
	if (field == NULL)
		return E_CARD_SIMPLE_FIELD_EMAIL;
	
	switch (*field) {
	case E_CARD_SIMPLE_FIELD_EMAIL:
		return E_CARD_SIMPLE_FIELD_EMAIL_2;
	case E_CARD_SIMPLE_FIELD_EMAIL_2:
		return E_CARD_SIMPLE_FIELD_EMAIL_3;
	default:
	}

	return E_CARD_SIMPLE_FIELD_LAST;
}

static ECardSimpleField
get_next_home (ECardSimpleField *field)
{
	if (field == NULL)
		return E_CARD_SIMPLE_FIELD_PHONE_HOME;
	
	switch (*field) {
	case E_CARD_SIMPLE_FIELD_PHONE_HOME:
		return E_CARD_SIMPLE_FIELD_PHONE_HOME_2;
	default:
	}

	return E_CARD_SIMPLE_FIELD_LAST;
}

static ECardSimpleField
get_next_work (ECardSimpleField *field)
{
	if (field == NULL)
		return E_CARD_SIMPLE_FIELD_PHONE_BUSINESS;
	
	switch (*field) {
	case E_CARD_SIMPLE_FIELD_PHONE_BUSINESS:
		return E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_2;
	default:
	}

	return E_CARD_SIMPLE_FIELD_LAST;
}

static ECardSimpleField
get_next_fax (ECardSimpleField *field)
{
	if (field == NULL)
		return E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_FAX;
	
	switch (*field) {
	case E_CARD_SIMPLE_FIELD_PHONE_BUSINESS_FAX:
		return E_CARD_SIMPLE_FIELD_PHONE_HOME_FAX;
	case E_CARD_SIMPLE_FIELD_PHONE_HOME_FAX:
		return E_CARD_SIMPLE_FIELD_PHONE_OTHER_FAX;
	default:
	}

	return E_CARD_SIMPLE_FIELD_LAST;
}

static ECardSimpleField
get_next_other (ECardSimpleField *field)
{
	if (field == NULL)
		return E_CARD_SIMPLE_FIELD_PHONE_OTHER;

	return E_CARD_SIMPLE_FIELD_LAST;
}

static ECardSimpleField
get_next_main (ECardSimpleField *field)
{
	if (field == NULL)
		return E_CARD_SIMPLE_FIELD_PHONE_PRIMARY;

	return E_CARD_SIMPLE_FIELD_LAST;
}

static ECardSimpleField
get_next_pager (ECardSimpleField *field)
{
	if (field == NULL)
		return E_CARD_SIMPLE_FIELD_PHONE_PAGER;

	return E_CARD_SIMPLE_FIELD_LAST;
}

static ECardSimpleField
get_next_mobile (ECardSimpleField *field)
{
	if (field == NULL)
		return E_CARD_SIMPLE_FIELD_PHONE_MOBILE;

	return E_CARD_SIMPLE_FIELD_LAST;
}

static void
get_next_init (ECardSimpleField *next_mail,
	       ECardSimpleField *next_home,
	       ECardSimpleField *next_work,
	       ECardSimpleField *next_fax,
	       ECardSimpleField *next_other,
	       ECardSimpleField *next_main,
	       ECardSimpleField *next_pager,
	       ECardSimpleField *next_mobile)
{	
	*next_mail = get_next_mail (NULL);
	*next_home = get_next_home (NULL);
	*next_work = get_next_work (NULL);
	*next_fax = get_next_fax (NULL);
	*next_other = get_next_other (NULL);
	*next_main = get_next_main (NULL);
	*next_pager = get_next_pager (NULL);
	*next_mobile = get_next_mobile (NULL);
}

static gboolean
is_next_done (ECardSimpleField field)
{
	if (field == E_CARD_SIMPLE_FIELD_LAST)
		return TRUE;
	
	return FALSE;
}

static gboolean
is_syncable (EAddrConduitContext *ctxt, EAddrLocalRecord *local) 
{	
	ECardSimpleField next_mail, next_home, next_work, next_fax;
	ECardSimpleField next_other, next_main, next_pager, next_mobile;
	gboolean syncable = TRUE;
	int i, l = 0;

	/* See if there are fields we can't sync or not in priority order */
	get_next_init (&next_mail, &next_home, &next_work, &next_fax,
		       &next_other, &next_main, &next_pager, &next_mobile);
	
	for (i = entryPhone1; i <= entryPhone5 && syncable; i++) {
		int phonelabel = local->addr->phoneLabel[i - entryPhone1];
		const char *phone_str = local->addr->entry[i];
		gboolean empty = !(phone_str && *phone_str);
		
		if (empty)
			continue;
		
		for ( ; priority_label[l] != -1; l++)
			if (phonelabel == priority_label[l])
				break;

		if (priority_label[l] == -1) {
			syncable = FALSE;
			continue;
		}
		
		if (phonelabel == LABEL_EMAIL) {
			if (is_next_done (next_mail) || next_mail != priority[l]) {
				syncable = FALSE;
				break;
			}
			next_mail = get_next_mail (&next_mail);
		} else if (phonelabel == LABEL_HOME) {
			if (is_next_done (next_home) || next_home != priority[l]) {
				syncable = FALSE;
				break;
			}
			next_home = get_next_home (&next_home);
		} else if (phonelabel == LABEL_WORK) {
			if (is_next_done (next_work) || next_work != priority[l]) {
				syncable = FALSE;
				break;
			}
			next_work = get_next_work (&next_work);
		} else if (phonelabel == LABEL_FAX) {
			if (is_next_done (next_fax) || next_fax != priority[l]) {
				syncable = FALSE;
				break;
			}
			next_fax = get_next_fax (&next_fax);
		} else if (phonelabel == LABEL_OTHER) {
			if (is_next_done (next_other) || next_other != priority[l]) {
				syncable = FALSE;
				break;
			}
			next_other = get_next_other (&next_other);
		} else if (phonelabel == LABEL_MAIN) {
			if (is_next_done (next_main) || next_main != priority[l]) {
				syncable = FALSE;
				break;
			}
			next_main = get_next_main (&next_main);
		} else if (phonelabel == LABEL_PAGER) {
			if (is_next_done (next_pager) || next_pager != priority[l]) {
				syncable = FALSE;
				break;
			}
			next_pager = get_next_pager (&next_pager);
		} else if (phonelabel == LABEL_MOBILE) {
			if (is_next_done (next_mobile) || next_mobile != priority[l]) {
				syncable = FALSE;
				break;
			}
			next_mobile = get_next_mobile (&next_mobile);
		}
	}

	return syncable;
}

static char *
get_entry_text (struct Address address, int field)
{
	if (address.entry[field])
		return e_pilot_utf8_from_pchar (address.entry[field]);
	
	return g_strdup ("");
}

static void
clear_entry_text (struct Address address, int field) 
{
	if (address.entry[field]) {
		free (address.entry[field]);
		address.entry[field] = NULL;
	}
}

static void
compute_status (EAddrConduitContext *ctxt, EAddrLocalRecord *local, const char *uid)
{
	CardObjectChange *coc;

	local->local.archived = FALSE;
	local->local.secret = FALSE;

	coc = g_hash_table_lookup (ctxt->changed_hash, uid);
	
	if (coc == NULL) {
		local->local.attr = GnomePilotRecordNothing;
		return;
	}
	
	switch (coc->type) {
	case CARD_ADDED:
		local->local.attr = GnomePilotRecordNew;
		break;	
	case CARD_MODIFIED:
		local->local.attr = GnomePilotRecordModified;
		break;
	case CARD_DELETED:
		local->local.attr = GnomePilotRecordDeleted;
		break;
	}
}

static GnomePilotRecord
local_record_to_pilot_record (EAddrLocalRecord *local,
			      EAddrConduitContext *ctxt)
{
	GnomePilotRecord p;
	static char record[0xffff];
	
	g_assert (local->addr != NULL );
	
	LOG (g_message ( "local_record_to_pilot_record\n" ));

	p.ID = local->local.ID;
	p.category = local->local.category;
	p.attr = local->local.attr;
	p.archived = local->local.archived;
	p.secret = local->local.secret;

	/* Generate pilot record structure */
	p.record = record;
	p.length = pack_Address (local->addr, p.record, 0xffff);

	return p;	
}

static void
local_record_from_ecard (EAddrLocalRecord *local, ECard *ecard, EAddrConduitContext *ctxt)
{
	ECardSimple *simple;
	const ECardDeliveryAddress *delivery;
	ECardSimpleAddressId mailing_address;
	int phone = entryPhone1;

	gboolean syncable;
	int i;
	
	g_return_if_fail (local != NULL);
	g_return_if_fail (ecard != NULL);

	local->ecard = ecard;
	g_object_ref (ecard);
	simple = e_card_simple_new (ecard);
	
	local->local.ID = e_pilot_map_lookup_pid (ctxt->map, ecard->id, TRUE);

	compute_status (ctxt, local, ecard->id);

	local->addr = g_new0 (struct Address, 1);

	/* Handle the fields and category we don't sync by making sure
         * we don't overwrite them 
	 */
	if (local->local.ID != 0) {
		struct Address addr;
		char record[0xffff];
		int cat = 0;
		
		if (dlp_ReadRecordById (ctxt->dbi->pilot_socket, 
					ctxt->dbi->db_handle,
					local->local.ID, &record, 
					NULL, NULL, NULL, &cat) > 0) {
			local->local.category = cat;
			memset (&addr, 0, sizeof (struct Address));
			unpack_Address (&addr, record, 0xffff);
			for (i = 0; i < 5; i++) {
				if (addr.entry[entryPhone1 + i])
					local->addr->entry[entryPhone1 + i] = 
						strdup (addr.entry[entryPhone1 + i]);
				local->addr->phoneLabel[i] = addr.phoneLabel[i];
			}
			local->addr->showPhone = addr.showPhone;
			for (i = 0; i < 4; i++) {
				if (addr.entry[entryCustom1 + i])
					local->addr->entry[entryCustom1 + i] = 
						strdup (addr.entry[entryCustom1 + i]);
			}
			free_Address (&addr);
		}
	}

	if (ecard->name) {
		local->addr->entry[entryFirstname] = e_pilot_utf8_to_pchar (ecard->name->given);
		local->addr->entry[entryLastname] = e_pilot_utf8_to_pchar (ecard->name->family);
	}

	local->addr->entry[entryCompany] = e_pilot_utf8_to_pchar (ecard->org);
	local->addr->entry[entryTitle] = e_pilot_utf8_to_pchar (ecard->title);

	mailing_address = -1;
	for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i++) {
		const ECardAddrLabel *address;
		
		address = e_card_simple_get_address(simple, i);
		if (address && (address->flags & E_CARD_ADDR_DEFAULT))
			mailing_address = i;
	}
	if (mailing_address == -1)
		mailing_address = ctxt->cfg->default_address;

	delivery = e_card_simple_get_delivery_address (simple, mailing_address);
	if (delivery) {
		char *add;
		
		/* If the address has 2 lines, make sure both get added */
		if (delivery->ext != NULL)
			add = g_strconcat (delivery->street, "\n", delivery->ext, NULL);
		else
			add = g_strdup (delivery->street);
		local->addr->entry[entryAddress] = e_pilot_utf8_to_pchar (add);
		g_free (add);
		
		local->addr->entry[entryCity] = e_pilot_utf8_to_pchar (delivery->city);
		local->addr->entry[entryState] = e_pilot_utf8_to_pchar (delivery->region);
		local->addr->entry[entryZip] = e_pilot_utf8_to_pchar (delivery->code);
		local->addr->entry[entryCountry] = e_pilot_utf8_to_pchar (delivery->country);
	}

	/* Phone numbers */

	/* See if everything is syncable */
	syncable = is_syncable (ctxt, local);
	
	if (syncable) {
		INFO ("Syncable");

		/* Sync by priority */
		for (i = 0, phone = entryPhone1; 
		     priority[i] != E_CARD_SIMPLE_FIELD_LAST && phone <= entryPhone5; i++) {
			const char *phone_str;
			
			phone_str = e_card_simple_get_const (simple, priority[i]);
			if (phone_str && *phone_str) {
				clear_entry_text (*local->addr, phone);
				local->addr->entry[phone] = e_pilot_utf8_to_pchar (phone_str);
				local->addr->phoneLabel[phone - entryPhone1] = priority_label[i];
				phone++;
			}
		}
		for ( ; phone <= entryPhone5; phone++)
			      local->addr->phoneLabel[phone - entryPhone1] = phone - entryPhone1;
		local->addr->showPhone = 0;
	} else {
		ECardSimpleField next_mail, next_home, next_work, next_fax;
		ECardSimpleField next_other, next_main, next_pager, next_mobile;

		INFO ("Not Syncable");
		get_next_init (&next_mail, &next_home, &next_work, &next_fax,
			       &next_other, &next_main, &next_pager, &next_mobile);

		/* Not completely syncable, so do the best we can */
		for (i = entryPhone1; i <= entryPhone5; i++) {
			int phonelabel = local->addr->phoneLabel[i - entryPhone1];
			const char *phone_str = NULL;
			
			if (phonelabel == LABEL_EMAIL && !is_next_done (next_mail)) {
				phone_str = e_card_simple_get_const (simple, next_mail);
				next_mail = get_next_mail (&next_mail);
			} else if (phonelabel == LABEL_HOME && !is_next_done (next_home)) {
				phone_str = e_card_simple_get_const (simple, next_home);
				next_home = get_next_home (&next_home);
			} else if (phonelabel == LABEL_WORK && !is_next_done (next_work)) {
				phone_str = e_card_simple_get_const (simple, next_work);
				next_work = get_next_work (&next_work);
			} else if (phonelabel == LABEL_FAX && !is_next_done (next_fax)) {
				phone_str = e_card_simple_get_const (simple, next_fax);
				next_fax = get_next_fax (&next_fax);
			} else if (phonelabel == LABEL_OTHER && !is_next_done (next_other)) {
				phone_str = e_card_simple_get_const (simple, next_other);
				next_other = get_next_other (&next_other);
			} else if (phonelabel == LABEL_MAIN && !is_next_done (next_main)) {
				phone_str = e_card_simple_get_const (simple, next_main);
				next_main = get_next_main (&next_main);
			} else if (phonelabel == LABEL_PAGER && !is_next_done (next_pager)) {
				phone_str = e_card_simple_get_const (simple, next_pager);
				next_pager = get_next_pager (&next_pager);
			} else if (phonelabel == LABEL_MOBILE && !is_next_done (next_mobile)) {
				phone_str = e_card_simple_get_const (simple, next_mobile);
				next_mobile = get_next_mobile (&next_mobile);
			}
			
			if (phone_str && *phone_str) {
				clear_entry_text (*local->addr, i);
				local->addr->entry[i] = e_pilot_utf8_to_pchar (phone_str);
			}
		}
	}
	
	/* Note */
	local->addr->entry[entryNote] = e_pilot_utf8_to_pchar (ecard->note);

	g_object_unref (simple);
}

static void 
local_record_from_uid (EAddrLocalRecord *local,
		       const char *uid,
		       EAddrConduitContext *ctxt)
{
	ECard *ecard = NULL;
	GList *l;
	
	g_assert (local != NULL);

	for (l = ctxt->cards; l != NULL; l = l->next) {
		ecard = l->data;
		
		if (ecard->id && !strcmp (ecard->id, uid))
			break;

		ecard = NULL;
	}

	if (ecard != NULL) {
		local_record_from_ecard (local, ecard, ctxt);
	} else {
		ecard = e_card_new ("");
		e_card_set_id (ecard, uid);
		local_record_from_ecard (local, ecard, ctxt);
		g_object_unref (ecard);
	}
}

static ECard *
ecard_from_remote_record(EAddrConduitContext *ctxt,
			 GnomePilotRecord *remote,
			 ECard *in_card)
{
	struct Address address;
	ECard *ecard;
	ECardSimple *simple;
	ECardName *name;
	ECardDeliveryAddress *delivery;
	ECardAddrLabel *label;
	ECardSimpleAddressId mailing_address;
	char *txt, *find;
	ECardSimpleField next_mail, next_home, next_work, next_fax;
	ECardSimpleField next_other, next_main, next_pager, next_mobile;
	int i;

	g_return_val_if_fail(remote!=NULL,NULL);
	memset (&address, 0, sizeof (struct Address));
	unpack_Address (&address, remote->record, remote->length);

	if (in_card == NULL)
		ecard = e_card_new("");
	else
		ecard = e_card_duplicate (in_card);

	/* Name */
	name = e_card_name_copy (ecard->name);
	name->given = get_entry_text (address, entryFirstname);
	name->family = get_entry_text (address, entryLastname);

	simple = e_card_simple_new (ecard);
	txt = e_card_name_to_string (name);	
	e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_FULL_NAME, txt);
	e_card_simple_set_name (simple, name);

	/* File as */
	if (!(txt && *txt))
		e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_FILE_AS, 
				  address.entry[entryCompany]);

	g_free (txt);
	e_card_name_unref (name);

	/* Title and Company */
	txt = get_entry_text (address, entryTitle);
	e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_TITLE, txt);
	g_free (txt);

	txt = get_entry_text (address, entryCompany);
	e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_ORG, txt);
	g_free (txt);

	/* Address */
	mailing_address = -1;
	for (i = 0; i < E_CARD_SIMPLE_ADDRESS_ID_LAST; i++) {
		const ECardAddrLabel *addr;

		addr = e_card_simple_get_address(simple, i);
		if (addr && (addr->flags & E_CARD_ADDR_DEFAULT))
			mailing_address = i;
	}
	if (mailing_address == -1)
		mailing_address = ctxt->cfg->default_address;

	delivery = e_card_delivery_address_new ();
	delivery->flags |= E_CARD_ADDR_DEFAULT;
	txt = get_entry_text (address, entryAddress);
	if ((find = strchr (txt, '\n')) != NULL) {
		*find = '\0';
		find++;
	}
	delivery->street = txt;
	delivery->ext = find != NULL ? find : g_strdup ("");
	delivery->city = get_entry_text (address, entryCity);
	delivery->region = get_entry_text (address, entryState);
	delivery->country = get_entry_text (address, entryCountry);
	delivery->code = get_entry_text (address, entryZip);

	label = e_card_address_label_new ();
	label->flags |= E_CARD_ADDR_DEFAULT;
	label->data = e_card_delivery_address_to_string (delivery);

	e_card_simple_set_address (simple, mailing_address, label);
	e_card_simple_set_delivery_address (simple, mailing_address, delivery);
	
	e_card_delivery_address_unref (delivery);
	e_card_address_label_unref (label);
	
	/* Phone numbers */
	get_next_init (&next_mail, &next_home, &next_work, &next_fax,
		       &next_other, &next_main, &next_pager, &next_mobile);

	for (i = entryPhone1; i <= entryPhone5; i++) {
		int phonelabel = address.phoneLabel[i - entryPhone1];
		char *phonenum = get_entry_text (address, i);
		
		if (phonelabel == LABEL_EMAIL && !is_next_done (next_mail)) {
			e_card_simple_set (simple, next_mail, phonenum);
			next_mail = get_next_mail (&next_mail);
		} else if (phonelabel == LABEL_HOME && !is_next_done (next_home)) {
			e_card_simple_set (simple, next_home, phonenum);
			next_home = get_next_home (&next_home);
		} else if (phonelabel == LABEL_WORK && !is_next_done (next_work)) {
			e_card_simple_set (simple, next_work, phonenum);
			next_work = get_next_work (&next_work);
		} else if (phonelabel == LABEL_FAX && !is_next_done (next_fax)) {
			e_card_simple_set (simple, next_fax, phonenum);
			next_fax = get_next_fax (&next_fax);
		} else if (phonelabel == LABEL_OTHER && !is_next_done (next_other)) {
			e_card_simple_set (simple, next_other, phonenum);
			next_other = get_next_other (&next_other);
		} else if (phonelabel == LABEL_MAIN && !is_next_done (next_main)) {
			e_card_simple_set (simple, next_main, phonenum);
			next_main = get_next_main (&next_main);
		} else if (phonelabel == LABEL_PAGER && !is_next_done (next_pager)) {
			e_card_simple_set (simple, next_pager, phonenum);
			next_pager = get_next_pager (&next_pager);
		} else if (phonelabel == LABEL_MOBILE && !is_next_done (next_mobile)) {
			e_card_simple_set (simple, next_mobile, phonenum);
			next_mobile = get_next_mobile (&next_mobile);
		}
		
		g_free (phonenum);
	}

	/* Note */
	txt = get_entry_text (address, entryNote);
	e_card_simple_set(simple, E_CARD_SIMPLE_FIELD_NOTE, txt);
	g_free (txt);
	
	e_card_simple_sync_card (simple);
	g_object_unref(simple);

	free_Address(&address);

	return ecard;
}

static void
check_for_slow_setting (GnomePilotConduit *c, EAddrConduitContext *ctxt)
{
	GnomePilotConduitStandard *conduit = GNOME_PILOT_CONDUIT_STANDARD (c);
	int map_count;
	const char *uri;
	
	map_count = g_hash_table_size (ctxt->map->pid_map);	
	if (map_count == 0)
		gnome_pilot_conduit_standard_set_slow (conduit, TRUE);

	/* Or if the URI's don't match */
	uri = e_book_get_uri (ctxt->ebook);
	LOG (g_message ("  Current URI %s (%s)\n", uri, ctxt->cfg->last_uri ? ctxt->cfg->last_uri : "<NONE>"));
	if (ctxt->cfg->last_uri != NULL && strcmp (ctxt->cfg->last_uri, uri)) {
		gnome_pilot_conduit_standard_set_slow (conduit, TRUE);
		e_pilot_map_clear (ctxt->map);
	}

	if (gnome_pilot_conduit_standard_get_slow (conduit)) {
		ctxt->map->write_touched_only = TRUE;
		LOG (g_message ( "    doing slow sync\n" ));
	} else {
		LOG (g_message ( "    doing fast sync\n" ));
	}
}

static void
card_added (EBookView *book_view, const GList *cards, EAddrConduitContext *ctxt)
{
	const GList *l;

	for (l = cards; l != NULL; l = l->next) {
		ECard *card = E_CARD (l->data);
		CardObjectChange *coc;
		
		if (e_card_evolution_list (card))
			continue;
		
		coc = g_new0 (CardObjectChange, 1);
		coc->card = card;
		coc->type = CARD_ADDED;

		g_object_ref (coc->card);
		ctxt->changed = g_list_prepend (ctxt->changed, coc);
		if (!e_pilot_map_uid_is_archived (ctxt->map, e_card_get_id (coc->card)))
			g_hash_table_insert (ctxt->changed_hash, (gpointer)e_card_get_id (coc->card), coc);
	}
}

static void
card_changed (EBookView *book_view, const GList *cards, EAddrConduitContext *ctxt)
{
	const GList *l;

	for (l = cards; l != NULL; l = l->next) {
		ECard *card = E_CARD (l->data);
		CardObjectChange *coc;

		if (e_card_evolution_list (card))
			continue;
		
		coc = g_new0 (CardObjectChange, 1);		
		coc->card = E_CARD (l->data);
		coc->type = CARD_MODIFIED;

		g_object_ref (coc->card);
		ctxt->changed = g_list_prepend (ctxt->changed, coc);
		if (!e_pilot_map_uid_is_archived (ctxt->map, e_card_get_id (coc->card)))
			g_hash_table_insert (ctxt->changed_hash, (gpointer)e_card_get_id (coc->card), coc);
	}
}


static void
card_removed (EBookView *book_view, GList *ids, EAddrConduitContext *ctxt)
{
	GList *l;

	for (l = ids; l != NULL; l = l->next) {
		const char *id = l->data;
		CardObjectChange *coc;
		gboolean archived;
		
		archived = e_pilot_map_uid_is_archived (ctxt->map, id);
	
		/* If its deleted, not in the archive and not in the map its a list */
		if (!archived && e_pilot_map_lookup_pid (ctxt->map, id, FALSE) == 0)
			return;	
	
		coc = g_new0 (CardObjectChange, 1);
		coc->card = e_card_new ("");
		e_card_set_id (coc->card, id);
		coc->type = CARD_DELETED;

		ctxt->changed = g_list_prepend (ctxt->changed, coc);
	
		if (!archived)
			g_hash_table_insert (ctxt->changed_hash, (gpointer)e_card_get_id (coc->card), coc);
		else
			e_pilot_map_remove_by_uid (ctxt->map, id);
	}
}

static void
sequence_complete (EBookView *book_view, EBookViewStatus status, EAddrConduitContext *ctxt)
{
	g_signal_handlers_disconnect_matched(book_view,
					     G_SIGNAL_MATCH_DATA,
					     0, 0,
					     NULL, NULL, ctxt);
	g_object_unref (book_view);
  	gtk_main_quit ();
}

static void
view_cb (EBook *book, EBookStatus status, EBookView *book_view, gpointer data)
{
	EAddrConduitContext *ctxt = data;
	
	g_object_ref (book_view);
	
  	g_signal_connect (book_view, "card_added", 
			  G_CALLBACK (card_added), ctxt);
	g_signal_connect (book_view, "card_changed", 
			  G_CALLBACK (card_changed), ctxt);
	g_signal_connect (book_view, "card_removed", 
			  G_CALLBACK (card_removed), ctxt);
  	g_signal_connect (book_view, "sequence_complete", 
			  G_CALLBACK (sequence_complete), ctxt);

}

/* Pilot syncing callbacks */
static gint
pre_sync (GnomePilotConduit *conduit,
	  GnomePilotDBInfo *dbi,
	  EAddrConduitContext *ctxt)
{
	GnomePilotConduitSyncAbs *abs_conduit;
/*    	GList *l; */
	int len;
	unsigned char *buf;
	char *filename;
	char *change_id;
/*  	gint num_records; */

	abs_conduit = GNOME_PILOT_CONDUIT_SYNC_ABS (conduit);

	LOG (g_message ( "---------------------------------------------------------\n" ));
	LOG (g_message ( "pre_sync: Addressbook Conduit v.%s", CONDUIT_VERSION ));
	/* g_message ("Addressbook Conduit v.%s", CONDUIT_VERSION); */

	ctxt->dbi = dbi;	
	ctxt->ebook = NULL;
	
	if (start_addressbook_server (ctxt) != 0) {
		WARN(_("Could not start wombat server"));
		gnome_pilot_conduit_error (conduit, _("Could not start wombat"));
		return -1;
	}

	/* Load the uid <--> pilot id mappings */
	filename = map_name (ctxt);
	e_pilot_map_read (filename, &ctxt->map);
	g_free (filename);

	/* Count and hash the changes */
	change_id = g_strdup_printf ("pilot-sync-evolution-addressbook-%d", ctxt->cfg->pilot_id);
	ctxt->changed_hash = g_hash_table_new (g_str_hash, g_str_equal);
	e_book_get_changes (ctxt->ebook, change_id, view_cb, ctxt);
	
	/* Force the view loading to be synchronous */
	gtk_main ();
	g_free (change_id);
	
	/* Set the count information */
/*  	num_records = cal_client_get_n_objects (ctxt->client, CALOBJ_TYPE_TODO); */
/*  	gnome_pilot_conduit_sync_abs_set_num_local_records(abs_conduit, num_records); */
/*  	gnome_pilot_conduit_sync_abs_set_num_new_local_records (abs_conduit, add_records); */
/*  	gnome_pilot_conduit_sync_abs_set_num_updated_local_records (abs_conduit, mod_records); */
/*  	gnome_pilot_conduit_sync_abs_set_num_deleted_local_records(abs_conduit, del_records); */

	buf = (unsigned char*)g_malloc (0xffff);
	len = dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,
			      (unsigned char *)buf, 0xffff);
	
	if (len < 0) {
		WARN (_("Could not read pilot's Address application block"));
		WARN ("dlp_ReadAppBlock(...) = %d", len);
		gnome_pilot_conduit_error (conduit,
					   _("Could not read pilot's Address application block"));
		return -1;
	}
	unpack_AddressAppInfo (&(ctxt->ai), buf, len);
	g_free (buf);

  	check_for_slow_setting (conduit, ctxt);
	if (ctxt->cfg->sync_type == GnomePilotConduitSyncTypeCopyToPilot
	    || ctxt->cfg->sync_type == GnomePilotConduitSyncTypeCopyFromPilot)
		ctxt->map->write_touched_only = TRUE;

	return 0;
}

static gint
post_sync (GnomePilotConduit *conduit,
	   GnomePilotDBInfo *dbi,
	   EAddrConduitContext *ctxt)
{
	gchar *filename, *change_id;
	
	LOG (g_message ( "post_sync: Address Conduit v.%s", CONDUIT_VERSION ));

	g_free (ctxt->cfg->last_uri);
	ctxt->cfg->last_uri = g_strdup (e_book_get_uri (ctxt->ebook));
	addrconduit_save_configuration (ctxt->cfg);

	filename = map_name (ctxt);
	e_pilot_map_write (filename, ctxt->map);
	g_free (filename);

	/* FIX ME ugly hack - our changes musn't count, this does introduce
	 * a race condition if anyone changes a record elsewhere during sycnc
         */	
	change_id = g_strdup_printf ("pilot-sync-evolution-addressbook-%d", ctxt->cfg->pilot_id);
	e_book_get_changes (ctxt->ebook, change_id, view_cb, ctxt);
	g_free (change_id);
	gtk_main ();

	LOG (g_message ( "---------------------------------------------------------\n" ));
	
	return 0;
}

static gint
set_pilot_id (GnomePilotConduitSyncAbs *conduit,
	      EAddrLocalRecord *local,
	      guint32 ID,
	      EAddrConduitContext *ctxt)
{
	LOG (g_message ( "set_pilot_id: setting to %d\n", ID ));
	
	e_pilot_map_insert (ctxt->map, ID, local->ecard->id, FALSE);

        return 0;
}

static gint
set_status_cleared (GnomePilotConduitSyncAbs *conduit,
		    EAddrLocalRecord *local,
		    EAddrConduitContext *ctxt)
{
	LOG (g_message ( "set_status_cleared: clearing status\n" ));
	
	g_hash_table_remove (ctxt->changed_hash, e_card_get_id (local->ecard));
	
        return 0;
}

static gint
for_each (GnomePilotConduitSyncAbs *conduit,
	  EAddrLocalRecord **local,
	  EAddrConduitContext *ctxt)
{
  	static GList *cards, *iterator;
  	static int count;

  	g_return_val_if_fail (local != NULL, -1);

	if (*local == NULL) {
		LOG (g_message ( "beginning for_each" ));

		cards = ctxt->cards;
		count = 0;
		
		if (cards != NULL) {
			LOG (g_message ( "iterating over %d records", g_list_length (cards) ));

			*local = g_new0 (EAddrLocalRecord, 1);
  			local_record_from_ecard (*local, cards->data, ctxt);
			g_list_prepend (ctxt->locals, *local);

			iterator = cards;
		} else {
			LOG (g_message ( "no events" ));
			(*local) = NULL;
			return 0;
		}
	} else {
		count++;
		if (g_list_next (iterator)) {
			iterator = g_list_next (iterator);

			*local = g_new0 (EAddrLocalRecord, 1);
			local_record_from_ecard (*local, iterator->data, ctxt);
			g_list_prepend (ctxt->locals, *local);
		} else {
			LOG (g_message ( "for_each ending" ));

  			/* Tell the pilot the iteration is over */
			*local = NULL;

			return 0;
		}
	}

	return 0;
}

static gint
for_each_modified (GnomePilotConduitSyncAbs *conduit,
		   EAddrLocalRecord **local,
		   EAddrConduitContext *ctxt)
{
	static GList *iterator;
	static int count;

	g_return_val_if_fail (local != NULL, 0);

	if (*local == NULL) {
		LOG (g_message ( "for_each_modified beginning\n" ));
		
		iterator = ctxt->changed;
		
		count = 0;
		
		iterator = next_changed_item (ctxt, iterator);
		if (iterator != NULL) {
			CardObjectChange *coc = iterator->data;
			
			LOG (g_message ( "iterating over %d records", g_hash_table_size (ctxt->changed_hash) ));
			 
			*local = g_new0 (EAddrLocalRecord, 1);
			local_record_from_ecard (*local, coc->card, ctxt);
			g_list_prepend (ctxt->locals, *local);
		} else {
			LOG (g_message ( "no events" ));

			*local = NULL;
		}
	} else {
		count++;
		iterator = g_list_next (iterator);
		if (iterator && (iterator = next_changed_item (ctxt, iterator))) {
			CardObjectChange *coc = iterator->data;

			*local = g_new0 (EAddrLocalRecord, 1);
			local_record_from_ecard (*local, coc->card, ctxt);
			g_list_prepend (ctxt->locals, *local);
		} else {
			LOG (g_message ( "for_each_modified ending" ));

    			/* Signal the iteration is over */
			*local = NULL;

			return 0;
		}
	}

	return 0;
}

static gint
compare (GnomePilotConduitSyncAbs *conduit,
	 EAddrLocalRecord *local,
	 GnomePilotRecord *remote,
	 EAddrConduitContext *ctxt)
{
	GnomePilotRecord local_pilot;
	int retval = 0;

	LOG (g_message ("compare: local=%s remote=%s...\n",
			print_local (local), print_remote (remote)));

	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	local_pilot = local_record_to_pilot_record (local, ctxt);

	if (remote->length != local_pilot.length
	    || memcmp (local_pilot.record, remote->record, remote->length))
		retval = 1;
	
	if (retval == 0)
		LOG (g_message ( "    equal" ));
	else
		LOG (g_message ( "    not equal" ));
	
	return retval;
}

static gint
add_record (GnomePilotConduitSyncAbs *conduit,
	    GnomePilotRecord *remote,
	    EAddrConduitContext *ctxt)
{
	ECard *ecard;
	CardObjectChangeStatus cons;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG (g_message ( "add_record: adding %s to desktop\n", print_remote (remote) ));

	ecard = ecard_from_remote_record (ctxt, remote, NULL);
	
	/* add the ecard to the server */
	e_book_add_card (ctxt->ebook, ecard, add_card_cb, &cons);

	gtk_main(); /* enter sub mainloop */
	
	if (cons.status != E_BOOK_STATUS_SUCCESS) {
		WARN ("add_record: failed to add card to ebook\n");
		return -1;
	}

	e_card_set_id (ecard, cons.id);
	e_pilot_map_insert (ctxt->map, remote->ID, ecard->id, FALSE);

	g_object_unref (ecard);

	return retval;
}

static gint
replace_record (GnomePilotConduitSyncAbs *conduit,
		EAddrLocalRecord *local,
		GnomePilotRecord *remote,
		EAddrConduitContext *ctxt)
{
	ECard *new_ecard;
	EBookStatus commit_status;
	CardObjectChange *coc;
	CardObjectChangeStatus cons;
	char *old_id;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG (g_message ("replace_record: replace %s with %s\n",
	     print_local (local), print_remote (remote)));

	old_id = g_strdup (e_card_get_id (local->ecard));
	coc = g_hash_table_lookup (ctxt->changed_hash, old_id);
	
	new_ecard = ecard_from_remote_record (ctxt, remote, local->ecard);
	g_object_unref (local->ecard);
	local->ecard = new_ecard;

	if (coc && coc->type == CARD_DELETED)
		e_book_add_card (ctxt->ebook, local->ecard, add_card_cb, &cons);
	else
		e_book_commit_card (ctxt->ebook, local->ecard, status_cb, &commit_status);
	
	gtk_main (); /* enter sub mainloop */

	/* Adding a record causes wombat to assign a new uid so we must tidy */
	if (coc && coc->type == CARD_DELETED) {
		gboolean arch = e_pilot_map_uid_is_archived (ctxt->map, e_card_get_id (local->ecard));
		
		e_card_set_id (local->ecard, cons.id);
		e_pilot_map_insert (ctxt->map, remote->ID, cons.id, arch);

		coc = g_hash_table_lookup (ctxt->changed_hash, old_id);
		if (coc) {
			g_hash_table_remove (ctxt->changed_hash, e_card_get_id (coc->card));
			g_object_unref (coc->card);
			g_object_ref (local->ecard);
			coc->card = local->ecard;
			g_hash_table_insert (ctxt->changed_hash, (gpointer)e_card_get_id (coc->card), coc);
		}
		
		commit_status = cons.status;
	}
	
	if (commit_status != E_BOOK_STATUS_SUCCESS)
		WARN ("replace_record: failed to update card in ebook\n");

	return retval;
}

static gint
delete_record (GnomePilotConduitSyncAbs *conduit,
	       EAddrLocalRecord *local,
	       EAddrConduitContext *ctxt)
{
	EBookStatus commit_status;
	int retval = 0;
	
	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (local->ecard != NULL, -1);

	LOG (g_message ( "delete_record: delete %s\n", print_local (local) ));

	e_pilot_map_remove_by_uid (ctxt->map, local->ecard->id);
	e_book_remove_card_by_id (ctxt->ebook, local->ecard->id, status_cb, &commit_status);
	
	gtk_main (); /* enter sub mainloop */
	
	if (commit_status != E_BOOK_STATUS_SUCCESS && commit_status != E_BOOK_STATUS_CARD_NOT_FOUND)
		WARN ("delete_record: failed to delete card in ebook\n");
	
	return retval;
}

static gint
archive_record (GnomePilotConduitSyncAbs *conduit,
		EAddrLocalRecord *local,
		gboolean archive,
		EAddrConduitContext *ctxt)
{
	int retval = 0;
	
	g_return_val_if_fail (local != NULL, -1);

	LOG (g_message ( "archive_record: %s\n", archive ? "yes" : "no" ));

	e_pilot_map_insert (ctxt->map, local->local.ID, local->ecard->id, archive);
	
        return retval;
}

static gint
match (GnomePilotConduitSyncAbs *conduit,
       GnomePilotRecord *remote,
       EAddrLocalRecord **local,
       EAddrConduitContext *ctxt)
{
  	const char *uid;
	
	LOG (g_message ("match: looking for local copy of %s\n",
	     print_remote (remote)));	
	
	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	*local = NULL;
	uid = e_pilot_map_lookup_uid (ctxt->map, remote->ID, TRUE);
	
	if (!uid)
		return 0;

	LOG (g_message ( "  matched\n" ));
	
	*local = g_new0 (EAddrLocalRecord, 1);
	local_record_from_uid (*local, uid, ctxt);
	
	return 0;
}

static gint
free_match (GnomePilotConduitSyncAbs *conduit,
	    EAddrLocalRecord *local,
	    EAddrConduitContext *ctxt)
{
	LOG (g_message ( "free_match: freeing\n" ));

	g_return_val_if_fail (local != NULL, -1);

	addrconduit_destroy_record (local);

	return 0;
}

static gint
prepare (GnomePilotConduitSyncAbs *conduit,
	 EAddrLocalRecord *local,
	 GnomePilotRecord *remote,
	 EAddrConduitContext *ctxt)
{
	LOG (g_message ( "prepare: encoding local %s\n", print_local (local) ));
	
	*remote = local_record_to_pilot_record (local, ctxt);
	
	return 0;
}

/* Pilot Settings Callbacks */
static void
fill_widgets (EAddrConduitContext *ctxt)
{
	e_pilot_settings_set_secret (E_PILOT_SETTINGS (ctxt->ps),
				     ctxt->cfg->secret);

	e_addr_gui_fill_widgets (ctxt->gui, ctxt->cfg);
}

static gint
create_settings_window (GnomePilotConduit *conduit,
			GtkWidget *parent,
			EAddrConduitContext *ctxt)
{
	LOG (g_message ( "create_settings_window" ));

	ctxt->ps = e_pilot_settings_new ();
	ctxt->gui = e_addr_gui_new (E_PILOT_SETTINGS (ctxt->ps));

	gtk_container_add (GTK_CONTAINER (parent), ctxt->ps);
	gtk_widget_show (ctxt->ps);

	fill_widgets (ctxt);
	
	return 0;
}
static void
display_settings (GnomePilotConduit *conduit, EAddrConduitContext *ctxt)
{
	LOG (g_message ( "display_settings" ));
	
	fill_widgets (ctxt);
}

static void
save_settings    (GnomePilotConduit *conduit, EAddrConduitContext *ctxt)
{
	LOG (g_message ( "save_settings" ));

	ctxt->new_cfg->secret =
		e_pilot_settings_get_secret (E_PILOT_SETTINGS (ctxt->ps));
	e_addr_gui_fill_config (ctxt->gui, ctxt->new_cfg);

	addrconduit_save_configuration (ctxt->new_cfg);
}

static void
revert_settings  (GnomePilotConduit *conduit, EAddrConduitContext *ctxt)
{
	LOG (g_message ( "revert_settings" ));

	addrconduit_save_configuration (ctxt->cfg);
	addrconduit_destroy_configuration (ctxt->new_cfg);
	ctxt->new_cfg = addrconduit_dupe_configuration (ctxt->cfg);
}

GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilot_id)
{
	GtkObject *retval;
	EAddrConduitContext *ctxt;

	LOG (g_message ( "in address's conduit_get_gpilot_conduit\n" ));

	retval = gnome_pilot_conduit_sync_abs_new ("AddressDB", 0x61646472);
	g_assert (retval != NULL);

	ctxt = e_addr_context_new (pilot_id);
	gtk_object_set_data (GTK_OBJECT (retval), "addrconduit_context", ctxt);

	gtk_signal_connect (retval, "pre_sync", (GtkSignalFunc) pre_sync, ctxt);
	gtk_signal_connect (retval, "post_sync", (GtkSignalFunc) post_sync, ctxt);

  	gtk_signal_connect (retval, "set_pilot_id", (GtkSignalFunc) set_pilot_id, ctxt);
  	gtk_signal_connect (retval, "set_status_cleared", (GtkSignalFunc) set_status_cleared, ctxt);

  	gtk_signal_connect (retval, "for_each", (GtkSignalFunc) for_each, ctxt);
  	gtk_signal_connect (retval, "for_each_modified", (GtkSignalFunc) for_each_modified, ctxt);
  	gtk_signal_connect (retval, "compare", (GtkSignalFunc) compare, ctxt);

  	gtk_signal_connect (retval, "add_record", (GtkSignalFunc) add_record, ctxt);
  	gtk_signal_connect (retval, "replace_record", (GtkSignalFunc) replace_record, ctxt);
  	gtk_signal_connect (retval, "delete_record", (GtkSignalFunc) delete_record, ctxt);
  	gtk_signal_connect (retval, "archive_record", (GtkSignalFunc) archive_record, ctxt);

  	gtk_signal_connect (retval, "match", (GtkSignalFunc) match, ctxt);
  	gtk_signal_connect (retval, "free_match", (GtkSignalFunc) free_match, ctxt);

  	gtk_signal_connect (retval, "prepare", (GtkSignalFunc) prepare, ctxt);

	/* Gui Settings */
	gtk_signal_connect (retval, "create_settings_window", (GtkSignalFunc) create_settings_window, ctxt);
	gtk_signal_connect (retval, "display_settings", (GtkSignalFunc) display_settings, ctxt);
	gtk_signal_connect (retval, "save_settings", (GtkSignalFunc) save_settings, ctxt);
	gtk_signal_connect (retval, "revert_settings", (GtkSignalFunc) revert_settings, ctxt);

	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{ 
	EAddrConduitContext *ctxt;

	ctxt = gtk_object_get_data (GTK_OBJECT (conduit), 
				    "addrconduit_context");

	e_addr_context_destroy (ctxt);

	gtk_object_destroy (GTK_OBJECT (conduit));
}
