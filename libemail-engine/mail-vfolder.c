/*
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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>

#include "libevolution-utils/e-alert-dialog.h"

#include "libemail-utils/mail-mt.h"
#include "libemail-engine/mail-folder-cache.h"
#include "libemail-engine/e-mail-folder-utils.h"
#include "libemail-engine/e-mail-session.h"
#include "libemail-engine/e-mail-utils.h"
#include "libemail-engine/mail-ops.h"
#include "libemail-engine/mail-tools.h"

#include <libemail-utils/em-vfolder-context.h>
#include <libemail-utils/em-vfolder-rule.h>
#include "mail-vfolder.h"

#define d(x)  /* (printf("%s:%s: ",  G_STRLOC, G_STRFUNC), (x))*/

/* Note: Once we completely move mail to EDS, this context wont be available for UI. 
 * and vfoldertypes.xml should be moved here really. */
EMVFolderContext *context;	/* context remains open all time */

/* lock for accessing shared resources (below) */
G_LOCK_DEFINE_STATIC (vfolder);

static GHashTable *vfolder_hash;
/* This is a slightly hacky solution to shutting down, we poll this variable in various
 * loops, and just quit processing if it is set. */
static volatile gint vfolder_shutdown;	/* are we shutting down? */

static void rule_changed (EFilterRule *rule, CamelFolder *folder);

/* ********************************************************************** */

struct _setup_msg {
	MailMsg base;

	EMailSession *session;
	CamelFolder *folder;
	gchar *query;
	GList *sources_uri;
	GList *sources_folder;
};

static gchar *
vfolder_setup_desc (struct _setup_msg *m)
{
	return g_strdup_printf (
		_("Setting up Search Folder: %s"),
		camel_folder_get_full_name (m->folder));
}

static void
vfolder_setup_exec (struct _setup_msg *m,
                    GCancellable *cancellable,
                    GError **error)
{
	GList *l, *list = NULL;
	CamelFolder *folder;

	camel_vee_folder_set_expression ((CamelVeeFolder *) m->folder, m->query);

	l = m->sources_uri;
	while (l && !vfolder_shutdown) {
		d(printf(" Adding uri: %s\n", (gchar *)l->data));

		/* FIXME Not passing a GCancellable or GError here. */
		folder = e_mail_session_uri_to_folder_sync (
			m->session, l->data, 0, NULL, NULL);
		if (folder != NULL)
			list = g_list_append (list, folder);
		l = l->next;
	}

	l = m->sources_folder;
	while (l && !vfolder_shutdown) {
		g_object_ref (l->data);
		list = g_list_append (list, l->data);
		l = l->next;
	}

	if (!vfolder_shutdown)
		camel_vee_folder_set_folders ((CamelVeeFolder *) m->folder, list, cancellable);

	l = list;
	while (l) {
		g_object_unref (l->data);
		l = l->next;
	}
	g_list_free (list);
}

static void
vfolder_setup_done (struct _setup_msg *m)
{
}

static void
vfolder_setup_free (struct _setup_msg *m)
{
	GList *l;

	camel_folder_thaw (m->folder);

	g_object_unref (m->session);
	g_object_unref (m->folder);
	g_free (m->query);

	l = m->sources_uri;
	while (l) {
		g_free (l->data);
		l = l->next;
	}
	g_list_free (m->sources_uri);

	l = m->sources_folder;
	while (l) {
		g_object_unref (l->data);
		l = l->next;
	}
	g_list_free (m->sources_folder);
}

static MailMsgInfo vfolder_setup_info = {
	sizeof (struct _setup_msg),
	(MailMsgDescFunc) vfolder_setup_desc,
	(MailMsgExecFunc) vfolder_setup_exec,
	(MailMsgDoneFunc) vfolder_setup_done,
	(MailMsgFreeFunc) vfolder_setup_free
};

