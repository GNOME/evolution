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

#include <config.h>
#include <pthread.h>
#include <string.h>

#include <glib.h>

#include <glib/gi18n.h>

#include <camel/camel-vee-folder.h>
#include <camel/camel-vee-store.h>
#include <camel/camel-vtrash-folder.h>

#include "libedataserver/e-account-list.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"

#include "em-folder-tree-model.h"
#include "em-utils.h"
#include "em-vfolder-context.h"
#include "em-vfolder-editor.h"
#include "em-vfolder-rule.h"
#include "mail-autofilter.h"
#include "mail-component.h"
#include "mail-config.h"
#include "mail-folder-cache.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-vfolder.h"

#define d(x)  /* (printf("%s:%s: ",  G_STRLOC, G_STRFUNC), (x))*/

static EMVFolderContext *context;	/* context remains open all time */
CamelStore *vfolder_store; /* the 1 static vfolder store */

/* lock for accessing shared resources (below) */
static pthread_mutex_t vfolder_lock = PTHREAD_MUTEX_INITIALIZER;

static GList *source_folders_remote;	/* list of source folder uri's - remote ones */
static GList *source_folders_local;	/* list of source folder uri's - local ones */
static GHashTable *vfolder_hash;
/* This is a slightly hacky solution to shutting down, we poll this variable in various
   loops, and just quit processing if it is set. */
static volatile gint shutdown;		/* are we shutting down? */
/* more globals ... */
extern CamelSession *session;

static void rule_changed(FilterRule *rule, CamelFolder *folder);

#define LOCK() pthread_mutex_lock(&vfolder_lock);
#define UNLOCK() pthread_mutex_unlock(&vfolder_lock);

/* ********************************************************************** */

struct _setup_msg {
	MailMsg base;

	CamelFolder *folder;
	gchar *query;
	GList *sources_uri;
	GList *sources_folder;
};

static gchar *
vfolder_setup_desc (struct _setup_msg *m)
{
	return g_strdup_printf(_("Setting up Search Folder: %s"), m->folder->full_name);
}

static void
vfolder_setup_exec (struct _setup_msg *m)
{
	GList *l, *list = NULL;
	CamelFolder *folder;

	d(printf("Setting up Search Folder: %s\n", m->folder->full_name));

	camel_vee_folder_set_expression((CamelVeeFolder *)m->folder, m->query);

	l = m->sources_uri;
	while (l && !shutdown) {
		d(printf(" Adding uri: %s\n", (gchar *)l->data));

		folder = mail_tool_uri_to_folder (l->data, 0, &m->base.ex);
		if (folder) {
			list = g_list_append(list, folder);
		} else {
			g_warning("Could not open vfolder source: %s", (gchar *)l->data);
			camel_exception_clear(&m->base.ex);
		}
		l = l->next;
	}

	l = m->sources_folder;
	while (l && !shutdown) {
		d(printf(" Adding folder: %s\n", ((CamelFolder *)l->data)->full_name));
		camel_object_ref(l->data);
		list = g_list_append(list, l->data);
		l = l->next;
	}

	if (!shutdown)
		camel_vee_folder_set_folders((CamelVeeFolder *)m->folder, list);

	l = list;
	while (l) {
		camel_object_unref(l->data);
		l = l->next;
	}
	g_list_free(list);
}

static void
vfolder_setup_done (struct _setup_msg *m)
{
}

