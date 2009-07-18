/*
 * Evolution calendar - Memo Conduit
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Eskil Heyn Olsen <deity@eskil.dk>
 *      JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define G_LOG_DOMAIN "ememoconduit"

#include <glib/gi18n.h>
#include <libecal/e-cal-types.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-categories.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-memo.h>
#include <libical/icaltypes.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-sync-abs.h>
#include <libgpilotdCM/gnome-pilot-conduit-management.h>
#include <libgpilotdCM/gnome-pilot-conduit-config.h>
#include <e-pilot-map.h>
#include <e-pilot-settings.h>
#include <e-pilot-util.h>
#include <libecalendar-common-conduit.h>

GnomePilotConduit * conduit_get_gpilot_conduit (guint32);
void conduit_destroy_gpilot_conduit (GnomePilotConduit*);

#define CONDUIT_VERSION "0.1.6"

#define DEBUG_MEMOCONDUIT 1
/* #undef DEBUG_MEMOCONDUIT */

#ifdef DEBUG_MEMOCONDUIT
#define LOG(x) x
#else
#define LOG(x)
#endif

#define WARN g_warning
#define INFO g_message

typedef struct _EMemoLocalRecord EMemoLocalRecord;
typedef struct _EMemoConduitCfg EMemoConduitCfg;
typedef struct _EMemoConduitGui EMemoConduitGui;
typedef struct _EMemoConduitContext EMemoConduitContext;

/* Local Record */
struct _EMemoLocalRecord {
	/* The stuff from gnome-pilot-conduit-standard-abs.h
	   Must be first in the structure, or instances of this
	   structure cannot be used by gnome-pilot-conduit-standard-abs.
	*/
	GnomePilotDesktopRecord local;

	/* The corresponding Comp object */
	ECalComponent *comp;

        /* pilot-link memo structure */
	struct Memo *memo;
};

gint lastDesktopUniqueID;

static void
memoconduit_destroy_record (EMemoLocalRecord *local)
{
	g_object_unref (local->comp);
	free_Memo (local->memo);
	g_free (local->memo);
	g_free (local);
}

/* Configuration */
struct _EMemoConduitCfg {
	guint32 pilot_id;
	GnomePilotConduitSyncType  sync_type;

	ESourceList *source_list;
	ESource *source;
	gboolean secret;
	gint priority;

	gchar *last_uri;
};

static EMemoConduitCfg *
memoconduit_load_configuration (guint32 pilot_id)
{
	EMemoConduitCfg *c;
	GnomePilotConduitManagement *management;
	GnomePilotConduitConfig *config;
	gchar prefix[256];

	g_snprintf (prefix, 255, "e-memo-conduit/Pilot_%u", pilot_id);

	c = g_new0 (EMemoConduitCfg,1);
	g_assert (c != NULL);

	c->pilot_id = pilot_id;

	management = gnome_pilot_conduit_management_new ((gchar *)"e_memo_conduit", GNOME_PILOT_CONDUIT_MGMT_ID);
	g_object_ref_sink (management);
	config = gnome_pilot_conduit_config_new (management, pilot_id);
	g_object_ref_sink (config);
	if (!gnome_pilot_conduit_config_is_enabled (config, &c->sync_type))
		c->sync_type = GnomePilotConduitSyncTypeNotSet;
	g_object_unref (config);
	g_object_unref (management);

	/* Custom settings */
	if (!e_cal_get_sources (&c->source_list, E_CAL_SOURCE_TYPE_JOURNAL, NULL))
		c->source_list = NULL;
	if (c->source_list) {
		c->source = e_pilot_get_sync_source (c->source_list);
		if (!c->source)
			c->source = e_source_list_peek_source_any (c->source_list);
		if (c->source) {
			g_object_ref (c->source);
		} else {
			g_object_unref (c->source_list);
			c->source_list = NULL;
		}
	}

	c->secret = e_pilot_setup_get_bool (prefix, "secret", FALSE);
	c->priority = e_pilot_setup_get_int (prefix, "priority", 3);
	c->last_uri = e_pilot_setup_get_string (prefix, "last_uri", NULL);

	return c;
}

static void
memoconduit_save_configuration (EMemoConduitCfg *c)
{
	gchar prefix[256];

	g_snprintf (prefix, 255, "e-memo-conduit/Pilot_%u", c->pilot_id);

	e_pilot_set_sync_source (c->source_list, c->source);
	e_pilot_setup_set_bool (prefix, "secret", c->secret);
	e_pilot_setup_set_int (prefix, "priority", c->priority);
	e_pilot_setup_set_string (prefix, "last_uri", c->last_uri ? c->last_uri : "");
}

static EMemoConduitCfg*
memoconduit_dupe_configuration (EMemoConduitCfg *c)
{
	EMemoConduitCfg *retval;

	g_return_val_if_fail (c != NULL, NULL);

	retval = g_new0 (EMemoConduitCfg, 1);
	retval->sync_type = c->sync_type;
	retval->pilot_id = c->pilot_id;

	if (c->source_list)
		retval->source_list = g_object_ref (c->source_list);
	if (c->source)
		retval->source = g_object_ref (c->source);
	retval->secret = c->secret;
	retval->priority = c->priority;
	retval->last_uri = g_strdup (c->last_uri);

	return retval;
}

