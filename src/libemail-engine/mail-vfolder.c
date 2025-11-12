/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "mail-vfolder.h"

#include <string.h>
#include <glib/gi18n.h>

#include "libemail-engine/e-mail-folder-utils.h"
#include "libemail-engine/e-mail-session.h"
#include "libemail-engine/e-mail-utils.h"
#include "libemail-engine/em-vfolder-context.h"
#include "libemail-engine/em-vfolder-rule.h"
#include "libemail-engine/mail-folder-cache.h"
#include "libemail-engine/mail-mt.h"
#include "libemail-engine/mail-ops.h"
#include "libemail-engine/mail-tools.h"

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

static gboolean
vfolder_cache_has_folder_info (EMailSession *session,
                               const gchar *folder_uri)
{
	MailFolderCache *folder_cache;
	CamelStore *store = NULL;
	gchar *folder_name = NULL;
	gboolean cache_has_info = FALSE;

	folder_cache = e_mail_session_get_folder_cache (session);

	e_mail_folder_uri_parse (
		CAMEL_SESSION (session), folder_uri,
		&store, &folder_name, NULL);

	if (store != NULL && folder_name != NULL) {
		cache_has_info = mail_folder_cache_has_folder_info (
			folder_cache, store, folder_name);
	}

	g_clear_object (&store);
	g_free (folder_name);

	return cache_has_info;
}

static GPtrArray * /* gchar *uri */
vfolder_get_include_subfolders_uris (EMailSession *session,
                                     const gchar *base_uri,
                                     GCancellable *cancellable)
{
	GPtrArray *uris = NULL;
	CamelStore *store = NULL;
	gchar *folder_name = NULL;
	CamelFolderInfo *fi;
	const CamelFolderInfo *cur;

	g_return_val_if_fail (session != NULL, NULL);
	g_return_val_if_fail (base_uri != NULL, NULL);

	if (!e_mail_folder_uri_parse (CAMEL_SESSION (session), base_uri, &store, &folder_name, NULL))
		return NULL;

	fi = camel_store_get_folder_info_sync (
		store, folder_name,
		CAMEL_STORE_FOLDER_INFO_RECURSIVE, cancellable, NULL);
	cur = fi;
	while (cur) {
		if ((cur->flags & CAMEL_FOLDER_NOSELECT) == 0) {
			gchar *fi_uri = e_mail_folder_uri_build (store, cur->full_name);

			if (fi_uri) {
				if (!uris)
					uris = g_ptr_array_new_with_free_func (g_free);

				g_ptr_array_add (uris, fi_uri);
			}
		}

		/* move to the next fi */
		if (cur->child) {
			cur = cur->child;
		} else if (cur->next) {
			cur = cur->next;
		} else {
			while (cur && !cur->next) {
				cur = cur->parent;
			}

			if (cur)
				cur = cur->next;
		}
	}

	camel_folder_info_free (fi);

	g_object_unref (store);
	g_free (folder_name);

	return uris;
}

struct _setup_msg {
	MailMsg base;

	EMailSession *session;
	CamelFolder *folder;
	gchar *query;
	GList *sources_uri;
};

static gchar *
vfolder_setup_desc (struct _setup_msg *m)
{
	return g_strdup_printf (
		_("Setting up Search Folder: %s"),
		camel_folder_get_full_display_name (m->folder));
}

static void
vfolder_setup_exec (struct _setup_msg *m,
                    GCancellable *cancellable,
                    GError **error)
{
	GList *l;
	GPtrArray *folders;
	CamelFolder *folder;

	camel_vee_folder_set_expression_sync ((CamelVeeFolder *) m->folder, m->query, CAMEL_VEE_FOLDER_OP_FLAG_SKIP_REBUILD, cancellable, NULL);

	folders = g_ptr_array_new_with_free_func (g_object_unref);

	for (l = m->sources_uri;
	     l && !vfolder_shutdown && !g_cancellable_is_cancelled (cancellable);
	     l = l->next) {
		const gchar *uri = l->data;

		d (printf (" Adding uri: %s\n", uri));

		if (!uri || !*uri || !uri[1])
			continue;

		if (*uri == '*') {
			/* include folder and its subfolders */
			GPtrArray *uris;
			guint ii;

			uris = vfolder_get_include_subfolders_uris (m->session, uri, cancellable);
			for (ii = 0; uris && ii < uris->len; ii++) {
				const gchar *fi_uri = g_ptr_array_index (uris, ii);

				folder = e_mail_session_uri_to_folder_sync (
					m->session, fi_uri, 0, cancellable, NULL);
				if (folder != NULL)
					g_ptr_array_add (folders, folder);
			}

			g_clear_pointer (&uris, g_ptr_array_unref);
		} else {
			folder = e_mail_session_uri_to_folder_sync (m->session, l->data, 0, cancellable, NULL);
			if (folder != NULL)
				g_ptr_array_add (folders, folder);
		}
	}

	if (!vfolder_shutdown && !g_cancellable_is_cancelled (cancellable))
		camel_vee_folder_set_folders_sync ((CamelVeeFolder *) m->folder, folders, CAMEL_VEE_FOLDER_OP_FLAG_NONE, cancellable, error);

	g_ptr_array_unref (folders);
}

