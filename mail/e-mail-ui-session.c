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

#include "e-mail-account-store.h"

#include "e-util/e-util.h"
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
#include "libemail-engine/mail-mt.h"
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
	EPhotoCache *photo_cache;
};

enum {
	PROP_0,
	PROP_ACCOUNT_STORE,
	PROP_LABEL_STORE,
	PROP_PHOTO_CACHE
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
	GList *button_captions;
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
	GList *iter;

	info_only = g_list_length (m->button_captions) <= 1;

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

		if (g_list_length (m->button_captions) <= 1)
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
	g_list_free_full (m->button_captions, g_free);
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

		case PROP_PHOTO_CACHE:
			g_value_set_object (
				value,
				e_mail_ui_session_get_photo_cache (
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

	if (priv->photo_cache != NULL) {
		g_object_unref (priv->photo_cache);
		priv->photo_cache = NULL;
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
	EClientCache *client_cache;
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

	client_cache = e_shell_get_client_cache (shell);
	priv->photo_cache = e_photo_cache_new (client_cache);

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
                              GList *button_captions,
                              GCancellable *cancellable)
{
	struct _user_message_msg *m;
	gint result = -1;

	m = mail_msg_new (&user_message_info);
	m->ismain = mail_in_main_thread ();
	m->type = type;
	m->prompt = g_strdup (prompt);
	m->done = e_flag_new ();
	m->button_captions = g_list_copy_deep (
		button_captions, (GCopyFunc) g_strdup, NULL);

	if (g_list_length (button_captions) > 1)
		mail_msg_ref (m);

	if (!cancellable)
		cancellable = m->base.cancellable;

	if (m->ismain)
		user_message_exec (m, cancellable, &m->base.error);
	else
		mail_msg_main_loop_push (m);

	if (g_list_length (button_captions) > 1) {
		e_flag_wait (m->done);
		result = m->result;
		mail_msg_unref (m);
	} else if (m->ismain)
		mail_msg_unref (m);

	return result;
}

CamelCertTrust
e_mail_ui_session_trust_prompt (CamelSession *session,
                                CamelService *service,
                                GTlsCertificate *certificate,
                                GTlsCertificateFlags errors)
{
	g_type_ensure (E_TYPE_MAIL_UI_SESSION);

	return CAMEL_SESSION_CLASS (e_mail_ui_session_parent_class)->
		trust_prompt (session, service, certificate, errors);
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

static gboolean
mail_ui_session_lookup_addressbook (CamelSession *session,
                                    const gchar *name)
{
	CamelInternetAddress *cia;
	gboolean known_address = FALSE;

	/* FIXME CamelSession's lookup_addressbook() method needs redone.
	 *       No GCancellable provided, no means of reporting an error. */

	if (!mail_config_get_lookup_book ())
		return FALSE;

	cia = camel_internet_address_new ();

	if (camel_address_decode (CAMEL_ADDRESS (cia), name) > 0) {
		GError *error = NULL;

		e_mail_ui_session_check_known_address_sync (
			E_MAIL_UI_SESSION (session), cia,
			mail_config_get_lookup_book_local_only (),
			NULL, &known_address, &error);

		if (error != NULL) {
			g_warning ("%s: %s", G_STRFUNC, error->message);
			g_error_free (error);
		}
	} else {
		g_warning (
			"%s: Failed to decode internet "
			"address '%s'", G_STRFUNC, name);
	}

	g_object_unref (cia);

	return known_address;
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
	session_class->lookup_addressbook = mail_ui_session_lookup_addressbook;

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

	g_object_class_install_property (
		object_class,
		PROP_PHOTO_CACHE,
		g_param_spec_object (
			"photo-cache",
			"Photo Cache",
			"Contact photo cache",
			E_TYPE_PHOTO_CACHE,
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

EPhotoCache *
e_mail_ui_session_get_photo_cache (EMailUISession *session)
{
	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), NULL);

	return session->priv->photo_cache;
}

void
e_mail_ui_session_add_activity (EMailUISession *session,
                                EActivity *activity)
{
	g_return_if_fail (E_IS_MAIL_UI_SESSION (session));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	g_signal_emit (session, signals[ACTIVITY_ADDED], 0, activity);
}

/**
 * e_mail_ui_session_check_known_address_sync:
 * @session: an #EMailUISession
 * @addr: a #CamelInternetAddress
 * @check_local_only: only check the builtin address book
 * @cancellable: optional #GCancellable object, or %NULL
 * @out_known_address: return location for the determination of
 *                     whether @addr is a known address
 * @error: return location for a #GError, or %NULL
 *
 * Determines whether @addr is a known email address by querying address
 * books for contacts with a matching email address.  If @check_local_only
 * is %TRUE then only the builtin address book is checked, otherwise all
 * enabled address books are checked.
 *
 * The result of the query is returned through the @out_known_address
 * boolean pointer, not through the return value.  The return value only
 * indicates whether the address book queries were completed successfully.
 * If an error occurred, the function sets @error and returns %FALSE.
 *
 * Returns: whether address books were successfully queried
 **/
gboolean
e_mail_ui_session_check_known_address_sync (EMailUISession *session,
                                            CamelInternetAddress *addr,
                                            gboolean check_local_only,
                                            GCancellable *cancellable,
                                            gboolean *out_known_address,
                                            GError **error)
{
	EPhotoCache *photo_cache;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	EBookQuery *book_query;
	GList *list, *link;
	const gchar *email_address = NULL;
	gchar *book_query_string;
	gboolean known_address = FALSE;
	gboolean success = TRUE;

	g_return_val_if_fail (E_IS_MAIL_UI_SESSION (session), FALSE);
	g_return_val_if_fail (CAMEL_IS_INTERNET_ADDRESS (addr), FALSE);

	camel_internet_address_get (addr, 0, NULL, &email_address);
	g_return_val_if_fail (email_address != NULL, FALSE);

	/* XXX EPhotoCache holds a reference on EClientCache, which
	 *     we need.  EMailUISession should probably hold its own
	 *     EClientCache reference, but this will do for now. */
	photo_cache = e_mail_ui_session_get_photo_cache (session);
	client_cache = e_photo_cache_ref_client_cache (photo_cache);
	registry = e_client_cache_ref_registry (client_cache);

	book_query = e_book_query_field_test (
		E_CONTACT_EMAIL, E_BOOK_QUERY_IS, email_address);
	book_query_string = e_book_query_to_string (book_query);
	e_book_query_unref (book_query);

	if (check_local_only) {
		ESource *source;

		source = e_source_registry_ref_builtin_address_book (registry);
		list = g_list_prepend (NULL, g_object_ref (source));
		g_object_unref (source);
	} else {
		list = e_source_registry_list_sources (
			registry, E_SOURCE_EXTENSION_ADDRESS_BOOK);
	}

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		EClient *client;
		GSList *uids = NULL;

		/* Skip disabled sources. */
		if (!e_source_get_enabled (source))
			continue;

		client = e_client_cache_get_client_sync (
			client_cache, source,
			E_SOURCE_EXTENSION_ADDRESS_BOOK,
			cancellable, error);

		if (client == NULL) {
			success = FALSE;
			break;
		}

		success = e_book_client_get_contacts_uids_sync (
			E_BOOK_CLIENT (client), book_query_string,
			&uids, cancellable, error);

		g_object_unref (client);

		if (!success) {
			g_warn_if_fail (uids == NULL);
			break;
		}

		if (uids != NULL) {
			g_slist_free_full (uids, (GDestroyNotify) g_free);
			known_address = TRUE;
			break;
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_free (book_query_string);

	g_object_unref (registry);
	g_object_unref (client_cache);

	if (success && out_known_address != NULL)
		*out_known_address = known_address;

	return success;
}