/* sources_uri should be camel uri's */
static gint
vfolder_setup (EMailSession *session,
               CamelFolder *folder,
               const gchar *query,
               GList *sources_uri,
               GList *sources_folder)
{
	struct _setup_msg *m;
	gint id;

	m = mail_msg_new (&vfolder_setup_info);
	m->session = g_object_ref (session);
	m->folder = g_object_ref (folder);
	m->query = g_strdup (query);
	m->sources_uri = sources_uri;
	m->sources_folder = sources_folder;

	camel_folder_freeze (m->folder);

	id = m->base.seq;
	mail_msg_slow_ordered_push (m);

	return id;
}

/* ********************************************************************** */

struct _adduri_msg {
	MailMsg base;

	EMailSession *session;
	gchar *uri;
	GList *folders;
	gint remove;
};

static gchar *
vfolder_adduri_desc (struct _adduri_msg *m)
{
	CamelStore *store;
	CamelService *service;
	const gchar *display_name;
	gchar *folder_name;
	gchar *description;
	gboolean success;

	success = e_mail_folder_uri_parse (
		CAMEL_SESSION (m->session), m->uri,
		&store, &folder_name, NULL);

	if (!success)
		return NULL;

	service = CAMEL_SERVICE (store);
	display_name = camel_service_get_display_name (service);

	description = g_strdup_printf (
		_("Updating Search Folders for '%s' - %s"),
		display_name, folder_name);

	g_object_unref (store);
	g_free (folder_name);

	return description;
}

static void
vfolder_adduri_exec (struct _adduri_msg *m,
                     GCancellable *cancellable,
                     GError **error)
{
	GList *l;
	CamelFolder *folder = NULL;
	MailFolderCache *folder_cache;

	if (vfolder_shutdown)
		return;

	folder_cache = e_mail_session_get_folder_cache (m->session);

	/* we dont try lookup the cache if we are removing it, its no longer there */

	if (!m->remove &&
	    !mail_folder_cache_get_folder_from_uri (folder_cache, m->uri, NULL)) {
		g_warning (
			"Folder '%s' disappeared while I was "
			"adding/removing it to/from my vfolder", m->uri);
		return;
	}

	/* always pick fresh folders - they are
	 * from CamelStore's folders bag anyway */
	folder = e_mail_session_uri_to_folder_sync (
		m->session, m->uri, 0, cancellable, error);

	if (folder != NULL) {
		l = m->folders;
		while (l && !vfolder_shutdown) {
			if (m->remove)
				camel_vee_folder_remove_folder (
					CAMEL_VEE_FOLDER (l->data), folder, cancellable);
			else
				camel_vee_folder_add_folder ((CamelVeeFolder *) l->data, folder, cancellable);
			l = l->next;
		}
		g_object_unref (folder);
	}
}

static void
vfolder_adduri_done (struct _adduri_msg *m)
{
}

static void
vfolder_adduri_free (struct _adduri_msg *m)
{
	g_object_unref (m->session);
	g_list_foreach (m->folders, (GFunc) camel_folder_thaw, NULL);
	g_list_free_full (m->folders, g_object_unref);
	g_free (m->uri);
}

static MailMsgInfo vfolder_adduri_info = {
	sizeof (struct _adduri_msg),
	(MailMsgDescFunc) vfolder_adduri_desc,
	(MailMsgExecFunc) vfolder_adduri_exec,
	(MailMsgDoneFunc) vfolder_adduri_done,
	(MailMsgFreeFunc) vfolder_adduri_free
};

/* uri should be a camel uri */
static gint
vfolder_adduri (EMailSession *session,
                const gchar *uri,
                GList *folders,
                gint remove)
{
	struct _adduri_msg *m;
	gint id;

	m = mail_msg_new (&vfolder_adduri_info);
	m->session = g_object_ref (session);
	m->folders = folders;
	m->uri = g_strdup (uri);
	m->remove = remove;

	g_list_foreach (m->folders, (GFunc) camel_folder_freeze, NULL);

	id = m->base.seq;
	mail_msg_slow_ordered_push (m);

	return id;
}

/* ********************************************************************** */

