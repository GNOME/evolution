/*
 * e-mail-ui-session.c
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
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

/* mail-session.c: handles the session information and resource manipulation */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#include <libebackend/libebackend.h>
#include <libedataserverui/libedataserverui.h>

#include "e-mail-account-store.h"

#include "e-util/e-util.h"
#include "libevolution-utils/e-alert-dialog.h"
#include "e-util/e-util-private.h"

#include "shell/e-shell.h"
#include "shell/e-shell-view.h"
#include "shell/e-shell-content.h"
#include "shell/e-shell-window.h"

#include "libemail-engine/e-mail-folder-utils.h"
#include "libemail-engine/e-mail-junk-filter.h"
#include "libemail-engine/e-mail-session.h"
#include "e-mail-ui-session.h"
#include "em-composer-utils.h"
#include "em-filter-context.h"
#include "em-vfolder-editor-context.h"
#include "em-filter-rule.h"
#include "em-utils.h"
#include "libemail-engine/mail-config.h"
#include "libemail-utils/mail-mt.h"
#include "libemail-engine/mail-ops.h"
#include "mail-send-recv.h"
#include "libemail-engine/mail-tools.h"

#define E_MAIL_UI_SESSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_UI_SESSION, EMailUISessionPrivate))

typedef struct _SourceContext SourceContext;

struct _EMailUISessionPrivate {
	FILE *filter_logfile;
	ESourceRegistry *registry;
	EMailAccountStore *account_store;
	EMailLabelListStore *label_store;
};

enum {
	PROP_0,
	PROP_ACCOUNT_STORE,
	PROP_LABEL_STORE
};