static void
memoconduit_destroy_configuration (EMemoConduitCfg *c)
{
	g_return_if_fail (c != NULL);

	g_object_unref (c->source_list);
	g_object_unref (c->source);
	g_free (c->last_uri);
	g_free (c);
}

/* Context */
struct _EMemoConduitContext {
	GnomePilotDBInfo *dbi;

	EMemoConduitCfg *cfg;
	EMemoConduitCfg *new_cfg;
	GtkWidget *ps;

	struct MemoAppInfo ai;

	ECal *client;

	icaltimezone *timezone;
	ECalComponent *default_comp;
	GList *comps;
	GList *changed;
	GHashTable *changed_hash;
	GList *locals;

	EPilotMap *map;
};

static EMemoConduitContext *
e_memo_context_new (guint32 pilot_id)
{
	EMemoConduitContext *ctxt = g_new0 (EMemoConduitContext, 1);

	ctxt->cfg = memoconduit_load_configuration (pilot_id);
	ctxt->new_cfg = memoconduit_dupe_configuration (ctxt->cfg);
	ctxt->ps = NULL;
	ctxt->client = NULL;
	ctxt->timezone = NULL;
	ctxt->default_comp = NULL;
	ctxt->comps = NULL;
	ctxt->changed_hash = NULL;
	ctxt->changed = NULL;
	ctxt->locals = NULL;
	ctxt->map = NULL;

	return ctxt;
}

static gboolean
e_memo_context_foreach_change (gpointer key, gpointer value, gpointer data)
{
	g_free (key);

	return TRUE;
}

static void
e_memo_context_destroy (EMemoConduitContext *ctxt)
{
	GList *l;

	g_return_if_fail (ctxt != NULL);

	if (ctxt->cfg != NULL)
		memoconduit_destroy_configuration (ctxt->cfg);
	if (ctxt->new_cfg != NULL)
		memoconduit_destroy_configuration (ctxt->new_cfg);

	if (ctxt->client != NULL)
		g_object_unref (ctxt->client);

	if (ctxt->default_comp != NULL)
		g_object_unref (ctxt->default_comp);
	if (ctxt->comps != NULL) {
		for (l = ctxt->comps; l; l = l->next)
			g_object_unref (l->data);
		g_list_free (ctxt->comps);
	}

	if (ctxt->changed_hash != NULL) {
		g_hash_table_foreach_remove (ctxt->changed_hash, e_memo_context_foreach_change, NULL);
		g_hash_table_destroy (ctxt->changed_hash);
	}

	if (ctxt->locals != NULL) {
		for (l = ctxt->locals; l != NULL; l = l->next)
			memoconduit_destroy_record (l->data);
		g_list_free (ctxt->locals);
	}

	if (ctxt->changed != NULL)
		e_cal_free_change_list (ctxt->changed);

	if (ctxt->map != NULL)
		e_pilot_map_destroy (ctxt->map);

	g_free (ctxt);
}