/* so special we never use it */
static gint
folder_is_spethal (CamelStore *store,
                   const gchar *folder_name)
{
	/* This is a bit of a hack, but really the only way it can be done
	 * at the moment. */

	if (store->flags & CAMEL_STORE_VTRASH)
		if (g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0)
			return TRUE;

	if (store->flags & CAMEL_STORE_VJUNK)
		if (g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0)
			return TRUE;

	return FALSE;
}

/**
 * mail_vfolder_add_folder:
 * @store: a #CamelStore
 * @folder: a folder name
 * @remove: whether the folder should be removed or added
 *
 * Called when a new folder becomes (un)available.  If @store is not a
 * CamelVeeStore, the folder is added/removed from the list of cached source
 * folders.  Then each vfolder rule is checked to see if the specified folder
 * matches a source of the rule.  It builds a list of vfolders that use (or
 * would use) the specified folder as a source.  It then adds (or removes)
 * this folder to (from) those vfolders via camel_vee_folder_add/
 * remove_folder() but does not modify the actual filters or write changes
 * to disk.
 *
 * NOTE: This function must be called from the main thread.
 */
static void
mail_vfolder_add_folder (CamelStore *store,
                         const gchar *folder_name,
                         gint remove)
{
	CamelService *service;
	CamelSession *session;
	EFilterRule *rule;
	const gchar *source;
	CamelVeeFolder *vf;
	CamelProvider *provider;
	GList *folders = NULL;
	gint remote;
	gchar *uri;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);

	service = CAMEL_SERVICE (store);
	session = camel_service_get_session (service);
	provider = camel_service_get_provider (service);

	remote = (provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0;

	if (folder_is_spethal (store, folder_name))
		return;

	g_return_if_fail (mail_in_main_thread ());

	uri = e_mail_folder_uri_build (store, folder_name);

	G_LOCK (vfolder);

	if (context == NULL)
		goto done;

	rule = NULL;
	while ((rule = e_rule_context_next_rule ((ERuleContext *) context, rule, NULL))) {
		gint found = FALSE;

		if (!rule->name) {
			d(printf("invalid rule (%p): rule->name is set to NULL\n", rule));
			continue;
		}
		/* Don't auto-add any sent/drafts folders etc,
		 * they must be explictly listed as a source. */
		if (rule->source
		    && !CAMEL_IS_VEE_STORE (store)
		    && ((em_vfolder_rule_get_with ((EMVFolderRule *) rule) == EM_VFOLDER_RULE_WITH_LOCAL && !remote)
			|| (em_vfolder_rule_get_with ((EMVFolderRule *) rule) == EM_VFOLDER_RULE_WITH_REMOTE_ACTIVE && remote)
			|| (em_vfolder_rule_get_with ((EMVFolderRule *) rule) == EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE)))
			found = TRUE;

		source = NULL;
		while (!found && (source = em_vfolder_rule_next_source (
				(EMVFolderRule *) rule, source))) {
			found = e_mail_folder_uri_equal (session, uri, source);
		}

		if (found) {
			vf = g_hash_table_lookup (vfolder_hash, rule->name);
			if (!vf) {
				g_warning ("vf is NULL for %s\n", rule->name);
				continue;
			}
			g_object_ref (vf);
			folders = g_list_prepend (folders, vf);
		}
	}

done:
	G_UNLOCK (vfolder);

	if (folders != NULL)
		vfolder_adduri (
			E_MAIL_SESSION (session),
			uri, folders, remove);

	g_free (uri);
}

/**
 * mail_vfolder_delete_folder:
 * @store: a #CamelStore
 * @folder_name: a folder name
 *
 * Looks through all vfolder rules to see if @folder_name is listed as a
 * source for any vfolder rules.  If the folder is found in the source for
 * any rule, it is removed and the user is alerted to the fact that the
 * vfolder rules have been updated.  The new vfolder rules are written
 * to disk.
 *
 * XXX: It doesn't appear that the changes to the vfolder rules are sent
 * down to the camel level, however. So the actual vfolders will not change
 * behavior until evolution is restarted (?)
 *
 * NOTE: This function must be called from the main thread.
 */