static void
vfolder_setup_done (struct _setup_msg *m)
{
}

static void
vfolder_setup_free (struct _setup_msg *m)
{
	camel_folder_thaw (m->folder);

	g_object_unref (m->session);
	g_object_unref (m->folder);
	g_free (m->query);
	g_list_free_full (m->sources_uri, g_free);
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
vfolder_setup (CamelSession *session,
               CamelFolder *folder,
               const gchar *query,
               GList *sources_uri)
{
	struct _setup_msg *m;
	gint id;

	m = mail_msg_new (&vfolder_setup_info);
	m->session = E_MAIL_SESSION (g_object_ref (session));
	m->folder = g_object_ref (folder);
	/* Make sure the query is enclosed in "(match-all ...)", to traverse the folders' content */
	m->query = (!query || g_str_has_prefix (query, "(match-all ") || strstr (query, "(match-threads ")) ? g_strdup (query) : g_strconcat ("(match-all ", query, ")", NULL);
	m->sources_uri = sources_uri;

	camel_folder_freeze (m->folder);

	id = m->base.seq;
	mail_msg_slow_ordered_push (m);

	return id;
}

/* ********************************************************************** */

static void
vfolder_setup_changed_cb (CamelVeeFolder *vfolder,
			  gpointer user_data)
{
	gboolean *p_changed = user_data;

	*p_changed = TRUE;
}

static void
vfolder_add_remove_one (CamelVeeFolder *vfolder,
			EMailSession *session,
			const gchar *uri,
			gboolean is_remove,
			GCancellable *cancellable)
{
	CamelFolder *subfolder;

	subfolder = e_mail_session_uri_to_folder_sync (session, uri, 0, cancellable, NULL);
	if (!subfolder)
		return;

	if (is_remove)
		camel_vee_folder_remove_folder_sync (vfolder, subfolder, CAMEL_VEE_FOLDER_OP_FLAG_SKIP_REBUILD, cancellable, NULL);
	else
		camel_vee_folder_add_folder_sync (vfolder, subfolder, CAMEL_VEE_FOLDER_OP_FLAG_SKIP_REBUILD, cancellable, NULL);

	g_object_unref (subfolder);
}

typedef struct _FolderURI {
	gchar *uri;
	gboolean is_remove;
	gboolean include_subfolders;
} FolderURI;

static void
folder_uri_clear (gpointer ptr)
{
	FolderURI *furi = ptr;

	g_free (furi->uri);
}

static void
vfolder_split_by_vfolders_locked (GHashTable *in_folders, /* (nullable) CamelStore ~> GPtrArray { gchar *folder_name } */
				  gboolean is_remove,
				  CamelSession *session,
				  GHashTable *inout_vfolders, /* CamelVeeFolder ~> GArray { FolderURI } */
				  GCancellable *cancellable)
{
	GHashTableIter iter;
	gpointer key = NULL, value = NULL;

	if (!in_folders || !context)
		return;

	g_hash_table_iter_init (&iter, in_folders);

	while (!vfolder_shutdown && g_hash_table_iter_next (&iter, &key, &value) && !g_cancellable_is_cancelled (cancellable)) {
		CamelStore *store = key;
		GPtrArray *names = value;
		gboolean is_remote_store;
		guint ii;

		is_remote_store = (camel_service_get_provider (CAMEL_SERVICE (store))->flags & CAMEL_PROVIDER_IS_REMOTE) != 0;

		for (ii = 0; ii < names->len && !vfolder_shutdown && !g_cancellable_is_cancelled (cancellable); ii++) {
			const gchar *folder_name = g_ptr_array_index (names, ii);
			EFilterRule *rule = NULL;
			EMVFolderRule *vrule;
			const gchar *source;
			gchar *uri;

			uri = e_mail_folder_uri_build (store, folder_name);

			while ((rule = e_rule_context_next_rule ((ERuleContext *) context, rule, NULL)) != NULL &&
			       !vfolder_shutdown && !g_cancellable_is_cancelled (cancellable)) {
				gboolean found = FALSE;

				if (!rule->name) {
					d (printf ("invalid rule (%p): rule->name is set to NULL\n", rule));
					continue;
				}

				vrule = (EMVFolderRule *) rule;

				/* Don't auto-add any sent/drafts folders etc, they must be explictly listed as a source. */
				if (rule->source && !CAMEL_IS_VEE_STORE (store) &&
				    ((em_vfolder_rule_get_with (vrule) == EM_VFOLDER_RULE_WITH_LOCAL && !is_remote_store) ||
				     (em_vfolder_rule_get_with (vrule) == EM_VFOLDER_RULE_WITH_REMOTE_ACTIVE && is_remote_store) ||
				     (em_vfolder_rule_get_with (vrule) == EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE)))
					found = TRUE;

				source = NULL;
				while (!found && (source = em_vfolder_rule_next_source (vrule, source))) {
					found = e_mail_folder_uri_equal (session, uri, source);
				}

				if (found) {
					CamelVeeFolder *vf;
					FolderURI furi;
					GArray *array;

					vf = g_hash_table_lookup (vfolder_hash, rule->name);
					if (!vf) {
						g_warning ("vf is NULL for %s\n", rule->name);
						continue;
					}

					array = g_hash_table_lookup (inout_vfolders, vf);
					if (!array) {
						array = g_array_new (FALSE, FALSE, sizeof (FolderURI));
						g_array_set_clear_func (array, folder_uri_clear);
						g_hash_table_insert (inout_vfolders, g_object_ref (vf), array);
					}

					furi.uri = g_strdup (uri);
					furi.is_remove = is_remove;
					furi.include_subfolders = em_vfolder_rule_source_get_include_subfolders (vrule, uri);

					g_array_append_val (array, furi);
				}
			}

			g_free (uri);
		}
	}
}

static void
vfolder_rebuild_in_session_job_cb (CamelSession *session,
				   GCancellable *cancellable,
				   gpointer user_data,
				   GError **error)
{
	camel_folder_refresh_info_sync (CAMEL_FOLDER (user_data), cancellable, error);
}

struct _note_folders_msg {
	MailMsg base;

	EMailSession *session;
	GHashTable *added_folders; /* (nullable) CamelStore ~> GPtrArray { gchar *folder_name } */
	GHashTable *removed_folders; /* (nullable) CamelStore ~> GPtrArray { gchar *folder_name } */
};

static gchar *
vfolder_note_folders_desc (struct _note_folders_msg *m)
{
	return NULL; /* g_strdup (_("Updating Search Folders")); it's a new string, cannot add it in the stable branch */
}

static void
vfolder_note_folders_exec (struct _note_folders_msg *m,
			   GCancellable *cancellable,
			   GError **error)
{
	GHashTable *split_vfolders; /* CamelVeeFolder ~> GArray { FolderURI } */
	GHashTableIter iter;
	gpointer key = NULL, value = NULL;

	if (vfolder_shutdown || (!m->added_folders && !m->removed_folders))
		return;

	g_return_if_fail (m->session != NULL);

	split_vfolders = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, (GDestroyNotify) g_array_unref);

	G_LOCK (vfolder);
	vfolder_split_by_vfolders_locked (m->removed_folders, TRUE, CAMEL_SESSION (m->session), split_vfolders, cancellable);
	vfolder_split_by_vfolders_locked (m->added_folders, FALSE, CAMEL_SESSION (m->session), split_vfolders, cancellable);
	G_UNLOCK (vfolder);

	g_hash_table_iter_init (&iter, split_vfolders);
	while (!vfolder_shutdown && g_hash_table_iter_next (&iter, &key, &value) && !g_cancellable_is_cancelled (cancellable)) {
		CamelVeeFolder *vf = key;
		GArray *furis_array = value;
		gboolean need_rebuild = FALSE;
		gulong handler_id;
		guint ii;

		handler_id = g_signal_connect (vf, "vee-setup-changed", G_CALLBACK (vfolder_setup_changed_cb), &need_rebuild);

		for (ii = 0; ii < furis_array->len && !vfolder_shutdown && !g_cancellable_is_cancelled (cancellable); ii++) {
			const FolderURI *furi = &g_array_index (furis_array, FolderURI, ii);

			if (furi->include_subfolders) {
				GPtrArray *uris;
				guint jj;

				uris = vfolder_get_include_subfolders_uris (m->session, furi->uri, cancellable);

				for (jj = 0; uris && jj < uris->len; jj++) {
					const gchar *uri = g_ptr_array_index (uris, jj);

					vfolder_add_remove_one (vf, m->session, uri, furi->is_remove, cancellable);
				}

				g_clear_pointer (&uris, g_ptr_array_unref);
			} else {
				vfolder_add_remove_one (vf, m->session, furi->uri, furi->is_remove, cancellable);
			}
		}

		g_signal_handler_disconnect (vf, handler_id);

		if (need_rebuild) {
			CamelStore *parent_store;
			gchar *description;

			parent_store = camel_folder_get_parent_store (CAMEL_FOLDER (vf));

			description = g_strdup_printf (_("Updating Search Folders for “%s : %s”"),
				parent_store ? camel_service_get_display_name (CAMEL_SERVICE (parent_store)) : "???",
				camel_folder_get_full_name (CAMEL_FOLDER (vf)));

			camel_session_submit_job (CAMEL_SESSION (m->session), description,
				vfolder_rebuild_in_session_job_cb, g_object_ref (vf), g_object_unref);

			g_free (description);
		}
	}

	g_hash_table_unref (split_vfolders);
}