/* Debug routines */
static gchar *
print_local (EMemoLocalRecord *local)
{
	static gchar buff[ 64 ];

	if (local == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	if (local->memo && local->memo->text) {
		g_snprintf (buff, 64, "['%s']",
			    local->memo->text ?
			    local->memo->text : "");
		return buff;
	}

	strcpy (buff, "");
	return buff;
}

static gchar *print_remote (GnomePilotRecord *remote)
{
	static gchar buff[ 64 ];
	struct Memo memo;
#ifdef PILOT_LINK_0_12
	pi_buffer_t *buffer;
#endif

	if (remote == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}

	memset (&memo, 0, sizeof (struct Memo));
#ifdef PILOT_LINK_0_12
	buffer = pi_buffer_new(DLP_BUF_SIZE);
	if (buffer == NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}
	if (pi_buffer_append(buffer, remote->record, remote->length)==NULL) {
		sprintf (buff, "[NULL]");
		return buff;
	}
	unpack_Memo (&memo, buffer, memo_v1);

	pi_buffer_free(buffer);
#else
	unpack_Memo (&memo, remote->record, remote->length);
#endif
	g_snprintf (buff, 64, "['%s']",
		    memo.text ?
		    memo.text : "");

	free_Memo (&memo);

	return buff;
}

static gint
start_calendar_server (EMemoConduitContext *ctxt)
{
	g_return_val_if_fail (ctxt != NULL, -2);

	if (ctxt->cfg->source) {
		ctxt->client = e_cal_new (ctxt->cfg->source, E_CAL_SOURCE_TYPE_JOURNAL);
		if (!e_cal_open (ctxt->client, TRUE, NULL))
			return -1;
	} else if (!e_cal_open_default (&ctxt->client, E_CAL_SOURCE_TYPE_JOURNAL, NULL, NULL, NULL)) {
		return -1;
	}

	return 0;
}

static icaltimezone *
get_default_timezone (void)
{
	GConfClient *client;
	icaltimezone *timezone = NULL;
	const gchar *key;
	gchar *location;

	client = gconf_client_get_default ();
	key = "/apps/evolution/calendar/display/timezone";
	location = gconf_client_get_string (client, key, NULL);

	if (location == NULL || *location == '\0') {
		g_free (location);
		location = g_strdup ("UTC");
	}

	timezone = icaltimezone_get_builtin_timezone (location);
	g_free (location);

	g_object_unref (client);

	return timezone;
}

static gchar *
map_name (EMemoConduitContext *ctxt)
{
	gchar *filename;

	filename = g_strdup_printf ("%s/.evolution/memos/local/system/pilot-map-memo-%d.xml", g_get_home_dir (), ctxt->cfg->pilot_id);

	return filename;
}

static GList *
next_changed_item (EMemoConduitContext *ctxt, GList *changes)
{
	ECalChange *ccc;
	GList *l;

	for (l = changes; l != NULL; l = l->next) {
		const gchar *uid;

		ccc = l->data;

		e_cal_component_get_uid (ccc->comp, &uid);
		if (g_hash_table_lookup (ctxt->changed_hash, uid))
			return l;
	}

	return NULL;
}

static void
compute_status (EMemoConduitContext *ctxt, EMemoLocalRecord *local, const gchar *uid)
{
	ECalChange *ccc;

	local->local.archived = FALSE;
	local->local.secret = FALSE;

	ccc = g_hash_table_lookup (ctxt->changed_hash, uid);

	if (ccc == NULL) {
		local->local.attr = GnomePilotRecordNothing;
		return;
	}

	switch (ccc->type) {
	case E_CAL_CHANGE_ADDED:
		local->local.attr = GnomePilotRecordNew;
		break;
	case E_CAL_CHANGE_MODIFIED:
		local->local.attr = GnomePilotRecordModified;
		break;
	case E_CAL_CHANGE_DELETED:
		local->local.attr = GnomePilotRecordDeleted;
		break;
	}
}

static GnomePilotRecord
local_record_to_pilot_record (EMemoLocalRecord *local,
			      EMemoConduitContext *ctxt)
{
	GnomePilotRecord p;
#ifdef PILOT_LINK_0_12
	pi_buffer_t * buffer;
#else
	static gchar record[0xffff];
#endif

	memset(&p, 0, sizeof (p));

	g_assert (local->comp != NULL);
	g_assert (local->memo != NULL );

	LOG (g_message ( "local_record_to_pilot_record\n" ));

	memset (&p, 0, sizeof (GnomePilotRecord));

	p.ID = local->local.ID;
	p.category = local->local.category;
	p.attr = local->local.attr;
	p.archived = local->local.archived;
	p.secret = local->local.secret;

	/* Generate pilot record structure */
#ifdef PILOT_LINK_0_12
	buffer = pi_buffer_new(DLP_BUF_SIZE);
	if (buffer == NULL) {
		pi_set_error(ctxt->dbi->pilot_socket, PI_ERR_GENERIC_MEMORY);
		return p;
	}

	pack_Memo (local->memo, buffer, memo_v1);
	p.record = g_new0(unsigned char, buffer->used);
	p.length = buffer->used;
	memcpy(p.record, buffer->data, buffer->used);

	pi_buffer_free(buffer);
#else
	p.record = (guchar *)record;
	p.length = pack_Memo (local->memo, p.record, 0xffff);
#endif
	return p;
}

/*
 * converts a ECalComponent object to a EMemoLocalRecord
 */
static void
local_record_from_comp (EMemoLocalRecord *local, ECalComponent *comp, EMemoConduitContext *ctxt)
{
	const gchar *uid;
	GSList *d_list = NULL;
	ECalComponentText *description;
	ECalComponentClassification classif;

	LOG (g_message ( "local_record_from_comp\n" ));

	g_return_if_fail (local != NULL);
	g_return_if_fail (comp != NULL);

	local->comp = comp;
	g_object_ref (comp);

	LOG(fprintf(stderr, "local_record_from_comp: calling e_cal_component_get_uid\n"));
	e_cal_component_get_uid (local->comp, &uid);
	LOG(fprintf(stderr, "local_record_from_comp: got UID - %s, calling e_pilot_map_lookup_pid\n", uid));
	local->local.ID = e_pilot_map_lookup_pid (ctxt->map, uid, TRUE);
	LOG(fprintf(stderr, "local_record_from_comp: local->local.ID == %lu\n", local->local.ID));

	compute_status (ctxt, local, uid);

	LOG(fprintf(stderr, "local_record_from_comp: local->local.attr: %d\n", local->local.attr));

	local->memo = g_new0 (struct Memo,1);

	/* Don't overwrite the category */
	if (local->local.ID != 0) {
#ifdef PILOT_LINK_0_12
		struct Memo memo;
		pi_buffer_t * record;
#else
		gchar record[0xffff];
#endif
		gint cat = 0;

#ifdef PILOT_LINK_0_12
		record = pi_buffer_new(DLP_BUF_SIZE);
		if (record == NULL) {
			pi_set_error(ctxt->dbi->pilot_socket, PI_ERR_GENERIC_MEMORY);
			return;
		}
#endif

		LOG(fprintf(stderr, "local_record_from_comp: calling dlp_ReadRecordById\n"));
		if (dlp_ReadRecordById (ctxt->dbi->pilot_socket,
					ctxt->dbi->db_handle,
#ifdef PILOT_LINK_0_12
					local->local.ID, record,
					NULL, NULL, &cat) > 0) {
			local->local.category = cat;
			memset (&memo, 0, sizeof (struct Memo));
			unpack_Memo (&memo, record, memo_v1);
			local->memo->text = strdup (memo.text);
			free_Memo (&memo);
		}
		pi_buffer_free (record);
#else
					local->local.ID, &record,
					NULL, NULL, NULL, &cat) > 0) {
			local->local.category = cat;
		}
#endif
		LOG(fprintf(stderr, "local_record_from_comp: done calling dlp_ReadRecordById\n"));
	}

	/*Category support*/
	e_pilot_local_category_to_remote(&(local->local.category), comp, &(ctxt->ai.category));

	/* STOP: don't replace these with g_strdup, since free_Memo
	   uses free to deallocate */

	e_cal_component_get_description_list (comp, &d_list);
	if (d_list) {
		description = (ECalComponentText *) d_list->data;
		if (description && description->value) {
			local->memo->text = e_pilot_utf8_to_pchar (description->value);
		}
		else{
			local->memo->text = NULL;
		}
	} else {
		local->memo->text = NULL;
	}

	e_cal_component_get_classification (comp, &classif);

	if (classif == E_CAL_COMPONENT_CLASS_PRIVATE)
		local->local.secret = 1;
	else
		local->local.secret = 0;

	local->local.archived = 0;
}