static void
mail_vfolder_delete_folder (CamelStore *store,
                            const gchar *folder_name)
{
	ERuleContext *rule_context;
	EFilterRule *rule;
	CamelService *service;
	CamelSession *session;
	const gchar *source;
	CamelVeeFolder *vf;
	GString *changed;
	guint changed_count;
	gchar *uri;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);

	if (folder_is_spethal (store, folder_name))
		return;

	d(printf ("Deleting uri to check: %s\n", uri));

	g_return_if_fail (mail_in_main_thread ());

	service = CAMEL_SERVICE (store);
	session = camel_service_get_session (service);

	uri = e_mail_folder_uri_build (store, folder_name);

	changed_count = 0;
	changed = g_string_new ("");

	G_LOCK (vfolder);

	if (context == NULL)
		goto done;

	rule_context = E_RULE_CONTEXT (context);

	/* see if any rules directly reference this removed uri */
	rule = NULL;
	while ((rule = e_rule_context_next_rule (rule_context, rule, NULL))) {
		EMVFolderRule *vf_rule = EM_VFOLDER_RULE (rule);

		if (!rule->name)
			continue;

		source = NULL;
		while ((source = em_vfolder_rule_next_source (vf_rule, source))) {
			/* Remove all sources that match, ignore changed events though
			 * because the adduri call above does the work async */
			if (e_mail_folder_uri_equal (session, uri, source)) {
				vf = g_hash_table_lookup (
					vfolder_hash, rule->name);

				if (!vf) {
					g_warning ("vf is NULL for %s\n", rule->name);
					continue;
				}

				g_signal_handlers_disconnect_matched (
					rule, G_SIGNAL_MATCH_FUNC |
					G_SIGNAL_MATCH_DATA, 0, 0, NULL,
					rule_changed, vf);

				em_vfolder_rule_remove_source (vf_rule, source);

				g_signal_connect (
					rule, "changed",
					G_CALLBACK (rule_changed), vf);

				if (changed_count == 0) {
					g_string_append (changed, rule->name);
				} else {
					if (changed_count == 1) {
						g_string_prepend (changed, "    ");
						g_string_append (changed, "\n");
					}
					g_string_append_printf (
						changed, "    %s\n",
						rule->name);
				}

				changed_count++;
				source = NULL;
			}
		}
	}

done:
	G_UNLOCK (vfolder);

	if (changed_count > 0) {
		EAlertSink *alert_sink;
		const gchar *config_dir;
		gchar *user, *info;

		alert_sink = mail_msg_get_alert_sink ();

		info = g_strdup_printf (ngettext (
			/* Translators: The first %s is name of the affected
			 * search folder(s), the second %s is the URI of the
			 * removed folder. For more than one search folder is
			 * each of them on a separate line, with four spaces
			 * in front of its name, without quotes. */
			"The Search Folder \"%s\" has been modified to "
			"account for the deleted folder\n\"%s\".",
			"The following Search Folders\n%s have been modified "
			"to account for the deleted folder\n\"%s\".",
			changed_count), changed->str, uri);
		e_alert_submit (
			alert_sink, "mail:vfolder-updated", info, NULL);
		g_free (info);

		config_dir = mail_session_get_config_dir ();
		user = g_build_filename (config_dir, "vfolders.xml", NULL);
		e_rule_context_save ((ERuleContext *) context, user);
		g_free (user);
	}

	g_string_free (changed, TRUE);

	g_free (uri);
}

