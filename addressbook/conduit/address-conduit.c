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
#include <libebook/e-book.h>
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

static EContactField priority [] = {
	E_CONTACT_PHONE_BUSINESS,
	E_CONTACT_PHONE_HOME,
	E_CONTACT_PHONE_BUSINESS_FAX,
	E_CONTACT_EMAIL_1,
	E_CONTACT_PHONE_PAGER,
	E_CONTACT_PHONE_MOBILE,
	E_CONTACT_PHONE_BUSINESS_2,
	E_CONTACT_PHONE_HOME_2,
	E_CONTACT_PHONE_HOME_FAX,
	E_CONTACT_EMAIL_2,
	E_CONTACT_PHONE_OTHER,
	E_CONTACT_PHONE_PRIMARY,
	E_CONTACT_PHONE_OTHER_FAX,
	E_CONTACT_EMAIL_3,
	E_CONTACT_FIELD_LAST
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
	EContact *contact;

        /* pilot-link address structure, used for implementing Transmit. */
	struct Address *addr;
};


static void
addrconduit_destroy_record (EAddrLocalRecord *local) 
{
	g_object_unref (local->contact);
	free_Address (local->addr);
	g_free (local->addr);
	g_free (local);
}

/* Configuration */
struct _EAddrConduitCfg {
	guint32 pilot_id;
	GnomePilotConduitSyncType  sync_type;

	gboolean secret;
	EContactField default_address;

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
		c->default_address = E_CONTACT_ADDRESS_WORK;
	else if (!strcmp (address, "home"))
		c->default_address = E_CONTACT_ADDRESS_HOME;
	else if (!strcmp (address, "other"))
		c->default_address = E_CONTACT_ADDRESS_OTHER;
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
	case E_CONTACT_ADDRESS_WORK:
		gnome_config_set_string ("default_address", "business");
		break;
	case E_CONTACT_ADDRESS_HOME:
		gnome_config_set_string ("default_address", "home");
		break;
	case E_CONTACT_ADDRESS_OTHER:
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
	E_CONTACT_ADDRESS_WORK,
	E_CONTACT_ADDRESS_HOME,
	E_CONTACT_ADDRESS_OTHER,
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
	