static void
vfolder_note_folders_done (struct _note_folders_msg *m)
{
}

static void
vfolder_note_folders_free (struct _note_folders_msg *m)
{
	g_clear_object (&m->session);
	g_clear_pointer (&m->added_folders, g_hash_table_unref);
	g_clear_pointer (&m->removed_folders, g_hash_table_unref);
}

static MailMsgInfo vfolder_note_folders_info = {
	sizeof (struct _note_folders_msg),
	(MailMsgDescFunc) vfolder_note_folders_desc,
	(MailMsgExecFunc) vfolder_note_folders_exec,
	(MailMsgDoneFunc) vfolder_note_folders_done,
	(MailMsgFreeFunc) vfolder_note_folders_free
};

static gint glob_folders_timeout_id = 0;
static GHashTable *glob_added_folders = NULL; /* CamelStore ~> GPtrArray { gchar *folder_name } */
static GHashTable *glob_removed_folders = NULL; /* CamelStore ~> GPtrArray { gchar *folder_name } */

static gboolean
mail_vfolder_note_folders_timeout_cb (gpointer user_data)
{
	struct _note_folders_msg *m;
	GHashTable *added_folders;
	GHashTable *removed_folders;
	GHashTable *hash_table;

	G_LOCK (vfolder);
	glob_folders_timeout_id = 0;

	if (!context || vfolder_shutdown) {
		g_clear_pointer (&glob_added_folders, g_hash_table_unref);
		g_clear_pointer (&glob_removed_folders, g_hash_table_unref);
		G_UNLOCK (vfolder);
		return G_SOURCE_REMOVE;
	}

	added_folders = g_steal_pointer (&glob_added_folders);
	removed_folders = g_steal_pointer (&glob_removed_folders);
	G_UNLOCK (vfolder);

	m = mail_msg_new (&vfolder_note_folders_info);
	m->session = NULL;
	m->added_folders = added_folders;
	m->removed_folders = removed_folders;

	/* it does not matter which one, just get the session from one of the stores */
	hash_table = added_folders ? added_folders : removed_folders;

	if (hash_table) {
		GHashTableIter iter;
		gpointer key = NULL;

		g_hash_table_iter_init (&iter, hash_table);
		while (!m->session && g_hash_table_iter_next (&iter, &key, NULL)) {
			CamelService *service = key;

			m->session = E_MAIL_SESSION (camel_service_ref_session (service));
		}
	}

	mail_msg_slow_ordered_push (m);

	return G_SOURCE_REMOVE;
}