/* called when a uri is renamed in a store */
static void
mail_vfolder_rename_folder (CamelStore *store,
                            const gchar *old_folder_name,
                            const gchar *new_folder_name)
{
	ERuleContext *rule_context;
	EFilterRule *rule;
	const gchar *source;
	CamelVeeFolder *vf;
	CamelService *service;
	CamelSession *session;
	gint changed = 0;
	gchar *old_uri;
	gchar *new_uri;

	d(printf("vfolder rename uri: %s to %s\n", cfrom, cto));

	if (context == NULL)
		return;

	if (folder_is_spethal (store, old_folder_name))
		return;

	if (folder_is_spethal (store, new_folder_name))
		return;

	g_return_if_fail (mail_in_main_thread ());

	service = CAMEL_SERVICE (store);
	session = camel_service_get_session (service);

	old_uri = e_mail_folder_uri_build (store, old_folder_name);
	new_uri = e_mail_folder_uri_build (store, new_folder_name);

	G_LOCK (vfolder);

	rule_context = E_RULE_CONTEXT (context);

	/* see if any rules directly reference this removed uri */
	rule = NULL;
	while ((rule = e_rule_context_next_rule (rule_context, rule, NULL))) {
		EMVFolderRule *vf_rule = EM_VFOLDER_RULE (rule);

		source = NULL;
		while ((source = em_vfolder_rule_next_source (vf_rule, source))) {
			/* Remove all sources that match, ignore changed events though
			 * because the adduri call above does the work async */
			if (e_mail_folder_uri_equal (session, old_uri, source)) {
				vf = g_hash_table_lookup (vfolder_hash, rule->name);
				if (!vf) {
					g_warning ("vf is NULL for %s\n", rule->name);
					continue;
				}

				g_signal_handlers_disconnect_matched (
					rule, G_SIGNAL_MATCH_FUNC |
					G_SIGNAL_MATCH_DATA, 0, 0, NULL,
					rule_changed, vf);

				em_vfolder_rule_remove_source (vf_rule, source);
				em_vfolder_rule_add_source (vf_rule, new_uri);

				g_signal_connect (
					vf_rule, "changed",
					G_CALLBACK (rule_changed), vf);

				changed++;
				source = NULL;
			}
		}
	}

	G_UNLOCK (vfolder);

	if (changed) {
		const gchar *config_dir;
		gchar *user;

		d(printf("Vfolders updated from renamed folder\n"));
		config_dir = mail_session_get_config_dir ();
		user = g_build_filename (config_dir, "vfolders.xml", NULL);
		e_rule_context_save ((ERuleContext *) context, user);
		g_free (user);
	}

	g_free (old_uri);
	g_free (new_uri);
}

/* ********************************************************************** */

static void context_rule_added (ERuleContext *ctx, EFilterRule *rule, EMailSession *session);

static void
rule_add_sources (EMailSession *session,
                  GQueue *queue,
                  GList **sources_folderp,
                  GList **sources_urip)
{
	GList *sources_folder = *sources_folderp;
	GList *sources_uri = *sources_urip;
	MailFolderCache *folder_cache;
	GList *head, *link;

	folder_cache = e_mail_session_get_folder_cache (session);

	head = g_queue_peek_head_link (queue);
	for (link = head; link != NULL; link = g_list_next (link)) {
		const gchar *uri = link->data;

		/* always pick fresh folders - they are
		 * from CamelStore's folders bag anyway */
		if (mail_folder_cache_get_folder_from_uri (folder_cache, uri, NULL)) {
			sources_uri = g_list_append (sources_uri, g_strdup (uri));
		}
	}

	*sources_folderp = sources_folder;
	*sources_urip = sources_uri;
}

static EMailSession *
get_session (CamelFolder *folder)
{
	CamelStore *store;

	store = camel_folder_get_parent_store (folder);

	return (EMailSession *) camel_service_get_session (CAMEL_SERVICE (store));
}