static void
vfolder_setup_free (struct _setup_msg *m)
{
	GList *l;

	camel_object_unref(m->folder);
	g_free(m->query);

	l = m->sources_uri;
	while (l) {
		g_free(l->data);
		l = l->next;
	}
	g_list_free(m->sources_uri);

	l = m->sources_folder;
	while (l) {
		camel_object_unref(l->data);
		l = l->next;
	}
	g_list_free(m->sources_folder);
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
vfolder_setup(CamelFolder *folder, const gchar *query, GList *sources_uri, GList *sources_folder)
{
	struct _setup_msg *m;
	gint id;

	m = mail_msg_new(&vfolder_setup_info);
	m->folder = folder;
	camel_object_ref(folder);
	m->query = g_strdup(query);
	m->sources_uri = sources_uri;
	m->sources_folder = sources_folder;

	id = m->base.seq;
	mail_msg_slow_ordered_push (m);

	return id;
}

/* ********************************************************************** */

struct _adduri_msg {
	MailMsg base;

	gchar *uri;
	GList *folders;
	gint remove;
};

static gchar *
vfolder_adduri_desc (struct _adduri_msg *m)
{
	gchar *euri, *desc = NULL;

	/* Yuck yuck.  Lookup the account name and use that to describe the path */
	/* We really need to normalise this across all of camel and evolution :-/ */
	euri = em_uri_from_camel(m->uri);
	if (euri) {
		CamelURL *url = camel_url_new(euri, NULL);

		if (url) {
			const gchar *loc = NULL;

			if (url->host && !strcmp(url->host, "local")
			    && url->user && !strcmp(url->user, "local")) {
				loc = _("On This Computer");
			} else {
				gchar *uid;
				const EAccount *account;

				if (url->user == NULL)
					uid = g_strdup(url->host);
				else
					uid = g_strdup_printf("%s@%s", url->user, url->host);

				account = e_account_list_find(mail_config_get_accounts(), E_ACCOUNT_FIND_UID, uid);
				g_free(uid);
				if (account != NULL)
					loc = account->name;
			}

			if (loc && url->path)
				desc = g_strdup_printf(_("Updating Search Folders for '%s:%s'"), loc, url->path);
			camel_url_free(url);
		}
		g_free(euri);
	}

	if (desc == NULL)
		desc = g_strdup_printf(_("Updating Search Folders for '%s'"), m->uri);

	return desc;
}

static void
vfolder_adduri_exec (struct _adduri_msg *m)
{
	GList *l;
	CamelFolder *folder = NULL;

	if (shutdown)
		return;

	d(printf("%s uri to vfolder: %s\n", m->remove?"Removing":"Adding", m->uri));

	/* we dont try lookup the cache if we are removing it, its no longer there */

	if (!m->remove && !mail_note_get_folder_from_uri(m->uri, &folder)) {
		g_warning("Folder '%s' disappeared while I was adding/remove it to/from my vfolder", m->uri);
		return;
	}

	if (folder == NULL)
		folder = mail_tool_uri_to_folder (m->uri, 0, &m->base.ex);

	if (folder != NULL) {
		l = m->folders;
		while (l && !shutdown) {
			if (m->remove)
				camel_vee_folder_remove_folder((CamelVeeFolder *)l->data, folder);
			else
				camel_vee_folder_add_folder((CamelVeeFolder *)l->data, folder);
			l = l->next;
		}
		camel_object_unref(folder);
	}
}

static void
vfolder_adduri_done(struct _adduri_msg *m)
{
}

static void
vfolder_adduri_free (struct _adduri_msg *m)
{
	g_list_foreach(m->folders, (GFunc)camel_object_unref, NULL);
	g_list_free(m->folders);
	g_free(m->uri);
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
vfolder_adduri(const gchar *uri, GList *folders, gint remove)
{
	struct _adduri_msg *m;
	gint id;

	m = mail_msg_new(&vfolder_adduri_info);
	m->folders = folders;
	m->uri = g_strdup(uri);
	m->remove = remove;

	id = m->base.seq;
	mail_msg_slow_ordered_push (m);

	return id;
}

/* ********************************************************************** */

static GList *
mv_find_folder(GList *l, CamelStore *store, const gchar *uri)
{
	while (l) {
		if (camel_store_folder_uri_equal(store, l->data, uri))
			break;
		l = l->next;
	}
	return l;
}

/* uri is a camel uri */
static gint
uri_is_ignore(CamelStore *store, const gchar *uri)
{
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	gint found = FALSE;

	d(printf("checking '%s' against:\n  %s\n  %s\n  %s\n", uri,
		 mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_OUTBOX),
		 mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT),
		 mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS)));

	found = camel_store_folder_uri_equal(store, mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_OUTBOX), uri)
		|| camel_store_folder_uri_equal(store, mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT), uri)
		|| camel_store_folder_uri_equal(store, mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS), uri);

	if (found)
		return found;

	accounts = mail_config_get_accounts ();
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		gchar *curi;

		account = (EAccount *) e_iterator_get (iter);

		d(printf("checking sent_folder_uri '%s' == '%s'\n",
			 account->sent_folder_uri ? account->sent_folder_uri : "empty", uri));

		if (account->sent_folder_uri) {
			curi = em_uri_to_camel(account->sent_folder_uri);
			found = camel_store_folder_uri_equal(store, uri, curi);
			g_free(curi);
		}
		if (!found && account->drafts_folder_uri) {
			curi = em_uri_to_camel(account->drafts_folder_uri);
			found = camel_store_folder_uri_equal(store, uri, curi);
			g_free(curi);
		}

		if (found)
			break;

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	return found;
}