enum {
	ACTIVITY_ADDED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (
	EMailUISession,
	e_mail_ui_session,
	E_TYPE_MAIL_SESSION,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

struct _SourceContext {
	EMailUISession *session;
	CamelService *service;
};

/* Support for CamelSession.alert_user() *************************************/

static gpointer user_message_dialog;
static GQueue user_message_queue = { NULL, NULL, 0 };

struct _user_message_msg {
	MailMsg base;

	CamelSessionAlertType type;
	gchar *prompt;
	GSList *button_captions;
	EFlag *done;

	gint result;
	guint ismain : 1;
};

static void user_message_exec (struct _user_message_msg *m,
                               GCancellable *cancellable,
                               GError **error);

static void
user_message_response_free (GtkDialog *dialog,
                            gint button)
{
	struct _user_message_msg *m = NULL;

	gtk_widget_destroy ((GtkWidget *) dialog);

	user_message_dialog = NULL;

	/* check for pendings */
	if (!g_queue_is_empty (&user_message_queue)) {
		GCancellable *cancellable;

		m = g_queue_pop_head (&user_message_queue);
		cancellable = m->base.cancellable;
		user_message_exec (m, cancellable, &m->base.error);
		mail_msg_unref (m);
	}
}

/* clicked, send back the reply */
static void
user_message_response (GtkDialog *dialog,
                       gint button,
                       struct _user_message_msg *m)
{
	/* if !m or !button_captions, then we've already replied */
	if (m && m->button_captions) {
		m->result = button;
		e_flag_set (m->done);
	}

	user_message_response_free (dialog, button);
}

static void
user_message_exec (struct _user_message_msg *m,
                   GCancellable *cancellable,
                   GError **error)
{
	gboolean info_only;
	GtkWindow *parent;
	EShell *shell;
	const gchar *error_type;
	gint index;
	GSList *iter;

	info_only = g_slist_length (m->button_captions) <= 1;

	if (!m->ismain && user_message_dialog != NULL && !info_only) {
		g_queue_push_tail (&user_message_queue, mail_msg_ref (m));
		return;
	}

	switch (m->type) {
		case CAMEL_SESSION_ALERT_INFO:
			error_type = "system:simple-info";
			break;
		case CAMEL_SESSION_ALERT_WARNING:
			error_type = "system:simple-warning";
			break;
		case CAMEL_SESSION_ALERT_ERROR:
			error_type = "system:simple-error";
			break;
		default:
			error_type = NULL;
			g_return_if_reached ();
	}

	shell = e_shell_get_default ();

	/* try to find "mail" view to place the informational alert to */
	if (info_only) {
		GtkWindow *active_window;
		EShellWindow *shell_window;
		EShellView *shell_view;
		EShellContent *shell_content = NULL;

		/* check currently active window first, ... */
		active_window = e_shell_get_active_window (shell);
		if (active_window && E_IS_SHELL_WINDOW (active_window)) {
			if (E_IS_SHELL_WINDOW (active_window)) {
				shell_window = E_SHELL_WINDOW (active_window);
				shell_view = e_shell_window_peek_shell_view (shell_window, "mail");
				if (shell_view)
					shell_content = e_shell_view_get_shell_content (shell_view);
			}
		}

		if (!shell_content) {
			GList *list, *iter;

			list = gtk_application_get_windows (GTK_APPLICATION (shell));

			/* ...then iterate through all opened
			 * windows and pick one which has it */
			for (iter = list; iter != NULL && !shell_content; iter = g_list_next (iter)) {
				if (E_IS_SHELL_WINDOW (iter->data)) {
					shell_window = iter->data;
					shell_view = e_shell_window_peek_shell_view (shell_window, "mail");
					if (shell_view)
						shell_content = e_shell_view_get_shell_content (shell_view);
				}
			}
		}

		/* When no shell-content found, which might not happen,
		 * but just in case, process the information alert like
		 * usual, through an EAlertDialog machinery. */
		if (shell_content) {
			e_alert_submit (
				E_ALERT_SINK (shell_content),
				error_type, m->prompt, NULL);
			return;
		} else if (!m->ismain && user_message_dialog != NULL) {
			g_queue_push_tail (&user_message_queue, mail_msg_ref (m));
			return;
		}
	}

	/* Pull in the active window from the shell to get a parent window */
	parent = e_shell_get_active_window (shell);
	user_message_dialog = e_alert_dialog_new_for_args (
		parent, error_type, m->prompt, NULL);
	g_object_set (user_message_dialog, "resizable", TRUE, NULL);

	if (m->button_captions) {
		GtkWidget *action_area;
		GList *children, *child;

		/* remove all default buttons and keep only those requested */
		action_area = gtk_dialog_get_action_area (GTK_DIALOG (user_message_dialog));

		children = gtk_container_get_children (GTK_CONTAINER (action_area));
		for (child = children; child != NULL; child = child->next) {
			gtk_container_remove (GTK_CONTAINER (action_area), child->data);
		}

		g_list_free (children);
	}

	for (index = 0, iter = m->button_captions; iter; index++, iter = iter->next) {
		gtk_dialog_add_button (GTK_DIALOG (user_message_dialog), iter->data, index);
	}

	/* XXX This is a case where we need to be able to construct
	 *     custom EAlerts without a predefined XML definition. */
	if (m->ismain) {
		gint response;

		response = gtk_dialog_run (user_message_dialog);
		user_message_response (
			user_message_dialog, response, m);
	} else {
		gpointer user_data = m;

		if (g_slist_length (m->button_captions) <= 1)
			user_data = NULL;

		g_signal_connect (
			user_message_dialog, "response",
			G_CALLBACK (user_message_response), user_data);
		gtk_widget_show (user_message_dialog);
	}
}

static void
user_message_free (struct _user_message_msg *m)
{
	g_free (m->prompt);
	g_slist_free_full (m->button_captions, g_free);
	e_flag_free (m->done);
}

static MailMsgInfo user_message_info = {
	sizeof (struct _user_message_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) user_message_exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) user_message_free
};

/* Support for CamelSession.get_filter_driver () *****************************/

static CamelFolder *
get_folder (CamelFilterDriver *d,
            const gchar *uri,
            gpointer user_data,
            GError **error)
{
	EMailSession *session = E_MAIL_SESSION (user_data);

	/* FIXME Not passing a GCancellable here. */
	/* FIXME Need a camel_filter_driver_get_session(). */
	return e_mail_session_uri_to_folder_sync (
		session, uri, 0, NULL, error);
}

static gboolean
session_play_sound_cb (const gchar *filename)
{
#ifdef HAVE_CANBERRA
	if (filename != NULL && *filename != '\0')
		ca_context_play (
			ca_gtk_context_get (), 0,
			CA_PROP_MEDIA_FILENAME, filename,
			NULL);
	else
#endif
		gdk_beep ();

	return FALSE;
}

static void
session_play_sound (CamelFilterDriver *driver,
                    const gchar *filename,
                    gpointer user_data)
{
	g_idle_add_full (
		G_PRIORITY_DEFAULT_IDLE,
		(GSourceFunc) session_play_sound_cb,
		g_strdup (filename), (GDestroyNotify) g_free);
}

static void
session_system_beep (CamelFilterDriver *driver,
                     gpointer user_data)
{
	g_idle_add ((GSourceFunc) session_play_sound_cb, NULL);
}

static CamelFilterDriver *
main_get_filter_driver (CamelSession *session,
                        const gchar *type,
                        GError **error)
{
	EMailSession *ms = E_MAIL_SESSION (session);
	CamelFilterDriver *driver;
	EFilterRule *rule = NULL;
	const gchar *config_dir;
	gchar *user, *system;
	GSettings *settings;
	ERuleContext *fc;
	EMailUISessionPrivate *priv;

	priv = E_MAIL_UI_SESSION_GET_PRIVATE (session);

	settings = g_settings_new ("org.gnome.evolution.mail");

	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	fc = (ERuleContext *) em_filter_context_new (ms);
	e_rule_context_load (fc, system, user);
	g_free (system);
	g_free (user);

	driver = camel_filter_driver_new (session);
	camel_filter_driver_set_folder_func (driver, get_folder, session);

	if (g_settings_get_boolean (settings, "filters-log-actions")) {
		if (priv->filter_logfile == NULL) {
			gchar *filename;

			filename = g_settings_get_string (settings, "filters-log-file");
			if (filename) {
				priv->filter_logfile = g_fopen (filename, "a+");
				g_free (filename);
			}
		}

		if (priv->filter_logfile)
			camel_filter_driver_set_logfile (driver, priv->filter_logfile);
	}

	camel_filter_driver_set_shell_func (driver, mail_execute_shell_command, NULL);
	camel_filter_driver_set_play_sound_func (driver, session_play_sound, NULL);
	camel_filter_driver_set_system_beep_func (driver, session_system_beep, NULL);

	if ((!strcmp (type, E_FILTER_SOURCE_INCOMING) ||
		!strcmp (type, E_FILTER_SOURCE_JUNKTEST))
		&& camel_session_get_check_junk (session)) {

		/* implicit junk check as 1st rule */
		camel_filter_driver_add_rule (
			driver, "Junk check", "(junk-test)",
			"(begin (set-system-flag \"junk\"))");
	}

	if (strcmp (type, E_FILTER_SOURCE_JUNKTEST) != 0) {
		GString *fsearch, *faction;

		fsearch = g_string_new ("");
		faction = g_string_new ("");

		if (!strcmp (type, E_FILTER_SOURCE_DEMAND))
			type = E_FILTER_SOURCE_INCOMING;

		/* add the user-defined rules next */
		while ((rule = e_rule_context_next_rule (fc, rule, type))) {
			g_string_truncate (fsearch, 0);
			g_string_truncate (faction, 0);

			/* skip disabled rules */
			if (!rule->enabled)
				continue;

			e_filter_rule_build_code (rule, fsearch);
			em_filter_rule_build_action (
				EM_FILTER_RULE (rule), faction);
			camel_filter_driver_add_rule (
				driver, rule->name,
				fsearch->str, faction->str);
		}

		g_string_free (fsearch, TRUE);
		g_string_free (faction, TRUE);
	}

	g_object_unref (fc);

	g_object_unref (settings);

	return driver;
}

static void
source_context_free (SourceContext *context)
{
	if (context->session != NULL)
		g_object_unref (context->session);

	if (context->service != NULL)
		g_object_unref (context->service);

	g_slice_free (SourceContext, context);
}

static gboolean
mail_ui_session_add_service_cb (SourceContext *context)
{
	EMailAccountStore *store;

	/* The CamelService should be fully initialized by now. */
	store = e_mail_ui_session_get_account_store (context->session);
	e_mail_account_store_add_service (store, context->service);

	return FALSE;
}

static void
mail_ui_session_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_STORE:
			g_value_set_object (
				value,
				e_mail_ui_session_get_account_store (
				E_MAIL_UI_SESSION (object)));
			return;