static void
rule_changed (EFilterRule *rule,
              CamelFolder *folder)
{
	EMailSession *session;
	CamelService *service;
	GList *sources_uri = NULL;
	GList *sources_folder = NULL;
	GString *query;
	const gchar *full_name;

	full_name = camel_folder_get_full_name (folder);
	session = get_session (folder);

	service = camel_session_get_service (
		CAMEL_SESSION (session), E_MAIL_SESSION_VFOLDER_UID);
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	/* If the folder has changed name, then
	 * add it, then remove the old manually. */
	if (strcmp (full_name, rule->name) != 0) {
		gchar *oldname;

		gpointer key;
		gpointer oldfolder;

		G_LOCK (vfolder);
		if (g_hash_table_lookup_extended (
				vfolder_hash, full_name, &key, &oldfolder)) {
			g_hash_table_remove (vfolder_hash, key);
			g_free (key);
			g_hash_table_insert (
				vfolder_hash, g_strdup (rule->name), folder);
			G_UNLOCK (vfolder);
		} else {
			G_UNLOCK (vfolder);
			g_warning (
				"couldn't find a vfolder rule "
				"in our table? %s", full_name);
		}

		oldname = g_strdup (full_name);
		/* FIXME Not passing a GCancellable or GError. */
		camel_store_rename_folder_sync (
			CAMEL_STORE (service),
			oldname, rule->name, NULL, NULL);
		g_free (oldname);
	}

	d(printf("Filter rule changed? for folder '%s'!!\n", folder->name));

	camel_vee_folder_set_auto_update (CAMEL_VEE_FOLDER (folder),
		em_vfolder_rule_get_autoupdate ((EMVFolderRule *) rule));

	if (em_vfolder_rule_get_with ((EMVFolderRule *) rule) == EM_VFOLDER_RULE_WITH_SPECIFIC) {
		/* find any (currently available) folders, and add them to the ones to open */
		rule_add_sources (
			session, em_vfolder_rule_get_sources ((EMVFolderRule *) rule),
			&sources_folder, &sources_uri);
	}

	G_LOCK (vfolder);

	if (em_vfolder_rule_get_with ((EMVFolderRule *) rule) == EM_VFOLDER_RULE_WITH_LOCAL ||
	    em_vfolder_rule_get_with ((EMVFolderRule *) rule) == EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE) {

		MailFolderCache *cache;
		GQueue queue = G_QUEUE_INIT;

		cache = e_mail_session_get_folder_cache (session);
		mail_folder_cache_get_local_folder_uris (cache, &queue);

		rule_add_sources (
			session, &queue, &sources_folder, &sources_uri);

		while (!g_queue_is_empty (&queue))
			g_free (g_queue_pop_head (&queue));
	}

	if (em_vfolder_rule_get_with ((EMVFolderRule *) rule) == EM_VFOLDER_RULE_WITH_REMOTE_ACTIVE ||
	    em_vfolder_rule_get_with ((EMVFolderRule *) rule) == EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE) {

		MailFolderCache *cache;
		GQueue queue = G_QUEUE_INIT;

		cache = e_mail_session_get_folder_cache (session);
		mail_folder_cache_get_remote_folder_uris (cache, &queue);

		rule_add_sources (
			session, &queue, &sources_folder, &sources_uri);

		while (!g_queue_is_empty (&queue))
			g_free (g_queue_pop_head (&queue));
	}

	G_UNLOCK (vfolder);

	query = g_string_new("");
	e_filter_rule_build_code (rule, query);

	vfolder_setup (session, folder, query->str, sources_uri, sources_folder);

	g_string_free (query, TRUE);
}

static void
context_rule_added (ERuleContext *ctx,
                    EFilterRule *rule,
                    EMailSession *session)
{
	CamelFolder *folder;
	CamelService *service;

	d(printf("rule added: %s\n", rule->name));

	service = camel_session_get_service (
		CAMEL_SESSION (session), E_MAIL_SESSION_VFOLDER_UID);
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	/* this always runs quickly */
	/* FIXME Not passing a GCancellable or GError. */
	folder = camel_store_get_folder_sync (
		CAMEL_STORE (service), rule->name, 0, NULL, NULL);
	if (folder) {
		g_signal_connect (
			rule, "changed",
			G_CALLBACK (rule_changed), folder);

		G_LOCK (vfolder);
		g_hash_table_insert (vfolder_hash, g_strdup (rule->name), folder);
		G_UNLOCK (vfolder);

		rule_changed (rule, folder);
	}
}