static void
local_record_from_uid (EMemoLocalRecord *local,
		       const gchar *uid,
		       EMemoConduitContext *ctxt)
{
	ECalComponent *comp;
	icalcomponent *icalcomp;
	GError *error = NULL;

	g_assert(local!=NULL);

	LOG(g_message("local_record_from_uid\n"));

	if (e_cal_get_object (ctxt->client, uid, NULL, &icalcomp, &error)) {
		comp = e_cal_component_new ();
		if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
			g_object_unref (comp);
			icalcomponent_free (icalcomp);
			return;
		}

		local_record_from_comp (local, comp, ctxt);
		g_object_unref (comp);
	} else if (error->code == E_CALENDAR_STATUS_OBJECT_NOT_FOUND) {
		comp = e_cal_component_new ();
		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_JOURNAL);
		e_cal_component_set_uid (comp, uid);
		local_record_from_comp (local, comp, ctxt);
		g_object_unref (comp);
	} else {
		INFO ("Object did not exist");
	}

	g_clear_error (&error);
}

static ECalComponent *
comp_from_remote_record (GnomePilotConduitSyncAbs *conduit,
			 GnomePilotRecord *remote,
			 ECalComponent *in_comp,
			 icaltimezone *timezone,
			 struct MemoAppInfo *ai)
{
	ECalComponent *comp;
	struct Memo memo;
	struct icaltimetype now;
	icaltimezone *utc_zone;
	gchar *txt, *txt2, *txt3;
	gint i;
#ifdef PILOT_LINK_0_12
	pi_buffer_t * buffer;
#endif
	g_return_val_if_fail (remote != NULL, NULL);

#ifdef PILOT_LINK_0_12
	buffer = pi_buffer_new(DLP_BUF_SIZE);
	if (buffer == NULL) {
		return NULL;
	}

	if (pi_buffer_append(buffer, remote->record, remote->length)==NULL) {
		return NULL;
	}

	unpack_Memo (&memo, buffer, memo_v1);
	pi_buffer_free(buffer);
#else
	memset (&memo, 0, sizeof (struct Memo));
	unpack_Memo (&memo, remote->record, remote->length);
#endif

	utc_zone = icaltimezone_get_utc_timezone ();
	now = icaltime_from_timet_with_zone (time (NULL), FALSE,
					     utc_zone);

	if (in_comp == NULL) {
		comp = e_cal_component_new ();
		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_JOURNAL);
		e_cal_component_set_created (comp, &now);
	} else {
		comp = e_cal_component_clone (in_comp);
	}

	e_cal_component_set_last_modified (comp, &now);

	/*Category support*/
	e_pilot_remote_category_to_local(remote->category, comp, &(ai->category));

	/* The iCal description field */
	if (!memo.text) {
		e_cal_component_set_comment_list (comp, NULL);
		e_cal_component_set_summary(comp, NULL);
	} else {
		gint idxToUse = -1, ntext = strlen(memo.text);
		gboolean foundNL = FALSE;
		GSList l;
		ECalComponentText text, sumText;

		for (i = 0; i<ntext && i<50; i++) {
			if (memo.text[i] == '\n') {
				idxToUse = i;
				foundNL = TRUE;
				break;
			}
		}

		if (foundNL == FALSE) {
			if (ntext > 50) {
				txt2 = g_strndup(memo.text, 50);
			}
			else{
				txt2 = g_strdup(memo.text);

			}
		}
		else{
			txt2 = g_strndup(memo.text, idxToUse); /* cuts off '\n' */

		}

		sumText.value = txt3 = e_pilot_utf8_from_pchar(txt2);
		sumText.altrep = NULL;

		text.value = txt = e_pilot_utf8_from_pchar (memo.text);
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		e_cal_component_set_summary(comp, &sumText);
		e_cal_component_set_description_list (comp, &l);
		free (txt);
		g_free(txt2);
		free(txt3);
	}

	e_cal_component_set_transparency (comp, E_CAL_COMPONENT_TRANSP_NONE);

	if (remote->secret)
		e_cal_component_set_classification (comp, E_CAL_COMPONENT_CLASS_PRIVATE);
	else
		e_cal_component_set_classification (comp, E_CAL_COMPONENT_CLASS_PUBLIC);

	e_cal_component_commit_sequence (comp);

	free_Memo(&memo);

	return comp;
}