	if (ctxt->changed != NULL)
		e_book_free_change_list (ctxt->changed);
	
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

/* Utility routines */
static char *
map_name (EAddrConduitContext *ctxt) 
{
	char *filename = NULL;
	
	filename = g_strdup_printf ("%s/.evolution/addressbook/local/system/pilot-map-%d.xml", g_get_home_dir (), ctxt->cfg->pilot_id);
	
	return filename;
}

static GList *
next_changed_item (EAddrConduitContext *ctxt, GList *changes) 
{
	EBookChange *ebc;
	GList *l;
	
	for (l = changes; l != NULL; l = l->next) {
		ebc = l->data;
		
		if (g_hash_table_lookup (ctxt->changed_hash, e_contact_get_const (ebc->contact, E_CONTACT_UID)))
			return l;
	}
	
	return NULL;
}

static EContactField
get_next_mail (EContactField *field)
{
	if (field == NULL)
		return E_CONTACT_EMAIL_1;
	
	switch (*field) {
	case E_CONTACT_EMAIL_1:
		return E_CONTACT_EMAIL_2;
	case E_CONTACT_EMAIL_2:
		return E_CONTACT_EMAIL_3;
	default:
		break;
	}

	return E_CONTACT_FIELD_LAST;
}

static EContactField
get_next_home (EContactField *field)
{
	if (field == NULL)
		return E_CONTACT_PHONE_HOME;
	
	switch (*field) {
	case E_CONTACT_PHONE_HOME:
		return E_CONTACT_PHONE_HOME_2;
	default:
		break;
	}

	return E_CONTACT_FIELD_LAST;
}

static EContactField
get_next_work (EContactField *field)
{
	if (field == NULL)
		return E_CONTACT_PHONE_BUSINESS;
	
	switch (*field) {
	case E_CONTACT_PHONE_BUSINESS:
		return E_CONTACT_PHONE_BUSINESS_2;
	default:
		break;
	}

	return E_CONTACT_FIELD_LAST;
}

static EContactField
get_next_fax (EContactField *field)
{
	if (field == NULL)
		return E_CONTACT_PHONE_BUSINESS_FAX;
	
	switch (*field) {
	case E_CONTACT_PHONE_BUSINESS_FAX:
		return E_CONTACT_PHONE_HOME_FAX;
	case E_CONTACT_PHONE_HOME_FAX:
		return E_CONTACT_PHONE_OTHER_FAX;
	default:
		break;
	}

	return E_CONTACT_FIELD_LAST;
}

static EContactField
get_next_other (EContactField *field)
{
	if (field == NULL)
		return E_CONTACT_PHONE_OTHER;

	return E_CONTACT_FIELD_LAST;
}

static EContactField
get_next_main (EContactField *field)
{
	if (field == NULL)
		return E_CONTACT_PHONE_PRIMARY;

	return E_CONTACT_FIELD_LAST;
}

static EContactField
get_next_pager (EContactField *field)
{
	if (field == NULL)
		return E_CONTACT_PHONE_PAGER;

	return E_CONTACT_FIELD_LAST;
}

static EContactField
get_next_mobile (EContactField *field)
{
	if (field == NULL)
		return E_CONTACT_PHONE_MOBILE;

	return E_CONTACT_FIELD_LAST;
}

static void
get_next_init (EContactField *next_mail,
	       EContactField *next_home,
	       EContactField *next_work,
	       EContactField *next_fax,
	       EContactField *next_other,
	       EContactField *next_main,
	       EContactField *next_pager,
	       EContactField *next_mobile)
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
is_next_done (EContactField field)
{
	if (field == E_CONTACT_FIELD_LAST)
		return TRUE;
	
	return FALSE;
}

static gboolean
is_syncable (EAddrConduitContext *ctxt, EAddrLocalRecord *local) 
{	
	EContactField next_mail, next_home, next_work, next_fax;
	EContactField next_other, next_main, next_pager, next_mobile;
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

static void
set_contact_text (EContact *contact, EContactField field, struct Address address, int entry)
{
	char *text = NULL;
	
	if (address.entry[entry])
		text = e_pilot_utf8_from_pchar (address.entry[entry]);

	e_contact_set (contact, field, text);
	
	g_free (text);
}

static char *
get_entry_text (struct Address address, int entry)
{
	if (address.entry[entry])
		return e_pilot_utf8_from_pchar (address.entry[entry]);

	return NULL;	
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
	EBookChange *ebc;

	local->local.archived = FALSE;
	local->local.secret = FALSE;

	ebc = g_hash_table_lookup (ctxt->changed_hash, uid);
	
	if (ebc == NULL) {
		local->local.attr = GnomePilotRecordNothing;
		return;
	}
	
	switch (ebc->change_type) {
	case E_BOOK_CHANGE_CARD_ADDED:
		local->local.attr = GnomePilotRecordNew;
		break;	
	case E_BOOK_CHANGE_CARD_MODIFIED:
		local->local.attr = GnomePilotRecordModified;
		break;
	case E_BOOK_CHANGE_CARD_DELETED:
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
local_record_from_ecard (EAddrLocalRecord *local, EContact *contact, EAddrConduitContext *ctxt)
{
	EContactAddress *address = NULL;
	int phone = entryPhone1;
	EContactField field;
	gboolean syncable;
	int i;
	
	g_return_if_fail (local != NULL);
	g_return_if_fail (contact != NULL);

	local->contact = g_object_ref (contact);
	local->local.ID = e_pilot_map_lookup_pid (ctxt->map, e_contact_get_const (contact, E_CONTACT_UID), TRUE);

	compute_status (ctxt, local, e_contact_get_const (contact, E_CONTACT_UID));

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

	local->addr->entry[entryFirstname] = e_pilot_utf8_to_pchar (e_contact_get_const (contact, E_CONTACT_GIVEN_NAME));
	local->addr->entry[entryLastname] = e_pilot_utf8_to_pchar (e_contact_get_const (contact, E_CONTACT_FAMILY_NAME));
	local->addr->entry[entryCompany] = e_pilot_utf8_to_pchar (e_contact_get_const (contact, E_CONTACT_ORG));
	local->addr->entry[entryTitle] = e_pilot_utf8_to_pchar (e_contact_get_const (contact, E_CONTACT_TITLE));
	
	/* See if the default has something in it */
	if ((address = e_contact_get (contact, ctxt->cfg->default_address))) {
		field = ctxt->cfg->default_address;
	} else {
		/* Try to find a non-empty address field */
		for (field = E_CONTACT_FIRST_ADDRESS_ID; field <= E_CONTACT_LAST_ADDRESS_ID; field++) {
			if ((address = e_contact_get (contact, field)))
				break;
		}
	}
	
	if (address) {
		char *add;
		
		/* If the address has 2 lines, make sure both get added */
		if (address->ext != NULL)
			add = g_strconcat (address->street, "\n", address->ext, NULL);
		else
			add = g_strdup (address->street);
		local->addr->entry[entryAddress] = e_pilot_utf8_to_pchar (add);
		g_free (add);
		
		local->addr->entry[entryCity] = e_pilot_utf8_to_pchar (address->locality);
		local->addr->entry[entryState] = e_pilot_utf8_to_pchar (address->region);
		local->addr->entry[entryZip] = e_pilot_utf8_to_pchar (address->code);
		local->addr->entry[entryCountry] = e_pilot_utf8_to_pchar (address->country);
		
		e_contact_address_free (address);
	}
	
	/* Phone numbers */

	/* See if everything is syncable */
	syncable = is_syncable (ctxt, local);
	
	if (syncable) {
		INFO ("Syncable");

		/* Sync by priority */
		for (i = 0, phone = entryPhone1; 
		     priority[i] != E_CONTACT_FIELD_LAST && phone <= entryPhone5; i++) {
			const char *phone_str;
			
			phone_str = e_contact_get_const (contact, priority[i]);
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
		EContactField next_mail, next_home, next_work, next_fax;
		EContactField next_other, next_main, next_pager, next_mobile;

		INFO ("Not Syncable");
		get_next_init (&next_mail, &next_home, &next_work, &next_fax,
			       &next_other, &next_main, &next_pager, &next_mobile);

		/* Not completely syncable, so do the best we can */
		for (i = entryPhone1; i <= entryPhone5; i++) {
			int phonelabel = local->addr->phoneLabel[i - entryPhone1];
			const char *phone_str = NULL;
			
			if (phonelabel == LABEL_EMAIL && !is_next_done (next_mail)) {
				phone_str = e_contact_get_const (contact, next_mail);
				next_mail = get_next_mail (&next_mail);
			} else if (phonelabel == LABEL_HOME && !is_next_done (next_home)) {
  				phone_str = e_contact_get_const (contact, next_home);
				next_home = get_next_home (&next_home);
			} else if (phonelabel == LABEL_WORK && !is_next_done (next_work)) {
				phone_str = e_contact_get_const (contact, next_work);
				next_work = get_next_work (&next_work);
			} else if (phonelabel == LABEL_FAX && !is_next_done (next_fax)) {
				phone_str = e_contact_get_const (contact, next_fax);
				next_fax = get_next_fax (&next_fax);
			} else if (phonelabel == LABEL_OTHER && !is_next_done (next_other)) {
				phone_str = e_contact_get_const (contact, next_other);
				next_other = get_next_other (&next_other);
			} else if (phonelabel == LABEL_MAIN && !is_next_done (next_main)) {
				phone_str = e_contact_get_const (contact, next_main);
				next_main = get_next_main (&next_main);
			} else if (phonelabel == LABEL_PAGER && !is_next_done (next_pager)) {
				phone_str = e_contact_get_const (contact, next_pager);
				next_pager = get_next_pager (&next_pager);
			} else if (phonelabel == LABEL_MOBILE && !is_next_done (next_mobile)) {
				phone_str = e_contact_get_const (contact, next_mobile);
				next_mobile = get_next_mobile (&next_mobile);
			}
			
			if (phone_str && *phone_str) {
				clear_entry_text (*local->addr, i);
				local->addr->entry[i] = e_pilot_utf8_to_pchar (phone_str);
			}
		}
	}
	
	/* Note */
	local->addr->entry[entryNote] = e_pilot_utf8_to_pchar (e_contact_get_const (contact, E_CONTACT_NOTE));
}

static void 
local_record_from_uid (EAddrLocalRecord *local,
		       const char *uid,
		       EAddrConduitContext *ctxt)
{
	EContact *contact = NULL;
	GList *l;
	
	g_assert (local != NULL);

	for (l = ctxt->cards; l != NULL; l = l->next) {
		contact = l->data;
		
		/* FIXME Do we need to check for the empty string? */
		if (e_contact_get_const (contact, E_CONTACT_UID));
			break;

		contact = NULL;
	}

	if (contact != NULL) {
		local_record_from_ecard (local, contact, ctxt);
	} else {
		contact = e_contact_new ();
		e_contact_set (contact, E_CONTACT_UID, (gpointer) uid);
		local_record_from_ecard (local, contact, ctxt);
		g_object_unref (contact);
	}
}

static EContact *
ecard_from_remote_record(EAddrConduitContext *ctxt,
			 GnomePilotRecord *remote,
			 EContact *in_contact)
{
	struct Address address;
	EContact *contact;
	EContactName *name;
	EContactAddress *eaddress;
	EContactField mailing_address;
	char *txt, *find;
	EContactField next_mail, next_home, next_work, next_fax;
	EContactField next_other, next_main, next_pager, next_mobile;
	int i;

	g_return_val_if_fail(remote!=NULL,NULL);
	memset (&address, 0, sizeof (struct Address));
	unpack_Address (&address, remote->record, remote->length);

	if (in_contact == NULL)
		contact = e_contact_new ();
	else
		contact = e_contact_duplicate (in_contact);

	/* Name */
	name = e_contact_name_new ();
	name->given = get_entry_text (address, entryFirstname);
	name->family = get_entry_text (address, entryLastname);

	e_contact_set (contact, E_CONTACT_NAME, name);
	e_contact_name_free (name);
	
	/* File as */
	if (!e_contact_get_const (contact, E_CONTACT_FULL_NAME))
		set_contact_text (contact, E_CONTACT_FILE_AS, address, entryCompany);

	/* Title and Company */
	set_contact_text (contact, E_CONTACT_TITLE, address, entryTitle);
	set_contact_text (contact, E_CONTACT_ORG, address, entryCompany);

	/* Address */
	mailing_address = -1;
	if ((eaddress = e_contact_get (contact, ctxt->cfg->default_address))) {
		mailing_address = ctxt->cfg->default_address;
		e_contact_address_free (eaddress);
	} else {
		for (i = E_CONTACT_FIRST_ADDRESS_ID; i <= E_CONTACT_LAST_ADDRESS_ID; i++) {
			if ((eaddress = e_contact_get (contact, i))) {
				e_contact_address_free (eaddress);
				mailing_address = i;
				break;
			}
		}
	}
	
	if (mailing_address == -1)
		mailing_address = ctxt->cfg->default_address;
	
	eaddress = g_new0 (EContactAddress, 1);

	txt = get_entry_text (address, entryAddress);
	if (txt && (find = strchr (txt, '\n')) != NULL) {
		*find = '\0';
		find++;
	} else {
		find = NULL;
	}
	
	eaddress->street = txt;
	eaddress->ext = find != NULL ? g_strdup (find) : g_strdup ("");
	eaddress->locality = get_entry_text (address, entryCity);
	eaddress->region = get_entry_text (address, entryState);
	eaddress->country = get_entry_text (address, entryCountry);
	eaddress->code = get_entry_text (address, entryZip);
	
	e_contact_set (contact, mailing_address, eaddress);
	e_contact_address_free (eaddress);
	
	/* Phone numbers */
	get_next_init (&next_mail, &next_home, &next_work, &next_fax,
		       &next_other, &next_main, &next_pager, &next_mobile);

	for (i = entryPhone1; i <= entryPhone5; i++) {
		int phonelabel = address.phoneLabel[i - entryPhone1];
		char *phonenum = get_entry_text (address, i);
		
		if (phonelabel == LABEL_EMAIL && !is_next_done (next_mail)) {
			e_contact_set (contact, next_mail, phonenum);
			next_mail = get_next_mail (&next_mail);
		} else if (phonelabel == LABEL_HOME && !is_next_done (next_home)) {
			e_contact_set (contact, next_home, phonenum);
			next_home = get_next_home (&next_home);
		} else if (phonelabel == LABEL_WORK && !is_next_done (next_work)) {
			e_contact_set (contact, next_work, phonenum);
			next_work = get_next_work (&next_work);
		} else if (phonelabel == LABEL_FAX && !is_next_done (next_fax)) {
			e_contact_set (contact, next_fax, phonenum);
			next_fax = get_next_fax (&next_fax);
		} else if (phonelabel == LABEL_OTHER && !is_next_done (next_other)) {
			e_contact_set (contact, next_other, phonenum);
			next_other = get_next_other (&next_other);
		} else if (phonelabel == LABEL_MAIN && !is_next_done (next_main)) {
			e_contact_set (contact, next_main, phonenum);
			next_main = get_next_main (&next_main);
		} else if (phonelabel == LABEL_PAGER && !is_next_done (next_pager)) {
			e_contact_set (contact, next_pager, phonenum);
			next_pager = get_next_pager (&next_pager);
		} else if (phonelabel == LABEL_MOBILE && !is_next_done (next_mobile)) {
			e_contact_set (contact, next_mobile, phonenum);
			next_mobile = get_next_mobile (&next_mobile);
		}
		
		g_free (phonenum);
	}

	/* Note */
	set_contact_text (contact, E_CONTACT_NOTE, address, entryNote);

	free_Address(&address);

	return contact;
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

/* Pilot syncing callbacks */
static gint
pre_sync (GnomePilotConduit *conduit,
	  GnomePilotDBInfo *dbi,
	  EAddrConduitContext *ctxt)
{
	GnomePilotConduitSyncAbs *abs_conduit;
	EBookQuery *query;
    	GList *l;
	int len;
	unsigned char *buf;
	char *filename;
	char *change_id;
	gint num_records, add_records = 0, mod_records = 0, del_records = 0;

	abs_conduit = GNOME_PILOT_CONDUIT_SYNC_ABS (conduit);

	LOG (g_message ( "---------------------------------------------------------\n" ));
	LOG (g_message ( "pre_sync: Addressbook Conduit v.%s", CONDUIT_VERSION ));
	/* g_message ("Addressbook Conduit v.%s", CONDUIT_VERSION); */

	ctxt->dbi = dbi;
	
	/* FIXME Need to allow our own concept of "local" */
	ctxt->ebook = e_book_new ();
	if (!e_book_load_local_addressbook (ctxt->ebook, NULL)) {
		WARN(_("Could not load addressbook"));
		gnome_pilot_conduit_error (conduit, _("Could not load addressbook"));

		return -1;
	}

	/* Load the uid <--> pilot id mappings */
	filename = map_name (ctxt);
	e_pilot_map_read (filename, &ctxt->map);
	g_free (filename);

	/* Get a list of all contacts */
	if (!(query = e_book_query_any_field_contains (""))) {
		LOG (g_warning ("Failed to get EBookQuery"));
		return -1;
	}
	
	if (!e_book_get_contacts (ctxt->ebook, query, &ctxt->cards, NULL)) {
		LOG (g_warning ("Failed to get Contacts"));
		e_book_query_unref (query);
		return -1;
	}
	
	e_book_query_unref (query);
	
	/* Count and hash the changes */
	change_id = g_strdup_printf ("pilot-sync-evolution-addressbook-%d", ctxt->cfg->pilot_id);
	if (!e_book_get_changes (ctxt->ebook, change_id, &ctxt->changed, NULL))
		return -1;
	ctxt->changed_hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_free (change_id);	

	for (l = ctxt->changed; l != NULL; l = l->next) {
		EBookChange *ebc = l->data;
		const char *uid;
		
		uid = e_contact_get_const (ebc->contact, E_CONTACT_UID);
		if (!e_pilot_map_uid_is_archived (ctxt->map, uid)) {

			g_hash_table_insert (ctxt->changed_hash, g_strdup (uid), ebc);

			switch (ebc->change_type) {
			case E_BOOK_CHANGE_CARD_ADDED:
				add_records++;
				break;
			case E_BOOK_CHANGE_CARD_MODIFIED:
				mod_records++;
				break;
			case E_BOOK_CHANGE_CARD_DELETED:
				del_records++;
				break;
			}
		} else if (ebc->change_type == E_BOOK_CHANGE_CARD_DELETED) {
			e_pilot_map_remove_by_uid (ctxt->map, uid);
		}
	}
	
	/* Set the count information */
  	num_records = g_list_length (ctxt->cards);
  	gnome_pilot_conduit_sync_abs_set_num_local_records(abs_conduit, num_records);
  	gnome_pilot_conduit_sync_abs_set_num_new_local_records (abs_conduit, add_records);
  	gnome_pilot_conduit_sync_abs_set_num_updated_local_records (abs_conduit, mod_records);
  	gnome_pilot_conduit_sync_abs_set_num_deleted_local_records(abs_conduit, del_records);

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
	GList *changed;
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
	if (e_book_get_changes (ctxt->ebook, change_id, &changed, NULL))
		e_book_free_change_list (changed);
	g_free (change_id);

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
	
	e_pilot_map_insert (ctxt->map, ID, e_contact_get_const (local->contact, E_CONTACT_UID), FALSE);

        return 0;
}

static gint
set_status_cleared (GnomePilotConduitSyncAbs *conduit,
		    EAddrLocalRecord *local,
		    EAddrConduitContext *ctxt)
{
	LOG (g_message ( "set_status_cleared: clearing status\n" ));
	
	g_hash_table_remove (ctxt->changed_hash, e_contact_get_const (local->contact, E_CONTACT_UID));
	
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
			EBookChange *ebc = iterator->data;
			
			LOG (g_message ( "iterating over %d records", g_hash_table_size (ctxt->changed_hash)));
			 
			*local = g_new0 (EAddrLocalRecord, 1);
			local_record_from_ecard (*local, ebc->contact, ctxt);
			g_list_prepend (ctxt->locals, *local);
		} else {
			LOG (g_message ( "no events" ));

			*local = NULL;
		}
	} else {
		count++;
		iterator = g_list_next (iterator);
		if (iterator && (iterator = next_changed_item (ctxt, iterator))) {
			EBookChange *ebc = iterator->data;

			*local = g_new0 (EAddrLocalRecord, 1);
			local_record_from_ecard (*local, ebc->contact, ctxt);
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
	EContact *contact;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG (g_message ( "add_record: adding %s to desktop\n", print_remote (remote) ));

	contact = ecard_from_remote_record (ctxt, remote, NULL);
	
	/* add the ecard to the server */
	if (!e_book_add_contact (ctxt->ebook, contact, NULL)) {
		WARN ("add_record: failed to add card to ebook\n");
		g_object_unref (contact);
		
		return -1;
	}

	e_pilot_map_insert (ctxt->map, remote->ID, e_contact_get (contact, E_CONTACT_UID), FALSE);

	g_object_unref (contact);

	return retval;
}

static gint
replace_record (GnomePilotConduitSyncAbs *conduit,
		EAddrLocalRecord *local,
		GnomePilotRecord *remote,
		EAddrConduitContext *ctxt)
{
	EContact *new_contact;
	EBookChange *ebc;
	char *old_id;
	int retval = 0;
	
	g_return_val_if_fail (remote != NULL, -1);

	LOG (g_message ("replace_record: replace %s with %s\n",
	     print_local (local), print_remote (remote)));

	old_id = e_contact_get (local->contact, E_CONTACT_UID);
	ebc = g_hash_table_lookup (ctxt->changed_hash, old_id);
	
	new_contact = ecard_from_remote_record (ctxt, remote, local->contact);
	g_object_unref (local->contact);
	local->contact = new_contact;

	if (ebc && ebc->change_type == E_BOOK_CHANGE_CARD_DELETED) {
		if (!e_book_add_contact (ctxt->ebook, local->contact, NULL)) {
			WARN (G_STRLOC ": failed to add card\n");

			return -1;
		}
		
	} else {
		if (!e_book_commit_contact (ctxt->ebook, local->contact, NULL)) {
			WARN (G_STRLOC ": failed to commit card\n");

			return -1;
		}		
	}

	/* Adding a record causes wombat to assign a new uid so we must tidy */
	if (ebc && ebc->change_type == E_BOOK_CHANGE_CARD_DELETED) {
		const char *uid = e_contact_get_const (local->contact, E_CONTACT_UID);
		gboolean arch;
		
		arch = e_pilot_map_uid_is_archived (ctxt->map, uid);
		e_pilot_map_insert (ctxt->map, remote->ID, uid, arch);

		ebc = g_hash_table_lookup (ctxt->changed_hash, old_id);
		if (ebc) {
			g_hash_table_remove (ctxt->changed_hash, old_id);
			g_object_unref (ebc->contact);
			g_object_ref (local->contact);
			ebc->contact = local->contact;
			/* FIXME We should possibly be duplicating the uid */
			g_hash_table_insert (ctxt->changed_hash, (gpointer) uid, ebc);
		}
	}

	return retval;
}

static gint
delete_record (GnomePilotConduitSyncAbs *conduit,
	       EAddrLocalRecord *local,
	       EAddrConduitContext *ctxt)
{
	int retval = 0;
	
	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (local->contact != NULL, -1);

	LOG (g_message ( "delete_record: delete %s\n", print_local (local) ));

	e_pilot_map_remove_by_uid (ctxt->map, e_contact_get_const (local->contact, E_CONTACT_UID));
	if (!e_book_remove_contact (ctxt->ebook, e_contact_get_const (local->contact, E_CONTACT_UID), NULL)) {
		WARN ("delete_record: failed to delete card in ebook\n");

		retval = -1;
	}
	
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

	e_pilot_map_insert (ctxt->map, local->local.ID, e_contact_get_const (local->contact, E_CONTACT_UID), archive);
	
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