/* so special we never use it */
static gint
uri_is_spethal(CamelStore *store, const gchar *uri)
{
	CamelURL *url;
	gint res;

	/* This is a bit of a hack, but really the only way it can be done at the moment. */

	if ((store->flags & (CAMEL_STORE_VTRASH|CAMEL_STORE_VJUNK)) == 0)
		return FALSE;

	url = camel_url_new(uri, NULL);
	if (url == NULL)
		return TRUE;

	/* don't use strcasecmp here */
	if (url->fragment) {
		res = (((store->flags & CAMEL_STORE_VTRASH)
			&& strcmp(url->fragment, CAMEL_VTRASH_NAME) == 0)
		       || ((store->flags & CAMEL_STORE_VJUNK)
			   && strcmp(url->fragment, CAMEL_VJUNK_NAME) == 0));
	} else {
		res = url->path
			&& (((store->flags & CAMEL_STORE_VTRASH)
			     && strcmp(url->path, "/" CAMEL_VTRASH_NAME) == 0)
			    || ((store->flags & CAMEL_STORE_VJUNK)
				&& strcmp(url->path, "/" CAMEL_VJUNK_NAME) == 0));
	}

	camel_url_free(url);

	return res;
}

/* called when a new uri becomes (un)available */
void
mail_vfolder_add_uri(CamelStore *store, const gchar *curi, gint remove)
{
	FilterRule *rule;
	const gchar *source;
	CamelVeeFolder *vf;
	GList *folders = NULL, *link;
	gint remote = (((CamelService *)store)->provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0;
	gint is_ignore;
	gchar *uri;

	uri = em_uri_from_camel(curi);
	if (uri_is_spethal (store, curi)) {
		g_free(uri);
		return;
	}

	g_return_if_fail (mail_in_main_thread());

	is_ignore = uri_is_ignore(store, curi);

	LOCK();

/*	d(printf("%s uri to check: %s\n", remove?"Removing":"Adding", uri)); */

	/* maintain the source folders lists for changed rules later on */
	if (CAMEL_IS_VEE_STORE(store)) {
		is_ignore = TRUE;
	} else if (remove) {
		if (remote) {
			if ((link = mv_find_folder(source_folders_remote, store, curi)) != NULL) {
				g_free(link->data);
				source_folders_remote = g_list_remove_link(source_folders_remote, link);
			}
		} else {
			if ((link = mv_find_folder(source_folders_local, store, curi)) != NULL) {
				g_free(link->data);
				source_folders_local = g_list_remove_link(source_folders_local, link);
			}
		}
	} else if (!is_ignore) {
		/* we ignore drafts/sent/outbox here */
		if (remote) {
			if (mv_find_folder(source_folders_remote, store, curi) == NULL)
				source_folders_remote = g_list_prepend(source_folders_remote, g_strdup(curi));
		} else {
			if (mv_find_folder(source_folders_local, store, curi) == NULL)
				source_folders_local = g_list_prepend(source_folders_local, g_strdup(curi));
		}
	}

	if (context == NULL)
		goto done;

	rule = NULL;
	while ((rule = rule_context_next_rule((RuleContext *)context, rule, NULL))) {
		gint found = FALSE;

		if (!rule->name) {
			d(printf("invalid rule (%p): rule->name is set to NULL\n", rule));
			continue;
		}
		/* dont auto-add any sent/drafts folders etc, they must be explictly listed as a source */
		if (rule->source
		    && !is_ignore
		    && ((((EMVFolderRule *)rule)->with == EM_VFOLDER_RULE_WITH_LOCAL && !remote)
			|| (((EMVFolderRule *)rule)->with == EM_VFOLDER_RULE_WITH_REMOTE_ACTIVE && remote)
			|| (((EMVFolderRule *)rule)->with == EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE)))
			found = TRUE;

		source = NULL;
		while (!found && (source = em_vfolder_rule_next_source((EMVFolderRule *)rule, source))) {
			gchar *csource;
			csource = em_uri_to_camel(source);
			found = camel_store_folder_uri_equal(store, curi, csource);
			d(printf(found?" '%s' == '%s'?\n":" '%s' != '%s'\n", curi, csource));
			g_free(csource);
		}

		if (found) {
			vf = g_hash_table_lookup(vfolder_hash, rule->name);
			if (!vf) {
				g_warning ("vf is NULL for %s\n", rule->name);
				continue;
			}
			camel_object_ref(vf);
			folders = g_list_prepend(folders, vf);
		}
	}

done:
	UNLOCK();

	if (folders != NULL)
		vfolder_adduri(curi, folders, remove);

	g_free(uri);
}

/* called when a uri is deleted from a store */
void
mail_vfolder_delete_uri(CamelStore *store, const gchar *curi)
{
	FilterRule *rule;
	const gchar *source;
	CamelVeeFolder *vf;
	GString *changed;
	gchar *uri;
	GList *link;

	if (uri_is_spethal (store, curi))
		return;

	uri = em_uri_from_camel(curi);

	d(printf ("Deleting uri to check: %s\n", uri));

	g_return_if_fail (mail_in_main_thread());

	changed = g_string_new ("");

	LOCK();

	if (context == NULL)
		goto done;

	/* see if any rules directly reference this removed uri */
	rule = NULL;
	while ((rule = rule_context_next_rule ((RuleContext *) context, rule, NULL))) {

		if (!rule->name) {
			d(printf("invalid rule (%p): rule->name is set to NULL\n", rule));
			continue;
		}

		source = NULL;
		while ((source = em_vfolder_rule_next_source ((EMVFolderRule *) rule, source))) {
			gchar *csource = em_uri_to_camel(source);

			/* Remove all sources that match, ignore changed events though
			   because the adduri call above does the work async */
			if (camel_store_folder_uri_equal(store, curi, csource)) {
				vf = g_hash_table_lookup (vfolder_hash, rule->name);
				if (!vf) {
					g_warning ("vf is NULL for %s\n", rule->name);
					continue;
				}
				g_signal_handlers_disconnect_matched (rule, G_SIGNAL_MATCH_FUNC|G_SIGNAL_MATCH_DATA, 0,
								      0, NULL, rule_changed, vf);
				em_vfolder_rule_remove_source ((EMVFolderRule *)rule, source);
				g_signal_connect (rule, "changed", G_CALLBACK(rule_changed), vf);
				g_string_append_printf (changed, "    %s\n", rule->name);
				source = NULL;
			}
			g_free(csource);
		}
	}

done:
	if ((link = mv_find_folder(source_folders_remote, store, curi)) != NULL) {
		g_free(link->data);
		source_folders_remote = g_list_remove_link(source_folders_remote, link);
	}

	if ((link = mv_find_folder(source_folders_local, store, curi)) != NULL) {
		g_free(link->data);
		source_folders_local = g_list_remove_link(source_folders_local, link);
	}

	UNLOCK();

	if (changed->str[0]) {
		GtkWidget *dialog;
		gchar *user;

		dialog = e_error_new(NULL, "mail:vfolder-updated", changed->str, uri, NULL);
		em_utils_show_info_silent (dialog);

		user = g_strdup_printf ("%s/vfolders.xml",
					mail_component_peek_base_directory (mail_component_peek ()));
		rule_context_save ((RuleContext *) context, user);
		g_free (user);
	}

	g_string_free (changed, TRUE);

	g_free(uri);
}

/* called when a uri is renamed in a store */
void
mail_vfolder_rename_uri(CamelStore *store, const gchar *cfrom, const gchar *cto)
{
	FilterRule *rule;
	const gchar *source;
	CamelVeeFolder *vf;
	gint changed = 0;
	gchar *from, *to;

	d(printf("vfolder rename uri: %s to %s\n", cfrom, cto));

	if (context == NULL || uri_is_spethal(store, cfrom) || uri_is_spethal(store, cto))
		return;

	g_return_if_fail (mail_in_main_thread());

	from = em_uri_from_camel(cfrom);
	to = em_uri_from_camel(cto);

	LOCK();

	/* see if any rules directly reference this removed uri */
	rule = NULL;
	while ( (rule = rule_context_next_rule((RuleContext *)context, rule, NULL)) ) {
		source = NULL;
		while ( (source = em_vfolder_rule_next_source((EMVFolderRule *)rule, source)) ) {
			gchar *csource = em_uri_to_camel(source);

			/* Remove all sources that match, ignore changed events though
			   because the adduri call above does the work async */
			if (camel_store_folder_uri_equal(store, cfrom, csource)) {
				d(printf("Vfolder '%s' used '%s' ('%s') now uses '%s'\n", rule->name, source, from, to));
				vf = g_hash_table_lookup(vfolder_hash, rule->name);
				if (!vf) {
					g_warning ("vf is NULL for %s\n", rule->name);
					continue;
				}
				g_signal_handlers_disconnect_matched(rule, G_SIGNAL_MATCH_FUNC|G_SIGNAL_MATCH_DATA, 0,
								     0, NULL, rule_changed, vf);
				em_vfolder_rule_remove_source((EMVFolderRule *)rule, source);
				em_vfolder_rule_add_source((EMVFolderRule *)rule, to);
				g_signal_connect(rule, "changed", G_CALLBACK(rule_changed), vf);
				changed++;
				source = NULL;
			}
			g_free(csource);
		}
	}

	UNLOCK();

	if (changed) {
		gchar *user;

		d(printf("Vfolders updated from renamed folder\n"));
		user = g_strdup_printf("%s/vfolders.xml", mail_component_peek_base_directory (mail_component_peek ()));
		rule_context_save((RuleContext *)context, user);
		g_free(user);
	}

	g_free(from);
	g_free(to);
}

GList *
mail_vfolder_get_sources_local (void)
{
	return source_folders_local;
}

GList *
mail_vfolder_get_sources_remote (void)
{
	return source_folders_remote;
}

/* ********************************************************************** */

static void context_rule_added(RuleContext *ctx, FilterRule *rule);

static void
rule_add_sources(GList *l, GList **sources_folderp, GList **sources_urip)
{
	GList *sources_folder = *sources_folderp;
	GList *sources_uri = *sources_urip;
	CamelFolder *newfolder;

	while (l) {
		gchar *curi = em_uri_to_camel(l->data);

		if (mail_note_get_folder_from_uri(curi, &newfolder)) {
			if (newfolder)
				sources_folder = g_list_append(sources_folder, newfolder);
			else
				sources_uri = g_list_append(sources_uri, g_strdup(curi));
		}
		g_free(curi);
		l = l->next;
	}

	*sources_folderp = sources_folder;
	*sources_urip = sources_uri;
}

static void
rule_changed(FilterRule *rule, CamelFolder *folder)
{
	GList *sources_uri = NULL, *sources_folder = NULL;
	GString *query;

	/* if the folder has changed name, then add it, then remove the old manually */
	if (strcmp(folder->full_name, rule->name) != 0) {
		gchar *oldname;

		gpointer key;
		gpointer oldfolder;

		LOCK();
		d(printf("Changing folder name in hash table to '%s'\n", rule->name));
		if (g_hash_table_lookup_extended (vfolder_hash, folder->full_name, &key, &oldfolder)) {
			g_hash_table_remove (vfolder_hash, key);
			g_free (key);
			g_hash_table_insert (vfolder_hash, g_strdup(rule->name), folder);
			UNLOCK();
		} else {
			UNLOCK();
			g_warning("couldn't find a vfolder rule in our table? %s", folder->full_name);
		}

		/* TODO: make the folder->full_name var thread accessible */
		oldname = g_strdup(folder->full_name);
		camel_store_rename_folder(vfolder_store, oldname, rule->name, NULL);
		g_free(oldname);
	}

	d(printf("Filter rule changed? for folder '%s'!!\n", folder->name));

	/* find any (currently available) folders, and add them to the ones to open */
	rule_add_sources(((EMVFolderRule *)rule)->sources, &sources_folder, &sources_uri);

	LOCK();
	if (((EMVFolderRule *)rule)->with == EM_VFOLDER_RULE_WITH_LOCAL || ((EMVFolderRule *)rule)->with == EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE)
		rule_add_sources(source_folders_local, &sources_folder, &sources_uri);
	if (((EMVFolderRule *)rule)->with == EM_VFOLDER_RULE_WITH_REMOTE_ACTIVE || ((EMVFolderRule *)rule)->with == EM_VFOLDER_RULE_WITH_LOCAL_REMOTE_ACTIVE)
		rule_add_sources(source_folders_remote, &sources_folder, &sources_uri);
	UNLOCK();

	query = g_string_new("");
	filter_rule_build_code(rule, query);

	vfolder_setup(folder, query->str, sources_uri, sources_folder);

	g_string_free(query, TRUE);
}

static void context_rule_added(RuleContext *ctx, FilterRule *rule)
{
	CamelFolder *folder;

	d(printf("rule added: %s\n", rule->name));

	/* this always runs quickly */
	folder = camel_store_get_folder(vfolder_store, rule->name, 0, NULL);
	if (folder) {
		g_signal_connect(rule, "changed", G_CALLBACK(rule_changed), folder);

		LOCK();
		g_hash_table_insert(vfolder_hash, g_strdup(rule->name), folder);
		UNLOCK();

		rule_changed(rule, folder);
	}
}

static void context_rule_removed(RuleContext *ctx, FilterRule *rule)
{
	gchar *path;

	gpointer key, folder = NULL;

	d(printf("rule removed; %s\n", rule->name));

	/* TODO: remove from folder info cache? */

	/* FIXME: is this even necessary? if we remove the folder from
	 * the CamelStore, the tree should pick it up auto-magically
	 * because it listens to CamelStore events... */
	path = g_strdup_printf("/%s", rule->name);
	mail_component_remove_folder (mail_component_peek (), vfolder_store, path);
	g_free(path);

	LOCK();
	if (g_hash_table_lookup_extended (vfolder_hash, rule->name, &key, &folder)) {
		g_hash_table_remove (vfolder_hash, key);
		g_free (key);
	}
	UNLOCK();

	camel_store_delete_folder(vfolder_store, rule->name, NULL);
	/* this must be unref'd after its deleted */
	if (folder)
		camel_object_unref ((CamelFolder *) folder);
}

static void
store_folder_created(CamelObject *o, gpointer event_data, gpointer data)
{
	CamelStore *store = (CamelStore *)o;
	CamelFolderInfo *info = event_data;

	store = store;
	info = info;
}

static void
store_folder_deleted(CamelObject *o, gpointer event_data, gpointer data)
{
	CamelStore *store = (CamelStore *)o;
	CamelFolderInfo *info = event_data;
	FilterRule *rule;
	gchar *user;

	d(printf("Folder deleted: %s\n", info->name));
	store = store;

	/* Warning not thread safe, but might be enough */

	LOCK();

	/* delete it from our list */
	rule = rule_context_find_rule((RuleContext *)context, info->full_name, NULL);
	if (rule) {
		/* We need to stop listening to removed events, otherwise we'll try and remove it again */
		g_signal_handlers_disconnect_matched(context, G_SIGNAL_MATCH_FUNC|G_SIGNAL_MATCH_DATA, 0,
						     0, NULL, context_rule_removed, context);
		rule_context_remove_rule((RuleContext *)context, rule);
		g_object_unref(rule);
		g_signal_connect(context, "rule_removed", G_CALLBACK(context_rule_removed), context);

		user = g_strdup_printf("%s/vfolders.xml", mail_component_peek_base_directory (mail_component_peek ()));
		rule_context_save((RuleContext *)context, user);
		g_free(user);
	} else {
		g_warning("Cannot find rule for deleted vfolder '%s'", info->name);
	}

	UNLOCK();
}

static void
store_folder_renamed(CamelObject *o, gpointer event_data, gpointer data)
{
	CamelRenameInfo *info = event_data;
	FilterRule *rule;
	gchar *user;

	gpointer key, folder;

	/* This should be more-or-less thread-safe */

	d(printf("Folder renamed to '%s' from '%s'\n", info->new->full_name, info->old_base));

	/* Folder is already renamed? */
	LOCK();
	d(printf("Changing folder name in hash table to '%s'\n", info->new->full_name));
	if (g_hash_table_lookup_extended (vfolder_hash, info->old_base, &key, &folder)) {
		g_hash_table_remove (vfolder_hash, key);
		g_free (key);
		g_hash_table_insert (vfolder_hash, g_strdup(info->new->full_name), folder);

		rule = rule_context_find_rule((RuleContext *)context, info->old_base, NULL);
		if (!rule) {
			UNLOCK ();
			g_warning ("Rule shouldn't be NULL\n");
			return;
		}

		g_signal_handlers_disconnect_matched(rule, G_SIGNAL_MATCH_FUNC|G_SIGNAL_MATCH_DATA, 0,
						     0, NULL, rule_changed, folder);
		filter_rule_set_name(rule, info->new->full_name);
		g_signal_connect(rule, "changed", G_CALLBACK(rule_changed), folder);

		user = g_strdup_printf("%s/vfolders.xml", mail_component_peek_base_directory (mail_component_peek ()));
		rule_context_save((RuleContext *)context, user);
		g_free(user);

		UNLOCK();
	} else {
		UNLOCK();
		g_warning("couldn't find a vfolder rule in our table? %s", info->new->full_name);
	}
}

void
vfolder_load_storage(void)
{
	/* lock for loading storage, it is safe to call it more than once */
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

	gchar *user, *storeuri;
	FilterRule *rule;
	gchar *xmlfile;
	GConfClient *gconf;

	pthread_mutex_lock (&lock);

	if (vfolder_hash) {
		/* we have already initialized */
		pthread_mutex_unlock (&lock);
		return;
	}

	vfolder_hash = g_hash_table_new(g_str_hash, g_str_equal);

	pthread_mutex_unlock (&lock);

	/* first, create the vfolder store, and set it up */
	storeuri = g_strdup_printf("vfolder:%s/vfolder", mail_component_peek_base_directory (mail_component_peek ()));
	vfolder_store = camel_session_get_store(session, storeuri, NULL);
	if (vfolder_store == NULL) {
		g_warning("Cannot open vfolder store - no vfolders available");
		return;
	}

	camel_object_hook_event(vfolder_store, "folder_created",
				(CamelObjectEventHookFunc)store_folder_created, NULL);
	camel_object_hook_event(vfolder_store, "folder_deleted",
				(CamelObjectEventHookFunc)store_folder_deleted, NULL);
	camel_object_hook_event(vfolder_store, "folder_renamed",
				(CamelObjectEventHookFunc)store_folder_renamed, NULL);

	d(printf("got store '%s' = %p\n", storeuri, vfolder_store));

	/* load our rules */
	user = g_strdup_printf ("%s/vfolders.xml", mail_component_peek_base_directory (mail_component_peek ()));
	context = em_vfolder_context_new ();

	xmlfile = g_build_filename (EVOLUTION_PRIVDATADIR, "vfoldertypes.xml", NULL);
	if (rule_context_load ((RuleContext *)context,
			       xmlfile, user) != 0) {
		g_warning("cannot load vfolders: %s\n", ((RuleContext *)context)->error);
	}
	g_free (xmlfile);
	g_free (user);

	g_signal_connect(context, "rule_added", G_CALLBACK(context_rule_added), context);
	g_signal_connect(context, "rule_removed", G_CALLBACK(context_rule_removed), context);

	/* load store to mail component */
	mail_component_load_store_by_uri (mail_component_peek (), storeuri, _("Search Folders"));

	/* and setup the rules we have */
	rule = NULL;
	while ( (rule = rule_context_next_rule((RuleContext *)context, rule, NULL)) ) {
		if (rule->name) {
			d(printf("rule added: %s\n", rule->name));
			context_rule_added((RuleContext *)context, rule);
		} else {
			d(printf("invalid rule (%p) encountered: rule->name is NULL\n", rule));
		}
	}

	g_free(storeuri);

	/* reenable the feature if required */
	gconf = mail_config_get_gconf_client();
	if (!gconf_client_get_bool (gconf, "/apps/evolution/mail/display/enable_vfolders", NULL))
		gconf_client_set_bool (gconf, "/apps/evolution/mail/display/enable_vfolders", TRUE, NULL);
}

void
vfolder_revert(void)
{
	gchar *user;

	d(printf("vfolder_revert\n"));
	user = g_strdup_printf("%s/vfolders.xml", mail_component_peek_base_directory (mail_component_peek ()));
	rule_context_revert((RuleContext *)context, user);
	g_free(user);
}

static GtkWidget *vfolder_editor = NULL;

static void
em_vfolder_editor_response (GtkWidget *dialog, gint button, gpointer data)
{
	gchar *user;

	user = g_strdup_printf ("%s/vfolders.xml", mail_component_peek_base_directory (mail_component_peek ()));

	switch (button) {
	case GTK_RESPONSE_OK:
		rule_context_save((RuleContext *)context, user);
		break;
	default:
		rule_context_revert((RuleContext *)context, user);
	}

	vfolder_editor = NULL;

	gtk_widget_destroy(dialog);

	g_free (user);
}

void
vfolder_edit (void)
{
	if (vfolder_editor) {
		gdk_window_raise (GTK_WIDGET (vfolder_editor)->window);
		return;
	}

	/* ensures vfolder is running */
	vfolder_load_storage ();

	vfolder_editor = GTK_WIDGET (em_vfolder_editor_new (context));
	gtk_window_set_title (GTK_WINDOW (vfolder_editor), _("Search Folders"));
	g_signal_connect(vfolder_editor, "response", G_CALLBACK(em_vfolder_editor_response), NULL);

	gtk_widget_show (vfolder_editor);
}

static void
edit_rule_response(GtkWidget *w, gint button, gpointer data)
{
	if (button == GTK_RESPONSE_OK) {
		gchar *user;
		FilterRule *rule = g_object_get_data (G_OBJECT (w), "rule");
		FilterRule *orig = g_object_get_data (G_OBJECT (w), "orig");

		filter_rule_copy(orig, rule);
		user = g_strdup_printf("%s/vfolders.xml", mail_component_peek_base_directory (mail_component_peek ()));
		rule_context_save((RuleContext *)context, user);
		g_free(user);
	}

	gtk_widget_destroy(w);
}

void
vfolder_edit_rule(const gchar *uri)
{
	GtkWidget *w;
	GtkDialog *gd;
	FilterRule *rule, *newrule;
	CamelURL *url;

	url = camel_url_new(uri, NULL);
	if (url && url->fragment
	    && (rule = rule_context_find_rule((RuleContext *)context, url->fragment, NULL))) {
		g_object_ref((GtkObject *)rule);
		newrule = filter_rule_clone(rule);

		w = filter_rule_get_widget((FilterRule *)newrule, (RuleContext *)context);

		gd = (GtkDialog *)gtk_dialog_new_with_buttons(_("Edit Search Folder"), NULL,
							      GTK_DIALOG_DESTROY_WITH_PARENT,
							      GTK_STOCK_CANCEL,
							      GTK_RESPONSE_CANCEL,
							      GTK_STOCK_OK,
							      GTK_RESPONSE_OK,
							      NULL);
		gtk_container_set_border_width (GTK_CONTAINER (gd), 6);
		gtk_box_set_spacing ((GtkBox *) gd->vbox, 6);
		gtk_dialog_set_default_response(gd, GTK_RESPONSE_OK);
		g_object_set(gd, "allow_shrink", FALSE, "allow_grow", TRUE, NULL);
		gtk_window_set_default_size (GTK_WINDOW (gd), 500, 500);
		gtk_box_pack_start((GtkBox *)gd->vbox, w, TRUE, TRUE, 0);
		gtk_widget_show((GtkWidget *)gd);
		g_object_set_data_full(G_OBJECT(gd), "rule", newrule, (GDestroyNotify)g_object_unref);
		g_object_set_data_full(G_OBJECT(gd), "orig", rule, (GDestroyNotify)g_object_unref);
		g_signal_connect(gd, "response", G_CALLBACK(edit_rule_response), NULL);
		gtk_widget_show((GtkWidget *)gd);
	} else {
		GtkWidget *w;
		/* TODO: we should probably just create it ... */
		w = e_error_new(NULL, "mail:vfolder-notexist", uri, NULL);
		em_utils_show_error_silent (w);
	}

	if (url)
		camel_url_free(url);
}

static void
new_rule_clicked(GtkWidget *w, gint button, gpointer data)
{
	if (button == GTK_RESPONSE_OK) {
		gchar *user;
		FilterRule *rule = g_object_get_data((GObject *)w, "rule");

		if (!filter_rule_validate(rule)) {
			/* no need to popup a dialog because the validate code does that. */
			return;
		}

		if (rule_context_find_rule ((RuleContext *)context, rule->name, rule->source)) {
			e_error_run((GtkWindow *)w, "mail:vfolder-notunique", rule->name, NULL);
			return;
		}

		g_object_ref(rule);
		rule_context_add_rule((RuleContext *)context, rule);
		user = g_strdup_printf("%s/vfolders.xml", mail_component_peek_base_directory (mail_component_peek ()));
		rule_context_save((RuleContext *)context, user);
		g_free(user);
	}

	gtk_widget_destroy(w);
}

FilterPart *
vfolder_create_part(const gchar *name)
{
	return rule_context_create_part((RuleContext *)context, name);
}

/* clones a filter/search rule into a matching vfolder rule (assuming the same system definitions) */
FilterRule *
vfolder_clone_rule(FilterRule *in)
{
	FilterRule *rule = (FilterRule *)em_vfolder_rule_new();
	xmlNodePtr xml;

	xml = filter_rule_xml_encode(in);
	filter_rule_xml_decode(rule, xml, (RuleContext *)context);
	xmlFreeNodeList(xml);

	return rule;
}

/* adds a rule with a gui */
void
vfolder_gui_add_rule(EMVFolderRule *rule)
{
	GtkWidget *w;
	GtkDialog *gd;

	/* this should be done before we call this function */
	vfolder_load_storage ();

	w = filter_rule_get_widget((FilterRule *)rule, (RuleContext *)context);

	gd = (GtkDialog *)gtk_dialog_new_with_buttons(_("New Search Folder"),
						      NULL,
						      GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_STOCK_CANCEL,
						      GTK_RESPONSE_CANCEL,
						      GTK_STOCK_OK,
						      GTK_RESPONSE_OK,
						      NULL);
	gtk_dialog_set_default_response(gd, GTK_RESPONSE_OK);
	gtk_container_set_border_width (GTK_CONTAINER (gd), 6);
	gtk_box_set_spacing ((GtkBox *) gd->vbox, 6);
	g_object_set(gd, "allow_shrink", FALSE, "allow_grow", TRUE, NULL);
	gtk_window_set_default_size (GTK_WINDOW (gd), 500, 500);
	gtk_box_pack_start((GtkBox *)gd->vbox, w, TRUE, TRUE, 0);
	gtk_widget_show((GtkWidget *)gd);
	g_object_set_data_full(G_OBJECT(gd), "rule", rule, (GDestroyNotify)g_object_unref);
	g_signal_connect(gd, "response", G_CALLBACK(new_rule_clicked), NULL);
	gtk_widget_show((GtkWidget *)gd);
}

void
vfolder_gui_add_from_message(CamelMimeMessage *msg, gint flags, const gchar *source)
{
	EMVFolderRule *rule;

	g_return_if_fail (msg != NULL);

	/* ensures vfolder is running */
	vfolder_load_storage ();

	rule = (EMVFolderRule*)em_vfolder_rule_from_message(context, msg, flags, source);
	vfolder_gui_add_rule(rule);
}

void
vfolder_gui_add_from_address(CamelInternetAddress *addr, gint flags, const gchar *source)
{
	EMVFolderRule *rule;

	g_return_if_fail (addr != NULL);

	/* ensures vfolder is running */
	vfolder_load_storage ();

	rule = (EMVFolderRule*)em_vfolder_rule_from_address(context, addr, flags, source);
	vfolder_gui_add_rule(rule);
}

static void
vfolder_foreach_cb (gpointer key, gpointer data, gpointer user_data)
{
	CamelFolder *folder = CAMEL_FOLDER (data);

	if (folder)
		camel_object_unref(folder);

	g_free (key);
}

void
mail_vfolder_shutdown (void)
{
	shutdown = 1;

	if (vfolder_hash) {
		g_hash_table_foreach (vfolder_hash, vfolder_foreach_cb, NULL);
		g_hash_table_destroy (vfolder_hash);
		vfolder_hash = NULL;
	}

	if (vfolder_store) {
		camel_object_unref (vfolder_store);
		vfolder_store = NULL;
	}

	if (context) {
		g_object_unref(context);
		context = NULL;
	}
}