static void
check_for_slow_setting (GnomePilotConduit *c, EMemoConduitContext *ctxt)
{
	GnomePilotConduitStandard *conduit = GNOME_PILOT_CONDUIT_STANDARD (c);
	gint map_count;
	const gchar *uri;

	/* If there are no objects or objects but no log */
	map_count = g_hash_table_size (ctxt->map->pid_map);
	if (map_count == 0)
		gnome_pilot_conduit_standard_set_slow (conduit, TRUE);

	/* Or if the URI's don't match */
	uri = e_cal_get_uri (ctxt->client);
	LOG (g_message ( "  Current URI %s (%s)\n", uri, ctxt->cfg->last_uri ? ctxt->cfg->last_uri : "<NONE>" ));
	if (ctxt->cfg->last_uri != NULL && (strcmp (ctxt->cfg->last_uri, uri) != 0)) {
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
	  EMemoConduitContext *ctxt)
{
	GnomePilotConduitSyncAbs *abs_conduit;
	GList *l;
	gint len;
	guchar *buf;
	gchar *filename, *change_id;
	icalcomponent *icalcomp;
	gint num_records, add_records = 0, mod_records = 0, del_records = 0;
#ifdef PILOT_LINK_0_12
	pi_buffer_t * buffer;
#endif

	abs_conduit = GNOME_PILOT_CONDUIT_SYNC_ABS (conduit);

	LOG (g_message ( "---------------------------------------------------------\n" ));
	LOG (g_message ( "pre_sync: Memo Conduit v.%s", CONDUIT_VERSION ));
	g_message ("Memo Conduit v.%s", CONDUIT_VERSION);

	ctxt->dbi = dbi;
	ctxt->client = NULL;

	if (start_calendar_server (ctxt) != 0) {
		WARN(_("Could not start evolution-data-server"));
		gnome_pilot_conduit_error (conduit, _("Could not start evolution-data-server"));
		return -1;
	}

	/* Get the timezone */
	ctxt->timezone = get_default_timezone ();
	if (ctxt->timezone == NULL)
		return -1;
	LOG (g_message ( "  Using timezone: %s", icaltimezone_get_tzid (ctxt->timezone) ));

	/* Set the default timezone on the backend. */
	if (ctxt->timezone && !e_cal_set_default_timezone (ctxt->client, ctxt->timezone, NULL))
		return -1;

	/* Get the default component */
	if (!e_cal_get_default_object (ctxt->client, &icalcomp, NULL))
		return -1;

	ctxt->default_comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (ctxt->default_comp, icalcomp)) {
		g_object_unref (ctxt->default_comp);
		icalcomponent_free (icalcomp);
		return -1;
	}

	/* Load the uid <--> pilot id map */
	filename = map_name (ctxt);
	e_pilot_map_read (filename, &ctxt->map);
	g_free (filename);

	/* Get the local database */
	if (!e_cal_get_object_list_as_comp (ctxt->client, "#t", &ctxt->comps, NULL))
		return -1;

	/* Count and hash the changes */
	change_id = g_strdup_printf ("pilot-sync-evolution-memo-%d", ctxt->cfg->pilot_id);
	if (!e_cal_get_changes (ctxt->client, change_id, &ctxt->changed, NULL))
		return -1;

	ctxt->changed_hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_free (change_id);

	for (l = ctxt->changed; l != NULL; l = l->next) {
		ECalChange *ccc = l->data;
		const gchar *uid;

		e_cal_component_get_uid (ccc->comp, &uid);
		if (!e_pilot_map_uid_is_archived (ctxt->map, uid)) {

			g_hash_table_insert (ctxt->changed_hash, g_strdup (uid), ccc);

			switch (ccc->type) {
			case E_CAL_CHANGE_ADDED:
				add_records++;
				break;
			case E_CAL_CHANGE_MODIFIED:
				mod_records++;
				break;
			case E_CAL_CHANGE_DELETED:
				del_records++;
				break;
			}
		} else if (ccc->type == E_CAL_CHANGE_DELETED) {
			e_pilot_map_remove_by_uid (ctxt->map, uid);
		}
	}

	/* Set the count information */
	num_records = g_list_length (ctxt->comps);
	gnome_pilot_conduit_sync_abs_set_num_local_records(abs_conduit, num_records);
	gnome_pilot_conduit_sync_abs_set_num_new_local_records (abs_conduit, add_records);
	gnome_pilot_conduit_sync_abs_set_num_updated_local_records (abs_conduit, mod_records);
	gnome_pilot_conduit_sync_abs_set_num_deleted_local_records(abs_conduit, del_records);

	g_message("num_records: %d\nadd_records: %d\nmod_records: %d\ndel_records: %d\n",
		num_records, add_records, mod_records, del_records);

#ifdef PILOT_LINK_0_12
	buffer = pi_buffer_new(DLP_BUF_SIZE);
	if (buffer == NULL) {
		pi_set_error(dbi->pilot_socket, PI_ERR_GENERIC_MEMORY);
		return -1;
	}

	len = dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,
				DLP_BUF_SIZE,
				buffer);
#else
	buf = (guchar *)g_malloc (0xffff);
	len = dlp_ReadAppBlock (dbi->pilot_socket, dbi->db_handle, 0,
			      (guchar *)buf, 0xffff);
#endif
	if (len < 0) {
		WARN (_("Could not read pilot's Memo application block"));
		WARN ("dlp_ReadAppBlock(...) = %d", len);
		gnome_pilot_conduit_error (conduit,
					   _("Could not read pilot's Memo application block"));
		return -1;
	}