static void
context_rule_removed (ERuleContext *ctx,
                      EFilterRule *rule,
                      EMailSession *session)
{
	CamelService *service;
	gpointer key, folder = NULL;

	d(printf("rule removed; %s\n", rule->name));

	service = camel_session_get_service (
		CAMEL_SESSION (session), E_MAIL_SESSION_VFOLDER_UID);
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	/* TODO: remove from folder info cache? */

	G_LOCK (vfolder);
	if (g_hash_table_lookup_extended (vfolder_hash, rule->name, &key, &folder)) {
		g_hash_table_remove (vfolder_hash, key);
		g_free (key);
	}
	G_UNLOCK (vfolder);

	/* FIXME Not passing a GCancellable  or GError. */
	camel_store_delete_folder_sync (
		CAMEL_STORE (service), rule->name, NULL, NULL);
	/* this must be unref'd after its deleted */
	if (folder)
		g_object_unref ((CamelFolder *) folder);
}

static void
store_folder_deleted_cb (CamelStore *store,
                         CamelFolderInfo *info)
{
	EFilterRule *rule;
	gchar *user;

	d(printf("Folder deleted: %s\n", info->name));

	/* Unmatched folder doesn't have any rule */
	if (g_strcmp0 (CAMEL_UNMATCHED_NAME, info->full_name) == 0)
		return;

	/* Warning not thread safe, but might be enough */
	G_LOCK (vfolder);

	/* delete it from our list */
	rule = e_rule_context_find_rule ((ERuleContext *) context, info->full_name, NULL);
	if (rule) {
		const gchar *config_dir;
		EMailSession *session = E_MAIL_SESSION (camel_service_get_session (CAMEL_SERVICE (store)));

		/* We need to stop listening to removed events,
		 * otherwise we'll try and remove it again. */
		g_signal_handlers_disconnect_matched (
			context, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, context_rule_removed, NULL);
		e_rule_context_remove_rule ((ERuleContext *) context, rule);
		g_object_unref (rule);
		g_signal_connect (
			context, "rule_removed",
			G_CALLBACK (context_rule_removed), session);

		config_dir = mail_session_get_config_dir ();
		user = g_build_filename (config_dir, "vfolders.xml", NULL);
		e_rule_context_save ((ERuleContext *) context, user);
		g_free (user);
	} else {
		g_warning (
			"Cannot find rule for deleted vfolder '%s'",
			info->display_name);
	}

	G_UNLOCK (vfolder);
}

static void
store_folder_renamed_cb (CamelStore *store,
                         const gchar *old_name,
                         CamelFolderInfo *info)
{
	EFilterRule *rule;
	gchar *user;

	gpointer key, folder;

	/* This should be more-or-less thread-safe */

	d(printf("Folder renamed to '%s' from '%s'\n", info->full_name, old_name));

	/* Folder is already renamed? */
	G_LOCK (vfolder);
	d(printf("Changing folder name in hash table to '%s'\n", info->full_name));
	if (g_hash_table_lookup_extended (vfolder_hash, old_name, &key, &folder)) {
		const gchar *config_dir;

		g_hash_table_remove (vfolder_hash, key);
		g_free (key);
		g_hash_table_insert (vfolder_hash, g_strdup (info->full_name), folder);

		rule = e_rule_context_find_rule ((ERuleContext *) context, old_name, NULL);
		if (!rule) {
			G_UNLOCK (vfolder);
			g_warning ("Rule shouldn't be NULL\n");
			return;
		}

		g_signal_handlers_disconnect_matched (
			rule, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
			0, 0, NULL, rule_changed, folder);
		e_filter_rule_set_name (rule, info->full_name);
		g_signal_connect (
			rule, "changed",
			G_CALLBACK (rule_changed), folder);

		config_dir = mail_session_get_config_dir ();
		user = g_build_filename (config_dir, "vfolders.xml", NULL);
		e_rule_context_save ((ERuleContext *) context, user);
		g_free (user);

		G_UNLOCK (vfolder);
	} else {
		G_UNLOCK (vfolder);
		g_warning("couldn't find a vfolder rule in our table? %s", info->full_name);
	}
}

static void
folder_available_cb (MailFolderCache *cache,
                     CamelStore *store,
                     const gchar *folder_name)
{
	mail_vfolder_add_folder (store, folder_name, FALSE);
}

static void
folder_unavailable_cb (MailFolderCache *cache,
                       CamelStore *store,
                       const gchar *folder_name)
{
	mail_vfolder_add_folder (store, folder_name, TRUE);
}