/* ********************************************************************** */

/* so special we never use it */
static gint
folder_is_spethal (CamelStore *store,
                   const gchar *folder_name)
{
	/* This is a bit of a hack, but really the only way it can be done
	 * at the moment. */

	if (camel_store_get_flags (store) & CAMEL_STORE_VTRASH)
		if (g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0)
			return TRUE;

	if (camel_store_get_flags (store) & CAMEL_STORE_VJUNK)
		if (g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0)
			return TRUE;

	return FALSE;
}

/*
 * mail_vfolder_note_folder:
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
mail_vfolder_note_folder (CamelStore *store,
			  const gchar *folder_name,
			  gint remove)
{
	g_return_if_fail (mail_in_main_thread ());

	if (folder_is_spethal (store, folder_name))
		return;

	G_LOCK (vfolder);

	if (context != NULL) {
		GHashTable **p_glob_folders;
		GHashTable *remove_from_hash;
		GPtrArray *names;

		if (remove)
			p_glob_folders = &glob_removed_folders;
		else
			p_glob_folders = &glob_added_folders;

		if (!*p_glob_folders)
			*p_glob_folders = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, (GDestroyNotify) g_ptr_array_unref);

		names = g_hash_table_lookup (*p_glob_folders, store);

		if (!names) {
			names = g_ptr_array_new_with_free_func (g_free);
			g_hash_table_insert (*p_glob_folders, g_object_ref (store), names);
		}

		g_ptr_array_add (names, g_strdup (folder_name));

		if (remove)
			remove_from_hash = glob_added_folders;
		else
			remove_from_hash = glob_removed_folders;

		if (remove_from_hash) {
			names = g_hash_table_lookup (remove_from_hash, store);
			if (names) {
				guint ii;

				for (ii = 0; ii < names->len; ii++) {
					const gchar *name = g_ptr_array_index (names, ii);

					if (g_strcmp0 (name, folder_name) == 0) {
						g_ptr_array_remove_index (names, ii);
						break;
					}
				}
			}
		}

		if (!glob_folders_timeout_id)
			glob_folders_timeout_id = g_timeout_add_seconds (1, mail_vfolder_note_folders_timeout_cb, NULL);
	}

	G_UNLOCK (vfolder);
}

/*
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

	d (printf ("Deleting uri to check: %s\n", uri));

	g_return_if_fail (mail_in_main_thread ());

	service = CAMEL_SERVICE (store);

	session = camel_service_ref_session (service);
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
						g_string_append_c (changed, '\n');
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
			"The Search Folder “%s” has been modified to "
			"account for the deleted folder\n“%s”.",
			"The following Search Folders\n%s have been modified "
			"to account for the deleted folder\n“%s”.",
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

	g_object_unref (session);
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

	d (printf ("vfolder rename uri: %s to %s\n", cfrom, cto));

	if (context == NULL)
		return;

	if (folder_is_spethal (store, old_folder_name))
		return;

	if (folder_is_spethal (store, new_folder_name))
		return;

	g_return_if_fail (mail_in_main_thread ());

	service = CAMEL_SERVICE (store);
	session = camel_service_ref_session (service);

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

		d (printf ("Vfolders updated from renamed folder\n"));
		config_dir = mail_session_get_config_dir ();
		user = g_build_filename (config_dir, "vfolders.xml", NULL);
		e_rule_context_save ((ERuleContext *) context, user);
		g_free (user);
	}

	g_free (old_uri);
	g_free (new_uri);

	g_object_unref (session);
}

/* ********************************************************************** */

