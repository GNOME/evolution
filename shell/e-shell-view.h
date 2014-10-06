/*
 * e-shell-view.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_SHELL_VIEW_H
#define E_SHELL_VIEW_H

#include <libedataserver/libedataserver.h>

#include <e-util/e-util.h>

#include <shell/e-shell-common.h>
#include <shell/e-shell-backend.h>
#include <shell/e-shell-content.h>
#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-taskbar.h>
#include <shell/e-shell-window.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_VIEW \
	(e_shell_view_get_type ())
#define E_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_VIEW, EShellView))
#define E_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_VIEW, EShellViewClass))
#define E_IS_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_VIEW))
#define E_IS_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_VIEW))
#define E_SHELL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_VIEW, EShellViewClass))

G_BEGIN_DECLS

typedef struct _EShellView EShellView;
typedef struct _EShellViewClass EShellViewClass;
typedef struct _EShellViewPrivate EShellViewPrivate;

/**
 * EShellView:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellView {
	GObject parent;
	EShellViewPrivate *priv;
};

/**
 * EShellViewClass:
 * @parent_class:	The parent class structure.
 * @label:		The initial value for the switcher action's
 *			#GtkAction:label property.  See
 *			e_shell_view_get_action().
 * @icon_name:		The initial value for the switcher action's
 *			#GtkAction:icon-name property.  See
 *			e_shell_view_get_action().
 * @ui_definition:	Base name of the UI definintion file to add
 *			when the shell view is activated.
 * @ui_manager_id:	The #GtkUIManager ID for #EPluginUI.  Plugins
 *			should use to this ID in their "eplug" files to
 *			add menu and toolbar items to the shell view.
 * @search_context_type:GType of the search context, which should be an
 *			instance of ERuleContextClass or a custom subclass.
 * @search_context:	A unique @search_context_type instance is created
 *			automatically for each subclass and shared across
 *			all instances of that subclass.
 * @search_options:	Widget path in the UI definition to the search
 *			options popup menu.  The menu gets shown when the
 *			user clicks the "find" icon in the search entry.
 * @search_rules:	Base name of the XML file containing predefined
 *			search rules for this shell view.  The XML files
 *			are usually named something like <filename>
 *			<emphasis>view</emphasis>types.xml</filename>.
 * @view_collection:	A unique #GalViewCollection instance is created
 *			for each subclass and shared across all instances
 *			of that subclass.  That much is done automatically
 *			for subclasses, but subclasses are still responsible
 *			for adding the appropriate #GalView factories to the
 *			view collection.
 * @shell_backend:	The corresponding #EShellBackend for the shell view.
 * @new_shell_content:	Factory method for the shell view's #EShellContent.
 *			See e_shell_view_get_shell_content().
 * @new_shell_sidebar:	Factory method for the shell view's #EShellSidebar.
 *			See e_shell_view_get_shell_sidebar().
 * @new_shell_taskbar:	Factory method for the shell view's #EShellTaskbar.
 *			See e_shell_view_get_shell_taskbar().
 * @new_shell_searchbar:
 *			Factory method for the shell view's #EShellSearchbar.
 *			See e_shell_view_get_searchbar().
 * @construct_searchbar:
 *			Class method to create, configure and pack a search
 *			bar widget.  The search bar differs in normal shell
 *			mode versus "express" mode.
 * @get_search_name:	Class method to obtain a suitable name for the
 *			current search criteria.  Subclasses should rarely
 *			need to override the default behavior.
 * @toggled:		Class method for the #EShellView::toggled signal.
 *			Subclasses should rarely need to override the
 *			default behavior.
 * @clear_search:	Class method for the #EShellView::clear-search
 *			signal.  The default method sets the
 *			#EShellView:search-rule to %NULL and then emits
 *			the #EShellView::execute-search signal.
 * @custom_search:	Class method for the #EShellView::custom-search
 *			signal.  This is emitted prior to executing an
 *			advanced or saved search.  The default method sets
 *			the #EShellView:search-rule property and then emits
 *			the #EShellView::execute-search signal.
 * @execute_search:	Class method for the #EShellView::execute-search
 *			signal.  There is no default behavior; subclasses
 *			should override this.
 * @update_actions:	Class method for the #EShellView::update-actions
 *			signal.  There is no default behavior; subclasses
 *			should override this.
 *
 * #EShellViewClass contains a number of important settings for subclasses.
 **/
struct _EShellViewClass {
	GObjectClass parent_class;

	/* Initial switcher action values. */
	const gchar *label;
	const gchar *icon_name;

	/* Base name of the UI definition file. */
	const gchar *ui_definition;

	/* GtkUIManager identifier for use with EPluginUI.
	 * Usually "org.gnome.evolution.$(VIEW_NAME)". */
	const gchar *ui_manager_id;

	/* Search context.  Subclasses may override the type.
	 * A unique instance is created for each subclass. */
	GType search_context_type;
	ERuleContext *search_context;