#ifdef PILOT_LINK_0_12
	buf = g_new0 (unsigned char,buffer->used);
	memcpy(buf, buffer->data, buffer->used);
	unpack_MemoAppInfo (&(ctxt->ai), buf, len);
	pi_buffer_free(buffer);
#else
	unpack_MemoAppInfo (&(ctxt->ai), buf, len);
#endif

	g_free (buf);

	lastDesktopUniqueID = 128;

	check_for_slow_setting (conduit, ctxt);
	if (ctxt->cfg->sync_type == GnomePilotConduitSyncTypeCopyToPilot
	    || ctxt->cfg->sync_type == GnomePilotConduitSyncTypeCopyFromPilot)
		ctxt->map->write_touched_only = TRUE;

	return 0;
}

static gint
post_sync (GnomePilotConduit *conduit,
	   GnomePilotDBInfo *dbi,
	   EMemoConduitContext *ctxt)
{
	GList *changed;
	gchar *filename, *change_id;
	guchar *buf;
	gint dlpRetVal, len;

	buf = (guchar *)g_malloc (0xffff);

	len = pack_MemoAppInfo (&(ctxt->ai), buf, 0xffff);

	dlpRetVal = dlp_WriteAppBlock (dbi->pilot_socket, dbi->db_handle,
			      (guchar *)buf, len);

	g_free (buf);

	if (dlpRetVal < 0) {
		WARN (_("Could not write pilot's Memo application block"));
		WARN ("dlp_WriteAppBlock(...) = %d", dlpRetVal);
		gnome_pilot_conduit_error (conduit,
					   _("Could not write pilot's Memo application block"));
		return -1;
	}

	LOG (g_message ( "post_sync: Memo Conduit v.%s", CONDUIT_VERSION ));

	g_free (ctxt->cfg->last_uri);
	ctxt->cfg->last_uri = g_strdup (e_cal_get_uri (ctxt->client));
	memoconduit_save_configuration (ctxt->cfg);

	filename = map_name (ctxt);
	e_pilot_map_write (filename, ctxt->map);
	g_free (filename);

	/* FIX ME ugly hack - our changes musn't count, this does introduce
	 * a race condition if anyone changes a record elsewhere during sycnc
         */
	change_id = g_strdup_printf ("pilot-sync-evolution-memo-%d", ctxt->cfg->pilot_id);
	if (e_cal_get_changes (ctxt->client, change_id, &changed, NULL))
		e_cal_free_change_list (changed);
	g_free (change_id);

	LOG (g_message ( "---------------------------------------------------------\n" ));

	return 0;
}

static gint
set_pilot_id (GnomePilotConduitSyncAbs *conduit,
	      EMemoLocalRecord *local,
	      guint32 ID,
	      EMemoConduitContext *ctxt)
{
	const gchar *uid;

	LOG (g_message ( "set_pilot_id: setting to %d\n", ID ));

	e_cal_component_get_uid (local->comp, &uid);
	e_pilot_map_insert (ctxt->map, ID, uid, FALSE);

        return 0;
}

static gint
set_status_cleared (GnomePilotConduitSyncAbs *conduit,
		    EMemoLocalRecord *local,
		    EMemoConduitContext *ctxt)
{
	const gchar *uid;

	LOG (g_message ( "set_status_cleared: clearing status\n" ));

	e_cal_component_get_uid (local->comp, &uid);
	g_hash_table_remove (ctxt->changed_hash, uid);

        return 0;
}