static void context_rule_added (ERuleContext *ctx, EFilterRule *rule, EMailSession *session);

static void
rule_add_source (GList **psources_uri,
		 const gchar *uri,
		 EMVFolderRule *rule)
{
	/* "tag" uris with subfolders with a star prefix */
	if (!rule || !em_vfolder_rule_source_get_include_subfolders (rule, uri))
		*psources_uri = g_list_prepend (*psources_uri, g_strdup (uri));
	else
		*psources_uri = g_list_prepend (*psources_uri, g_strconcat ("*", uri, NULL));
}

static void
rule_add_sources (EMailSession *session,
                  GQueue *queue,
                  GList **sources_urip,
                  EMVFolderRule *rule)
{
	GList *head, *link;

	head = g_queue_peek_head_link (queue);
	for (link = head; link != NULL; link = g_list_next (link)) {
		const gchar *uri = link->data;

		/* always pick fresh folders - they are
		 * from CamelStore's folders bag anyway */
		if (vfolder_cache_has_folder_info (session, uri))
			rule_add_source (sources_urip, uri, rule);
	}
}

static gboolean
mail_vfolder_foreach_folder_uri_cb (const gchar *uri,
				    gpointer user_data)
{
	GList **psources_uri = user_data;

	rule_add_source (psources_uri, uri, NULL);

	return TRUE;
}