	/* Widget path to the search options popup menu. */
	const gchar *search_options;

	/* Base name of the search rule definition file. */
	const gchar *search_rules;

	/* A unique instance is created for each subclass. */
	GalViewCollection *view_collection;

	/* This is set by the corresponding EShellBackend. */
	EShellBackend *shell_backend;

	/* Factory Methods */
	GtkWidget *	(*new_shell_content)	(EShellView *shell_view);
	GtkWidget *	(*new_shell_sidebar)	(EShellView *shell_view);
	GtkWidget *	(*new_shell_taskbar)	(EShellView *shell_view);
	GtkWidget *	(*new_shell_searchbar)	(EShellView *shell_view);

	/* Create, configure and pack a search bar widget. */
	GtkWidget *	(*construct_searchbar)	(EShellView *shell_view);
	gchar *		(*get_search_name)	(EShellView *shell_view);

	/* Signals */
	void		(*toggled)		(EShellView *shell_view);
	void		(*clear_search)		(EShellView *shell_view);
	void		(*custom_search)	(EShellView *shell_view,
						 EFilterRule *custom_rule);
	void		(*execute_search)	(EShellView *shell_view);
	void		(*update_actions)	(EShellView *shell_view);
};

GType		e_shell_view_get_type		(void);
const gchar *	e_shell_view_get_name		(EShellView *shell_view);
GtkAction *	e_shell_view_get_action		(EShellView *shell_view);
const gchar *	e_shell_view_get_title		(EShellView *shell_view);
void		e_shell_view_set_title		(EShellView *shell_view,
						 const gchar *title);
const gchar *	e_shell_view_get_view_id	(EShellView *shell_view);
void		e_shell_view_set_view_id	(EShellView *shell_view,
						 const gchar *view_id);
GalViewInstance *
		e_shell_view_new_view_instance	(EShellView *shell_view,
						 const gchar *instance_id);
GalViewInstance *
		e_shell_view_get_view_instance	(EShellView *shell_view);
void		e_shell_view_set_view_instance	(EShellView *shell_view,
						 GalViewInstance *view_instance);
gboolean	e_shell_view_is_active		(EShellView *shell_view);
gint		e_shell_view_get_page_num	(EShellView *shell_view);
void		e_shell_view_set_page_num	(EShellView *shell_view,
						 gint page_num);
GtkWidget *	e_shell_view_get_searchbar	(EShellView *shell_view);
gchar *		e_shell_view_get_search_name	(EShellView *shell_view);
EFilterRule *	e_shell_view_get_search_rule	(EShellView *shell_view);
void		e_shell_view_set_search_rule	(EShellView *shell_view,
						 EFilterRule *search_rule);
gchar *		e_shell_view_get_search_query	(EShellView *shell_view);
GtkSizeGroup *	e_shell_view_get_size_group	(EShellView *shell_view);
EShellBackend *	e_shell_view_get_shell_backend	(EShellView *shell_view);
EShellContent *	e_shell_view_get_shell_content	(EShellView *shell_view);
EShellSidebar *	e_shell_view_get_shell_sidebar	(EShellView *shell_view);
EShellTaskbar *	e_shell_view_get_shell_taskbar	(EShellView *shell_view);
EShellWindow *	e_shell_view_get_shell_window	(EShellView *shell_view);
GKeyFile *	e_shell_view_get_state_key_file	(EShellView *shell_view);
void		e_shell_view_set_state_dirty	(EShellView *shell_view);
void		e_shell_view_clear_search	(EShellView *shell_view);
void		e_shell_view_custom_search	(EShellView *shell_view,
						 EFilterRule *custom_rule);
void		e_shell_view_execute_search	(EShellView *shell_view);
void		e_shell_view_block_execute_search
						(EShellView *shell_view);
void		e_shell_view_unblock_execute_search
						(EShellView *shell_view);
gboolean	e_shell_view_is_execute_search_blocked
						(EShellView *shell_view);
void		e_shell_view_update_actions	(EShellView *shell_view);
void		e_shell_view_update_actions_in_idle
						(EShellView *shell_view);
GtkWidget *	e_shell_view_show_popup_menu	(EShellView *shell_view,
						 const gchar *widget_path,
						 GdkEvent *button_event);
void		e_shell_view_write_source	(EShellView *shell_view,
						 ESource *source);
void		e_shell_view_remove_source	(EShellView *shell_view,
						 ESource *source);
void		e_shell_view_remote_delete_source
						(EShellView *shell_view,
						 ESource *source);

EActivity *	e_shell_view_submit_thread_job	(EShellView *shell_view,
						 const gchar *description,
						 const gchar *alert_ident,
						 const gchar *alert_arg_0,
						 EAlertSinkThreadJobFunc func,
						 gpointer user_data,
						 GDestroyNotify free_user_data);

G_END_DECLS

#endif /* E_SHELL_VIEW_H */