		case PROP_LABEL_STORE:
			g_value_set_object (
				value,
				e_mail_ui_session_get_label_store (
				E_MAIL_UI_SESSION (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_ui_session_dispose (GObject *object)
{
	EMailUISessionPrivate *priv;

	priv = E_MAIL_UI_SESSION_GET_PRIVATE (object);

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->account_store != NULL) {
		e_mail_account_store_clear (priv->account_store);
		g_object_unref (priv->account_store);
		priv->account_store = NULL;
	}

	if (priv->label_store != NULL) {
		g_object_unref (priv->label_store);
		priv->label_store = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_ui_session_parent_class)->dispose (object);
}

static void
mail_ui_session_constructed (GObject *object)
{
	EMailUISessionPrivate *priv;
	EMFolderTreeModel *folder_tree_model;
	ESourceRegistry *registry;
	EMailSession *session;
	EShell *shell;

	session = E_MAIL_SESSION (object);
	shell = e_shell_get_default ();

	/* synchronize online state first, before any CamelService is created */
	g_object_bind_property (
		shell, "online",
		session, "online",
		G_BINDING_SYNC_CREATE);

	priv = E_MAIL_UI_SESSION_GET_PRIVATE (object);
	priv->account_store = e_mail_account_store_new (session);

	/* Keep our own reference to the ESourceRegistry so we
	 * can easily disconnect signal handlers in dispose(). */
	registry = e_mail_session_get_registry (session);
	priv->registry = g_object_ref (registry);

	/* XXX Make sure the folder tree model is created before we
	 *     add built-in CamelStores so it gets signals from the
	 *     EMailAccountStore.
	 *
	 * XXX This is creating a circular reference.  Perhaps the
	 *     model should only hold a weak pointer to EMailSession?
	 *
	 * FIXME EMailSession should just own the default instance.
	 */
	folder_tree_model = em_folder_tree_model_get_default ();
	em_folder_tree_model_set_session (folder_tree_model, session);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_ui_session_parent_class)->constructed (object);
}

static CamelService *
mail_ui_session_add_service (CamelSession *session,
                             const gchar *uid,
                             const gchar *protocol,
                             CamelProviderType type,
                             GError **error)
{
	CamelService *service;

	/* Chain up to parent's constructed() method. */
	service = CAMEL_SESSION_CLASS (e_mail_ui_session_parent_class)->
		add_service (session, uid, protocol, type, error);

	/* Inform the EMailAccountStore of the new CamelService
	 * from an idle callback so the service has a chance to
	 * fully initialize first. */
	if (CAMEL_IS_STORE (service)) {
		SourceContext *context;

		context = g_slice_new0 (SourceContext);
		context->session = g_object_ref (session);
		context->service = g_object_ref (service);

		/* Prioritize ahead of GTK+ redraws. */
		g_idle_add_full (
			G_PRIORITY_HIGH_IDLE,
			(GSourceFunc) mail_ui_session_add_service_cb,
			context, (GDestroyNotify) source_context_free);
	}

	return service;
}

static void
mail_ui_session_remove_service (CamelSession *session,
                                CamelService *service)
{
	EMailAccountStore *store;
	EMailUISession *ui_session;

	/* Passing a NULL parent window skips confirmation prompts. */
	ui_session = E_MAIL_UI_SESSION (session);
	store = e_mail_ui_session_get_account_store (ui_session);
	e_mail_account_store_remove_service (store, NULL, service);
}

gint
e_mail_ui_session_alert_user (CamelSession *session,
                              CamelSessionAlertType type,
                              const gchar *prompt,
                              GSList *button_captions)
{
	struct _user_message_msg *m;
	GCancellable *cancellable;
	gint result = -1;
	GSList *iter;

	m = mail_msg_new (&user_message_info);
	m->ismain = mail_in_main_thread ();
	m->type = type;
	m->prompt = g_strdup (prompt);
	m->done = e_flag_new ();
	m->button_captions = g_slist_copy (button_captions);

	for (iter = m->button_captions; iter; iter = iter->next)
		iter->data = g_strdup (iter->data);

	if (g_slist_length (button_captions) > 1)
		mail_msg_ref (m);

	cancellable = m->base.cancellable;

	if (m->ismain)
		user_message_exec (m, cancellable, &m->base.error);
	else
		mail_msg_main_loop_push (m);

	if (g_slist_length (button_captions) > 1) {
		e_flag_wait (m->done);
		result = m->result;
		mail_msg_unref (m);
	} else if (m->ismain)
		mail_msg_unref (m);

	return result;
}

static CamelFilterDriver *
mail_ui_session_get_filter_driver (CamelSession *session,
                                   const gchar *type,
                                   GError **error)
{
	return (CamelFilterDriver *) mail_call_main (
		MAIL_CALL_p_ppp, (MailMainFunc) main_get_filter_driver,
		session, type, error);
}

static void
mail_ui_session_refresh_service (EMailSession *session,
                                 CamelService *service)
{
	if (camel_session_get_online (CAMEL_SESSION (session)))
		mail_receive_service (service);
}

static EMVFolderContext *
mail_ui_session_create_vfolder_context (EMailSession *session)
{
	return (EMVFolderContext *) em_vfolder_editor_context_new (session);
}

static void
e_mail_ui_session_class_init (EMailUISessionClass *class)
{
	GObjectClass *object_class;
	CamelSessionClass *session_class;
	EMailSessionClass *mail_session_class;

	g_type_class_add_private (class, sizeof (EMailUISessionPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_ui_session_get_property;
	object_class->dispose = mail_ui_session_dispose;
	object_class->constructed = mail_ui_session_constructed;

	session_class = CAMEL_SESSION_CLASS (class);
	session_class->add_service = mail_ui_session_add_service;
	session_class->remove_service = mail_ui_session_remove_service;
	session_class->alert_user = e_mail_ui_session_alert_user;
	session_class->get_filter_driver = mail_ui_session_get_filter_driver;

	mail_session_class = E_MAIL_SESSION_CLASS (class);
	mail_session_class->create_vfolder_context = mail_ui_session_create_vfolder_context;
	mail_session_class->refresh_service = mail_ui_session_refresh_service;

	g_object_class_install_property (
		object_class,
		PROP_LABEL_STORE,
		g_param_spec_object (
			"label-store",
			"Label Store",
			"Mail label store",
			E_TYPE_MAIL_LABEL_LIST_STORE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	signals[ACTIVITY_ADDED] = g_signal_new (
		"activity-added",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailUISessionClass, activity_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);
}

static void
e_mail_ui_session_init (EMailUISession *session)
{
	session->priv = E_MAIL_UI_SESSION_GET_PRIVATE (session);
	session->priv->label_store = e_mail_label_list_store_new ();
}

EMailSession *
e_mail_ui_session_new (ESourceRegistry *registry)
{
	const gchar *user_data_dir;
	const gchar *user_cache_dir;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	user_data_dir = mail_session_get_data_dir ();
	user_cache_dir = mail_session_get_cache_dir ();

	return g_object_new (
		E_TYPE_MAIL_UI_SESSION,
		"registry", registry,
		"user-data-dir", user_data_dir,
		"user-cache-dir", user_cache_dir,
		NULL);
}

EMailAccountStore *
e_mail_ui_session_get_account_store (EMailUISession *session)
{
	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), NULL);

	return session->priv->account_store;
}

EMailLabelListStore *
e_mail_ui_session_get_label_store (EMailUISession *session)
{
	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), NULL);

	return session->priv->label_store;
}

void
e_mail_ui_session_add_activity (EMailUISession *session,
                                EActivity *activity)
{
	g_return_if_fail (E_IS_MAIL_UI_SESSION (session));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	g_signal_emit (session, signals[ACTIVITY_ADDED], 0, activity);
}