static void
rule_changed (EFilterRule *rule,
              CamelFolder *folder)
{
	CamelStore *store;
	CamelService *service;
	CamelSession *session;
	MailFolderCache *cache;
	GList *sources_uri = NULL;
	GString *query;
	const gchar *full_name;
	em_vfolder_rule_with_t rule_with;

	full_name = camel_folder_get_full_name (folder);
	store = camel_folder_get_parent_store (folder);
	session = camel_service_ref_session (CAMEL_SERVICE (store));
	cache = e_mail_session_get_folder_cache (E_MAIL_SESSION (session));

	service = camel_session_ref_service (
		session, E_MAIL_SESSION_VFOLDER_UID);
	g_return_if_fail (service != NULL);

	/* If the folder has changed name, then
	 * add it, then remove the old manually. */
	if (strcmp (full_name, rule->name) != 0) {
		gchar *oldname;

		gpointer key;
		gpointer oldfolder;

		G_LOCK (vfolder);
		if (g_hash_table_lookup_extended (
				vfolder_hash, full_name, &key, &oldfolder)) {
			g_warn_if_fail (oldfolder == folder);
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

	g_object_unref (service);
	service = NULL;

	d (printf ("Filter rule changed? for folder '%s'!!\n", folder->name));

	camel_vee_folder_set_auto_update (
		CAMEL_VEE_FOLDER (folder),
		em_vfolder_rule_get_autoupdate ((EMVFolderRule *) rule));

	if (em_vfolder_rule_get_with ((EMVFolderRule *) rule) == EM_VFOLDER_RULE_WITH_SPECIFIC) {
		/* find any (currently available) folders, and add them to the ones to open */
		rule_add_sources (
			E_MAIL_SESSION (session),
			em_vfolder_rule_get_sources ((EMVFolderRule *) rule),
			&sources_uri, (EMVFolderRule *) rule);
	}

	G_LOCK (vfolder);

	rule_with = em_vfolder_rule_get_with (EM_VFOLDER_RULE (rule));
	if (rule_with == EM_VFOLDER_RULE_WITH_LOCAL ||
	    rule_with == EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE) {
		mail_folder_cache_foreach_local_folder_uri (cache, mail_vfolder_foreach_folder_uri_cb, &sources_uri);
	}

	if (rule_with == EM_VFOLDER_RULE_WITH_REMOTE_ACTIVE ||
	    rule_with == EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE) {
		mail_folder_cache_foreach_remote_folder_uri (cache, mail_vfolder_foreach_folder_uri_cb, &sources_uri);
	}

	G_UNLOCK (vfolder);

	query = g_string_new ("");
	e_filter_rule_build_code (rule, query);

	vfolder_setup (session, folder, query->str, sources_uri);

	g_string_free (query, TRUE);

	g_object_unref (session);
}

static void
context_rule_added (ERuleContext *ctx,
                    EFilterRule *rule,
                    EMailSession *session)
{
	CamelFolder *folder;
	CamelService *service;

	d (printf ("rule added: %s\n", rule->name));

	service = camel_session_ref_service (
		CAMEL_SESSION (session), E_MAIL_SESSION_VFOLDER_UID);
	g_return_if_fail (service != NULL);

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

	g_object_unref (service);
}

static void
context_rule_removed (ERuleContext *ctx,
                      EFilterRule *rule,
                      EMailSession *session)
{
	CamelService *service;
	gpointer key, folder = NULL;

	d (printf ("rule removed; %s\n", rule->name));

	service = camel_session_ref_service (
		CAMEL_SESSION (session), E_MAIL_SESSION_VFOLDER_UID);
	g_return_if_fail (service != NULL);

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

	g_object_unref (service);
}

static void
store_folder_deleted_cb (CamelStore *store,
                         CamelFolderInfo *info)
{
	EFilterRule *rule;
	gchar *user;

	d (printf ("Folder deleted: %s\n", info->name));

	/* Warning not thread safe, but might be enough */
	G_LOCK (vfolder);

	/* delete it from our list; can be NULL when already removed in the Search Folders edit dialog */
	rule = e_rule_context_find_rule ((ERuleContext *) context, info->full_name, NULL);
	if (rule) {
		CamelSession *session;
		const gchar *config_dir;

		session = camel_service_ref_session (CAMEL_SERVICE (store));

		/* We need to stop listening to removed events,
		 * otherwise we'll try and remove it again. */
		g_signal_handlers_disconnect_matched (
			context, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, context_rule_removed, NULL);
		e_rule_context_remove_rule ((ERuleContext *) context, rule);
		g_object_unref (rule);

		/* FIXME This is dangerous.  Either the signal closure
		 *       needs to be referenced somehow, or ERuleContext
		 *       needs to keep its own CamelSession reference. */
		g_signal_connect (
			context, "rule_removed",
			G_CALLBACK (context_rule_removed), session);

		config_dir = mail_session_get_config_dir ();
		user = g_build_filename (config_dir, "vfolders.xml", NULL);
		e_rule_context_save ((ERuleContext *) context, user);
		g_free (user);

		g_object_unref (session);
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

	d (printf ("Folder renamed to '%s' from '%s'\n", info->full_name, old_name));

	/* Folder is already renamed? */
	G_LOCK (vfolder);
	d (printf ("Changing folder name in hash table to '%s'\n", info->full_name));
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
		g_warning ("couldn't find a vfolder rule in our table? %s", info->full_name);
	}
}

static void
folder_available_cb (MailFolderCache *cache,
                     CamelStore *store,
                     const gchar *folder_name)
{
	mail_vfolder_note_folder (store, folder_name, FALSE);
}

static void
folder_unavailable_cb (MailFolderCache *cache,
                       CamelStore *store,
                       const gchar *folder_name)
{
	mail_vfolder_note_folder (store, folder_name, TRUE);
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

static gboolean glob_thread_subject = FALSE;

static void
mail_vfolder_thread_subject_changed_cb (GSettings *settings,
					const gchar *key,
					gpointer user_data)
{
	gboolean thread_subject;

	thread_subject = g_settings_get_boolean (settings, key);

	/* maybe it changed, verify it did */
	if ((!thread_subject) != (!glob_thread_subject)) {
		glob_thread_subject = thread_subject;

		if (context && !vfolder_shutdown) {
			EFilterRule *rule;
			GSList *rules = NULL, *link;

			G_LOCK (vfolder);

			rule = NULL;
			while ((rule = e_rule_context_next_rule ((ERuleContext *) context, rule, NULL))) {
				if (rule->name && rule->threading != E_FILTER_THREAD_NONE)
					rules = g_slist_prepend (rules, g_object_ref (rule));
			}

			G_UNLOCK (vfolder);

			for (link = rules; link; link = g_slist_next (link)) {
				rule = link->data;

				e_filter_rule_emit_changed (rule);
			}

			g_slist_free_full (rules, g_object_unref);
		}
	}
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
	GSettings *settings;
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
	/* This needs editor context which is only in the mail/. But really to run here we don't need editor context.
	 * So till we split this to EDS, we would let mail/ create this and later one it is any ways two separate
	 * contexts. */
	context = e_mail_session_create_vfolder_context (session);

	xmlfile = g_build_filename (EVOLUTION_PRIVDATADIR, "vfoldertypes.xml", NULL);
	if (e_rule_context_load ((ERuleContext *) context,
			       xmlfile, user) != 0) {
		g_warning ("cannot load vfolders: %s\n", ((ERuleContext *) context)->error);
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
			d (printf ("rule added: %s\n", rule->name));
			context_rule_added ((ERuleContext *) context, rule, session);
		} else {
			d (printf ("invalid rule (%p) encountered: rule->name is NULL\n", rule));
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

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_signal_connect_object (settings, "changed::thread-subject",
		G_CALLBACK (mail_vfolder_thread_subject_changed_cb), context, 0);
	glob_thread_subject = g_settings_get_boolean (settings, "thread-subject");
	g_clear_object (&settings);
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

	g_clear_object (&context);
}