static void
folder_deleted_cb (MailFolderCache *cache,
                   CamelStore *store,
                   const gchar *folder_name)
{
	mail_vfolder_delete_folder (store, folder_name);
}

static void
folder_renamed_cb (MailFolderCache *cache,
                   CamelStore *store,
                   const gchar *old_folder_name,
                   const gchar *new_folder_name,
                   gpointer user_data)
{
	mail_vfolder_rename_folder (store, old_folder_name, new_folder_name);
}

void
vfolder_load_storage (EMailSession *session)
{
	/* lock for loading storage, it is safe to call it more than once */
	G_LOCK_DEFINE_STATIC (vfolder_hash);

	CamelStore *vfolder_store;
	const gchar *config_dir;
	gchar *user;
	EFilterRule *rule;
	MailFolderCache *folder_cache;
	gchar *xmlfile;

	G_LOCK (vfolder_hash);

	if (vfolder_hash) {
		/* we have already initialized */
		G_UNLOCK (vfolder_hash);
		return;
	}

	vfolder_hash = g_hash_table_new (g_str_hash, g_str_equal);

	G_UNLOCK (vfolder_hash);

	config_dir = mail_session_get_config_dir ();
	vfolder_store = e_mail_session_get_vfolder_store (session);

	g_signal_connect (
		vfolder_store, "folder-deleted",
		G_CALLBACK (store_folder_deleted_cb), NULL);

	g_signal_connect (
		vfolder_store, "folder-renamed",
		G_CALLBACK (store_folder_renamed_cb), NULL);

	/* load our rules */
	user = g_build_filename (config_dir, "vfolders.xml", NULL);
	/* This needs editor context which is only in the mail/. But really to run here we dont need editor context.
	 * So till we split this to EDS, we would let mail/ create this and later one it is any ways two separate
	 * contexts. */
	context = e_mail_session_create_vfolder_context (session);

	xmlfile = g_build_filename (EVOLUTION_PRIVDATADIR, "vfoldertypes.xml", NULL);
	if (e_rule_context_load ((ERuleContext *) context,
			       xmlfile, user) != 0) {
		g_warning("cannot load vfolders: %s\n", ((ERuleContext *)context)->error);
	}
	g_free (xmlfile);
	g_free (user);

	g_signal_connect (
		context, "rule_added",
		G_CALLBACK (context_rule_added), session);
	g_signal_connect (
		context, "rule_removed",
		G_CALLBACK (context_rule_removed), session);

	/* and setup the rules we have */
	rule = NULL;
	while ((rule = e_rule_context_next_rule ((ERuleContext *) context, rule, NULL))) {
		if (rule->name) {
			d(printf("rule added: %s\n", rule->name));
			context_rule_added ((ERuleContext *) context, rule, session);
		} else {
			d(printf("invalid rule (%p) encountered: rule->name is NULL\n", rule));
		}
	}

	folder_cache = e_mail_session_get_folder_cache (session);

	g_signal_connect (
		folder_cache, "folder-available",
		G_CALLBACK (folder_available_cb), NULL);
	g_signal_connect (
		folder_cache, "folder-unavailable",
		G_CALLBACK (folder_unavailable_cb), NULL);
	g_signal_connect (
		folder_cache, "folder-deleted",
		G_CALLBACK (folder_deleted_cb), NULL);
	g_signal_connect (
		folder_cache, "folder-renamed",
		G_CALLBACK (folder_renamed_cb), NULL);
}

static void
vfolder_foreach_cb (gpointer key,
                    gpointer data,
                    gpointer user_data)
{
	CamelFolder *folder = CAMEL_FOLDER (data);

	if (folder)
		g_object_unref (folder);

	g_free (key);
}

void
mail_vfolder_shutdown (void)
{
	vfolder_shutdown = 1;

	if (vfolder_hash) {
		g_hash_table_foreach (vfolder_hash, vfolder_foreach_cb, NULL);
		g_hash_table_destroy (vfolder_hash);
		vfolder_hash = NULL;
	}

	if (context) {
		g_object_unref (context);
		context = NULL;
	}
}