static gint
for_each (GnomePilotConduitSyncAbs *conduit,
	  EMemoLocalRecord **local,
	  EMemoConduitContext *ctxt)
{
	static GList *comps, *iterator;
	static gint count;
        GList *unused;

	g_return_val_if_fail (local != NULL, -1);

	if (*local == NULL) {
		LOG (g_message ( "beginning for_each" ));

		comps = ctxt->comps;
		count = 0;

		if (comps != NULL) {
			LOG (g_message ( "for_each: iterating over %d records", g_list_length (comps)));

			*local = g_new0 (EMemoLocalRecord, 1);
			local_record_from_comp (*local, comps->data, ctxt);

			/* NOTE: ignore the return value, otherwise ctxt->locals
			 * gets messed up. The calling function keeps track of
			 * the *local variable */
			unused = g_list_prepend (ctxt->locals, *local);
			iterator = comps;
		} else {
			LOG (g_message ( "no events" ));
			(*local) = NULL;
			return 0;
		}
	} else {
		count++;

		if (g_list_next (iterator)) {
			iterator = g_list_next (iterator);

			*local = g_new0 (EMemoLocalRecord, 1);
			LOG(fprintf(stderr, "for_each: calling local_record_from_comp\n"));
			local_record_from_comp (*local, iterator->data, ctxt);

			/* NOTE: ignore the return value, otherwise ctxt->locals
			 * gets messed up. The calling function keeps track of
			 * the *local variable */
			unused = g_list_prepend (ctxt->locals, *local);
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
		   EMemoLocalRecord **local,
		   EMemoConduitContext *ctxt)
{
	static GList *iterator;
	static gint count;
        GList *unused;

	g_return_val_if_fail (local != NULL, 0);

	if (*local == NULL) {
		LOG (g_message ( "for_each_modified beginning\n" ));

		iterator = ctxt->changed;

		count = 0;

		LOG (g_message ( "iterating over %d records", g_hash_table_size (ctxt->changed_hash) ));

		iterator = next_changed_item (ctxt, iterator);
		if (iterator != NULL) {
			ECalChange *ccc = iterator->data;

			*local = g_new0 (EMemoLocalRecord, 1);
			local_record_from_comp (*local, ccc->comp, ctxt);

			/* NOTE: ignore the return value, otherwise ctxt->locals
			 * gets messed up. The calling function keeps track of
			 * the *local variable */
			unused = g_list_prepend (ctxt->locals, *local);
		} else {
			LOG (g_message ( "no events" ));

			*local = NULL;
		}
	} else {
		count++;

		iterator = g_list_next (iterator);
		if (iterator && (iterator = next_changed_item (ctxt, iterator))) {
			ECalChange *ccc = iterator->data;

			*local = g_new0 (EMemoLocalRecord, 1);
			local_record_from_comp (*local, ccc->comp, ctxt);

			/* NOTE: ignore the return value, otherwise ctxt->locals
			 * gets messed up. The calling function keeps track of
			 * the *local variable */
			unused = g_list_prepend (ctxt->locals, *local);
		} else {
			LOG (g_message ( "for_each_modified ending" ));

			/* Signal the iteration is over */
			*local = NULL;
		}
	}

	return 0;
}

static gint
compare (GnomePilotConduitSyncAbs *conduit,
	 EMemoLocalRecord *local,
	 GnomePilotRecord *remote,
	 EMemoConduitContext *ctxt)
{
	/* used by the quick compare */
	GnomePilotRecord local_pilot;
	gint retval = 0;

	LOG (g_message ("compare: local=%s remote=%s...\n",
			print_local (local), print_remote (remote)));

	g_return_val_if_fail (local!=NULL,-1);
	g_return_val_if_fail (remote!=NULL,-1);

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
	    EMemoConduitContext *ctxt)
{
	ECalComponent *comp;
	gchar *uid;
	gint retval = 0;

	g_return_val_if_fail (remote != NULL, -1);

	LOG (g_message ( "add_record: adding %s to desktop\n", print_remote (remote) ));

	comp = comp_from_remote_record (conduit, remote, ctxt->default_comp, ctxt->timezone, &(ctxt->ai));

	/* Give it a new UID otherwise it will be the uid of the default comp */
	uid = e_cal_component_gen_uid ();
	e_cal_component_set_uid (comp, uid);

	if (!e_cal_create_object (ctxt->client, e_cal_component_get_icalcomponent (comp), NULL, NULL))
		return -1;

	e_pilot_map_insert (ctxt->map, remote->ID, uid, FALSE);

	g_object_unref (comp);

	return retval;
}

static gint
replace_record (GnomePilotConduitSyncAbs *conduit,
		EMemoLocalRecord *local,
		GnomePilotRecord *remote,
		EMemoConduitContext *ctxt)
{
	ECalComponent *new_comp;
	gint retval = 0;

	g_return_val_if_fail (remote != NULL, -1);

	LOG (g_message ("replace_record: replace %s with %s\n",
			print_local (local), print_remote (remote)));

	new_comp = comp_from_remote_record (conduit, remote, local->comp, ctxt->timezone, &(ctxt->ai));
	g_object_unref (local->comp);
	local->comp = new_comp;

	if (!e_cal_modify_object (ctxt->client, e_cal_component_get_icalcomponent (new_comp),
				       CALOBJ_MOD_ALL, NULL))
		return -1;

	return retval;
}

static gint
delete_record (GnomePilotConduitSyncAbs *conduit,
	       EMemoLocalRecord *local,
	       EMemoConduitContext *ctxt)
{
	const gchar *uid;

	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (local->comp != NULL, -1);

	e_cal_component_get_uid (local->comp, &uid);

	LOG (g_message ( "delete_record: deleting %s", uid ));

	e_pilot_map_remove_by_uid (ctxt->map, uid);
	/* FIXME Error handling */
	e_cal_remove_object (ctxt->client, uid, NULL);

        return 0;
}

static gint
archive_record (GnomePilotConduitSyncAbs *conduit,
		EMemoLocalRecord *local,
		gboolean archive,
		EMemoConduitContext *ctxt)
{
	const gchar *uid;
	gint retval = 0;

	g_return_val_if_fail (local != NULL, -1);

	LOG (g_message ( "archive_record: %s\n", archive ? "yes" : "no" ));

	e_cal_component_get_uid (local->comp, &uid);
	e_pilot_map_insert (ctxt->map, local->local.ID, uid, archive);

        return retval;
}

static gint
match (GnomePilotConduitSyncAbs *conduit,
       GnomePilotRecord *remote,
       EMemoLocalRecord **local,
       EMemoConduitContext *ctxt)
{
	const gchar *uid;

	LOG (g_message ("match: looking for local copy of %s\n",
			print_remote (remote)));

	g_return_val_if_fail (local != NULL, -1);
	g_return_val_if_fail (remote != NULL, -1);

	*local = NULL;
	uid = e_pilot_map_lookup_uid (ctxt->map, remote->ID, TRUE);

	if (!uid)
		return 0;

	LOG (g_message ( "  matched\n" ));

	*local = g_new0 (EMemoLocalRecord, 1);
	local_record_from_uid (*local, uid, ctxt);

	return 0;
}

static gint
free_match (GnomePilotConduitSyncAbs *conduit,
	    EMemoLocalRecord *local,
	    EMemoConduitContext *ctxt)
{
	LOG (g_message ( "free_match: freeing\n" ));

	g_return_val_if_fail (local != NULL, -1);

	ctxt->locals = g_list_remove (ctxt->locals, local);

	memoconduit_destroy_record (local);

	return 0;
}

static gint
prepare (GnomePilotConduitSyncAbs *conduit,
	 EMemoLocalRecord *local,
	 GnomePilotRecord *remote,
	 EMemoConduitContext *ctxt)
{
	LOG (g_message ( "prepare: encoding local %s\n", print_local (local) ));

	*remote = local_record_to_pilot_record (local, ctxt);

	return 0;
}

/* Pilot Settings Callbacks */
static void
fill_widgets (EMemoConduitContext *ctxt)
{
	if (ctxt->cfg->source)
		e_pilot_settings_set_source (E_PILOT_SETTINGS (ctxt->ps),
					     ctxt->cfg->source);
	e_pilot_settings_set_secret (E_PILOT_SETTINGS (ctxt->ps),
				     ctxt->cfg->secret);
}

static gint
create_settings_window (GnomePilotConduit *conduit,
			GtkWidget *parent,
			EMemoConduitContext *ctxt)
{
	LOG (g_message ( "create_settings_window" ));

	if (!ctxt->cfg->source_list)
		return -1;

	ctxt->ps = e_pilot_settings_new (ctxt->cfg->source_list);

	gtk_container_add (GTK_CONTAINER (parent), ctxt->ps);
	gtk_widget_show (ctxt->ps);

	fill_widgets (ctxt);

	return 0;
}

static void
display_settings (GnomePilotConduit *conduit, EMemoConduitContext *ctxt)
{
	LOG (g_message ( "display_settings" ));

	fill_widgets (ctxt);
}

static void
save_settings    (GnomePilotConduit *conduit, EMemoConduitContext *ctxt)
{
	LOG (g_message ( "save_settings" ));

	if (ctxt->new_cfg->source)
		g_object_unref (ctxt->new_cfg->source);
	ctxt->new_cfg->source = e_pilot_settings_get_source (E_PILOT_SETTINGS (ctxt->ps));
	g_object_ref (ctxt->new_cfg->source);
	ctxt->new_cfg->secret = e_pilot_settings_get_secret (E_PILOT_SETTINGS (ctxt->ps));

	memoconduit_save_configuration (ctxt->new_cfg);
}

static void
revert_settings  (GnomePilotConduit *conduit, EMemoConduitContext *ctxt)
{
	LOG (g_message ( "revert_settings" ));

	memoconduit_save_configuration (ctxt->cfg);
	memoconduit_destroy_configuration (ctxt->new_cfg);
	ctxt->new_cfg = memoconduit_dupe_configuration (ctxt->cfg);
}

GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilot_id)
{
	GtkObject *retval;
	EMemoConduitContext *ctxt;

	LOG (g_message ( "in memo's conduit_get_gpilot_conduit\n" ));

	retval = gnome_pilot_conduit_sync_abs_new ((gchar *)"MemoDB", 0x6D656D6F);
	g_assert (retval != NULL);

	ctxt = e_memo_context_new (pilot_id);
	g_object_set_data (G_OBJECT (retval), "memoconduit_context", ctxt);

	g_signal_connect (retval, "pre_sync", G_CALLBACK (pre_sync), ctxt);
	g_signal_connect (retval, "post_sync", G_CALLBACK (post_sync), ctxt);

	g_signal_connect (retval, "set_pilot_id", G_CALLBACK (set_pilot_id), ctxt);
	g_signal_connect (retval, "set_status_cleared", G_CALLBACK (set_status_cleared), ctxt);

	g_signal_connect (retval, "for_each", G_CALLBACK (for_each), ctxt);
	g_signal_connect (retval, "for_each_modified", G_CALLBACK (for_each_modified), ctxt);
	g_signal_connect (retval, "compare", G_CALLBACK (compare), ctxt);

	g_signal_connect (retval, "add_record", G_CALLBACK (add_record), ctxt);
	g_signal_connect (retval, "replace_record", G_CALLBACK (replace_record), ctxt);
	g_signal_connect (retval, "delete_record", G_CALLBACK (delete_record), ctxt);
	g_signal_connect (retval, "archive_record", G_CALLBACK (archive_record), ctxt);

	g_signal_connect (retval, "match", G_CALLBACK (match), ctxt);
	g_signal_connect (retval, "free_match", G_CALLBACK (free_match), ctxt);

	g_signal_connect (retval, "prepare", G_CALLBACK (prepare), ctxt);

	/* Gui Settings */
	g_signal_connect (retval, "create_settings_window", G_CALLBACK (create_settings_window), ctxt);
	g_signal_connect (retval, "display_settings", G_CALLBACK (display_settings), ctxt);
	g_signal_connect (retval, "save_settings", G_CALLBACK (save_settings), ctxt);
	g_signal_connect (retval, "revert_settings", G_CALLBACK (revert_settings), ctxt);

	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{
	GtkObject *obj = GTK_OBJECT (conduit);
	EMemoConduitContext *ctxt;

	ctxt = g_object_get_data (G_OBJECT (obj), "memoconduit_context");
	e_memo_context_destroy (ctxt);

	gtk_object_destroy (obj);
}
