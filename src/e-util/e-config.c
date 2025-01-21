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

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libedataserver/libedataserver.h>

#include "e-config.h"

#define d(x)

typedef GtkWidget *
		(*EConfigItemSectionFactoryFunc)
						(EConfig *ec,
						 EConfigItem *item,
						 GtkWidget *parent,
						 GtkWidget *old,
						 gint position,
						 gpointer data,
						 GtkWidget **real_frame);

struct _EConfigFactory {
	gchar *id;
	EConfigFactoryFunc func;
	gpointer user_data;
};

struct _menu_node {
	GSList *menu;
	EConfigItemsFunc free;
	gpointer data;
};

struct _widget_node {
	EConfig *config;

	struct _menu_node *context;
	EConfigItem *item;
	GtkWidget *widget; /* widget created by the factory, if any */
	GtkWidget *frame; /* if created by us */
	GtkWidget *real_frame; /* used for sections and section grids, this is the real GtkFrame (whereas "frame" above is the internal vbox/grid) */

	guint empty:1;		/* set if empty (i.e. hidden) */
};

struct _check_node {
	gchar *pageid;
	EConfigCheckFunc func;
	gpointer data;
};

struct _finish_page_node {
	gchar *pageid;
	gboolean is_finish;
	gint orig_type;
};

struct _EConfigPrivate {
	GList *menus;
	GList *widgets;
	GList *checks;
};

static GtkWidget *
		config_hook_section_factory	(EConfig *config,
						 EConfigItem *item,
						 GtkWidget *parent,
						 GtkWidget *old,
						 gint position,
						 gpointer data,
						 GtkWidget **real_frame);

enum {
	ABORT,
	COMMIT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EConfig, e_config, G_TYPE_OBJECT)

static void
check_node_free (struct _check_node *node)
{
	g_free (node->pageid);

	g_slice_free (struct _check_node, node);
}

static void
config_finalize (GObject *object)
{
	EConfig *self = E_CONFIG (object);
	GList *link;

	d (printf ("finalising EConfig %p\n", object));

	g_free (self->id);

	link = self->priv->menus;
	while (link != NULL) {
		struct _menu_node *node = link->data;

		if (node->free)
			node->free (E_CONFIG (object), node->menu, node->data);

		g_free (node);

		link = g_list_delete_link (link, link);
	}

	link = self->priv->widgets;
	while (link != NULL) {
		struct _widget_node *node = link->data;

		/* disconnect the ec_widget_destroyed function from the widget */
		if (node->widget)
			g_signal_handlers_disconnect_matched (
				node->widget, G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, node);

		g_free (node);

		link = g_list_delete_link (link, link);
	}

	g_list_free_full (self->priv->checks, (GDestroyNotify) check_node_free);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_config_parent_class)->finalize (object);
}

static void
config_target_free (EConfig *config,
                    EConfigTarget *target)
{
	g_free (target);
	g_object_unref (config);
}

static void
config_set_target (EConfig *config,
                   EConfigTarget *target)
{
	if (config->target != NULL)
		e_config_target_free (config, target);

	config->target = target;
}

static void
e_config_class_init (EConfigClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = config_finalize;

	class->set_target = config_set_target;
	class->target_free = config_target_free;

	signals[ABORT] = g_signal_new (
		"abort",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EConfigClass, abort),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[COMMIT] = g_signal_new (
		"commit",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EConfigClass, commit),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_config_init (EConfig *config)
{
	config->priv = e_config_get_instance_private (config);
}

/**
 * e_config_construct:
 * @config: The instance to initialise.
 * @id: The name of the configuration window this manager drives.
 *
 * Used by implementing classes to initialise base parameters.
 *
 * Return value: @config is returned.
 **/
EConfig *
e_config_construct (EConfig *config,
                    const gchar *id)
{
	config->id = g_strdup (id);

	return config;
}

/**
 * e_config_add_items:
 * @config: An initialised implementing instance of EConfig.
 * @items: A list of EConfigItem's to add to the configuration manager.
 * @freefunc: If supplied, called to free the item list (and/or items)
 * once they are no longer needed.
 * @data: Data for the callback methods.
 *
 * Add new EConfigItems to the configuration window.  Nothing will be
 * done with them until the widget is built.
 *
 * TODO: perhaps commit and abort should just be signals.
 **/
void
e_config_add_items (EConfig *ec,
                    GSList *items,
                    EConfigItemsFunc freefunc,
                    gpointer data)
{
	struct _menu_node *node;

	node = g_malloc (sizeof (*node));
	node->menu = items;
	node->free = freefunc;
	node->data = data;

	ec->priv->menus = g_list_append (ec->priv->menus, node);
}

/**
 * e_config_add_page_check:
 * @config: Initialised implemeting instance of EConfig.
 * @pageid: pageid to check.
 * @func: checking callback.
 * @data: user-data for the callback.
 *
 * Add a page-checking function callback.  It will be called to validate the
 * data in the given page or pages.  If @pageid is NULL then it will be called
 * to validate every page, or the whole configuration window.
 *
 * In the latter case, the pageid in the callback will be either the
 * specific page being checked, or NULL when the whole config window
 * is being checked.
 *
 * The page check function is used to validate input before allowing
 * the assistant to continue or the notebook to close.
 **/
void
e_config_add_page_check (EConfig *ec,
                         const gchar *pageid,
                         EConfigCheckFunc func,
                         gpointer data)
{
	struct _check_node *cn;

	cn = g_slice_new0 (struct _check_node);
	cn->pageid = g_strdup (pageid);
	cn->func = func;
	cn->data = data;

	ec->priv->checks = g_list_append (ec->priv->checks, cn);
}

static void
ec_add_static_items (EConfig *config)
{
	EConfigClass *class;
	GList *link;

	class = E_CONFIG_GET_CLASS (config);
	for (link = class->factories; link != NULL; link = link->next) {
		EConfigFactory *factory = link->data;

		if (factory->id == NULL || strcmp (factory->id, config->id) == 0)
			factory->func (config, factory->user_data);
	}
}

static gint
ep_cmp (gconstpointer ap,
        gconstpointer bp)
{
	struct _widget_node *a = *((gpointer *) ap);
	struct _widget_node *b = *((gpointer *) bp);

	return strcmp (a->item->path, b->item->path);
}

static void
ec_widget_destroyed (GtkWidget *widget,
                     struct _widget_node *node)
{
	/* Use our own function instead of gtk_widget_destroyed()
	 * so it's easier to trap EConfig widgets in a debugger. */

	node->widget = NULL;
}

static void
ec_rebuild (EConfig *config)
{
	EConfigPrivate *p = config->priv;
	struct _widget_node *sectionnode = NULL, *pagenode = NULL;
	GtkWidget *book = NULL, *page = NULL, *section = NULL, *root = NULL;
	gint pageno = 0, sectionno = 0, itemno = 0;
	gint n_visible_widgets = 0;
	GList *link;

	d (printf ("target changed, rebuilding:\n"));

	/* TODO: This code is pretty complex, and will probably just
	 * become more complex with time.  It could possibly be split
	 * into the two base types, but there would be a lot of code
	 * duplication */

	/* because rebuild destroys pages, and destroying active page causes crashes */
	for (link = p->widgets; link != NULL; link = g_list_next (link)) {
		struct _widget_node *wn = link->data;
		struct _EConfigItem *item = wn->item;
		const gchar *translated_label = NULL;
		GtkWidget *w;

		d (printf (" '%s'\n", item->path));

		if (item->label != NULL)
			translated_label = gettext (item->label);

		/* If the last section doesn't contain any visible widgets, hide it */
		if (sectionnode != NULL
		    && sectionnode->frame != NULL
		    && (item->type == E_CONFIG_PAGE
			|| item->type == E_CONFIG_SECTION
			|| item->type == E_CONFIG_SECTION_GRID)) {
			if ((sectionnode->empty = (itemno == 0 || n_visible_widgets == 0))) {
				if (sectionnode->real_frame)
					gtk_widget_hide (sectionnode->real_frame);

				if (sectionnode->frame)
					gtk_widget_hide (sectionnode->frame);

				sectionno--;
			} else {
				if (sectionnode->real_frame)
					gtk_widget_show (sectionnode->real_frame);

				if (sectionnode->frame)
					gtk_widget_show (sectionnode->frame);
			}

			d (printf ("%s section '%s' [sections=%d]\n", sectionnode->empty?"hiding":"showing", sectionnode->item->path, sectionno));
		}

		/* If the last page doesn't contain anything, hide it */
		if (pagenode != NULL
		    && pagenode->frame != NULL
		    && item->type == E_CONFIG_PAGE) {
			if ((pagenode->empty = sectionno == 0)) {
				gtk_widget_hide (pagenode->frame);
				pageno--;
			} else
				gtk_widget_show (pagenode->frame);
			d (printf ("%s page '%s' [section=%d]\n", pagenode->empty?"hiding":"showing", pagenode->item->path, pageno));
		}

		/* Now process the item */
		switch (item->type) {
		case E_CONFIG_INVALID:
			g_warn_if_reached ();
			break;
		case E_CONFIG_BOOK:
			/* This is used by the defining code to mark the
			 * type of the config window.  It is cross-checked
			 * with the code's defined type. */
			if (root != NULL) {
				g_warning ("EConfig book redefined at: %s", item->path);
				break;
			}

			if (wn->widget == NULL) {
				if (item->factory) {
					root = item->factory (
						config, item, NULL, wn->widget,
						0, wn->context->data);
				} else {
					root = gtk_notebook_new ();
					gtk_notebook_set_show_border (GTK_NOTEBOOK (root), FALSE);
					gtk_widget_show (root);
				}

				config->widget = root;
				wn->widget = root;
			} else {
				root = wn->widget;
			}

			book = root;

			page = NULL;
			pagenode = NULL;
			section = NULL;
			sectionnode = NULL;
			pageno = 0;
			sectionno = 0;
			break;
		case E_CONFIG_PAGE:
			/* The page is a VBox, stored in the notebook. */
			sectionno = 0;
			if (root == NULL) {
				g_warning ("EConfig page defined before container widget: %s", item->path);
				break;
			}

			if (item->factory) {
				page = item->factory (
					config, item, root, wn->frame,
					pageno, wn->context->data);
					wn->frame = page;
				if (page)
					gtk_notebook_reorder_child ((GtkNotebook *) book, page, pageno);
				if (page)
					sectionno = 1;
			} else if (wn->widget == NULL) {
				w = gtk_label_new_with_mnemonic (translated_label);
				gtk_widget_show (w);
				page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
				gtk_container_set_border_width ((GtkContainer *) page, 12);
				gtk_widget_show (page);
				gtk_notebook_insert_page ((GtkNotebook *) book, page, w, pageno);
				gtk_container_child_set (GTK_CONTAINER (book), page, "tab-fill", TRUE, "tab-expand", TRUE, NULL);
				wn->frame = page;
			} else
				page = wn->widget;

			d (printf ("page %d:%s widget %p\n", pageno, item->path, page));

			if (wn->widget && wn->widget != page) {
				d (printf ("destroy old widget for page '%s' (%p)\n", item->path, wn->widget));
				gtk_widget_destroy (wn->widget);
			}

			pageno++;
			pagenode = wn;
			section = NULL;
			sectionnode = NULL;
			wn->widget = page;
			if (page)
				g_signal_connect (
					page, "destroy",
					G_CALLBACK (ec_widget_destroyed), wn);
			break;
		case E_CONFIG_SECTION:
		case E_CONFIG_SECTION_GRID:
			/* The section factory is always called with
			 * the parent vbox object.  Even for assistant pages. */
			if (page == NULL) {
				/*g_warning("EConfig section '%s' has no parent page", item->path);*/
				section = NULL;
				wn->widget = NULL;
				wn->frame = NULL;
				goto nopage;
			}

			itemno = 0;
			n_visible_widgets = 0;

			d (printf ("Building section %s - '%s' - %s factory\n", item->path, item->label, item->factory ? "with" : "without"));

			if (item->factory) {
				/* For sections, we pass an extra argument to the usual EConfigItemFactoryFunc.
				 * If this is an automatically-generated section, that extra argument (real_frame from
				 * EConfigItemSectionFactoryFunc) will contain the actual GtkFrame upon returning.
				 */
				EConfigItemSectionFactoryFunc factory = (EConfigItemSectionFactoryFunc) item->factory;

				section = factory (
					config, item, page, wn->widget, 0,
					wn->context->data, &wn->real_frame);
				wn->frame = section;
				if (section)
					itemno = 1;

				if (factory != config_hook_section_factory) {
					/* This means there is a section that came from a user-specified factory,
					 * so we don't know what is inside the section.  In that case, we increment
					 * n_visible_widgets so that the section will not get hidden later (we don't know
					 * if the section is empty or not, so we cannot decide to hide it).
					 *
					 * For automatically-generated sections, we use a special config_hook_section_factory() -
					 * see config_hook_construct_item().
					 */
					n_visible_widgets++;
					d (printf ("  n_visible_widgets++ because there is a section factory -> frame=%p\n", section));
				}

				if (section
				    && ((item->type == E_CONFIG_SECTION && !GTK_IS_BOX (section))
					|| (item->type == E_CONFIG_SECTION_GRID && !GTK_IS_GRID (section))))
					g_warning ("EConfig section type is wrong");
			} else {
				GtkWidget *frame;
				GtkWidget *label = NULL;

				if (wn->frame) {
					d (printf ("Item %s, clearing generated section widget\n", wn->item->path));
					gtk_widget_destroy (wn->frame);
					wn->widget = NULL;
					wn->frame = NULL;
				}

				if (translated_label != NULL) {
					gchar *txt = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>", translated_label);

					label = g_object_new (
						gtk_label_get_type (),
						"label", txt,
						"use_markup", TRUE,
						"xalign", 0.0, NULL);
					g_free (txt);
				}

				if (item->type == E_CONFIG_SECTION)
					section = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
				else {
					section = gtk_grid_new ();
					gtk_grid_set_column_spacing (GTK_GRID (section), 6);
					gtk_grid_set_row_spacing (GTK_GRID (section), 6);
				}

				gtk_widget_set_margin_top (section, 6);
				gtk_widget_set_margin_start (section, 12);
				frame = g_object_new (
					gtk_frame_get_type (),
					"shadow_type", GTK_SHADOW_NONE,
					"label_widget", label,
					"child", section,
					NULL);
				gtk_widget_show_all (frame);
				gtk_box_pack_start ((GtkBox *) page, frame, TRUE, TRUE, 0);
				wn->frame = frame;
			}
		nopage:
			if (wn->widget && wn->widget != section) {
				d (printf ("destroy old widget for section '%s'\n", item->path));
				gtk_widget_destroy (wn->widget);
			}

			d (printf ("Item %s, setting section widget\n", wn->item->path));

			sectionno++;
			wn->widget = section;
			if (section)
				g_signal_connect (
					section, "destroy",
					G_CALLBACK (ec_widget_destroyed), wn);
			sectionnode = wn;
			break;
		case E_CONFIG_ITEM:
		case E_CONFIG_ITEM_GRID:
			/* generated sections never retain their widgets on a rebuild */
			if (sectionnode && sectionnode->item->factory == NULL)
				wn->widget = NULL;

			/* ITEMs are called with the section parent.
			 * The type depends on the section type,
			 * either a GtkGrid, or a GtkVBox */
			w = NULL;
			if (section == NULL) {
				wn->widget = NULL;
				wn->frame = NULL;
				g_warning ("EConfig item has no parent section: %s", item->path);
			} else if ((item->type == E_CONFIG_ITEM && !GTK_IS_BOX (section))
				 || (item->type == E_CONFIG_ITEM_GRID && !GTK_IS_GRID (section)))
				g_warning ("EConfig item parent type is incorrect: %s", item->path);
			else if (item->factory)
				w = item->factory (
					config, item, section, wn->widget,
					0, wn->context->data);

			if (wn->widget && wn->widget != w) {
				d (printf ("destroy old widget for item '%s'\n", item->path));
				gtk_widget_destroy (wn->widget);
			}

			wn->widget = w;
			if (w) {
				g_signal_connect (
					w, "destroy",
					G_CALLBACK (ec_widget_destroyed), wn);
				itemno++;

				if (gtk_widget_get_visible (w))
					n_visible_widgets++;
			}
			break;
		}
	}

	/* If the last section doesn't contain any visible widgets, hide it */
	if (sectionnode != NULL && sectionnode->frame != NULL) {
		d (printf ("Section %s - %d visible widgets (frame=%p)\n", sectionnode->item->path, n_visible_widgets, sectionnode->frame));
		if ((sectionnode->empty = (itemno == 0 || n_visible_widgets == 0))) {
			if (sectionnode->real_frame)
				gtk_widget_hide (sectionnode->real_frame);

			if (sectionnode->frame)
				gtk_widget_hide (sectionnode->frame);

			sectionno--;
		} else {
			if (sectionnode->real_frame)
				gtk_widget_show (sectionnode->real_frame);

			if (sectionnode->frame)
				gtk_widget_show (sectionnode->frame);
		}
		d (printf ("%s section '%s' [sections=%d]\n", sectionnode->empty?"hiding":"showing", sectionnode->item->path, sectionno));
	}

	/* If the last page doesn't contain anything, hide it */
	if (pagenode != NULL && pagenode->frame != NULL) {
		if ((pagenode->empty = sectionno == 0)) {
			gtk_widget_hide (pagenode->frame);
			pageno--;
		} else
			gtk_widget_show (pagenode->frame);
		d (printf ("%s page '%s' [section=%d]\n", pagenode->empty?"hiding":"showing", pagenode->item->path, pageno));
	}

	if (book) {
		/* make this depend on flags?? */
		if (gtk_notebook_get_n_pages ((GtkNotebook *) book) == 1) {
			gtk_notebook_set_show_tabs ((GtkNotebook *) book, FALSE);
			gtk_notebook_set_show_border ((GtkNotebook *) book, FALSE);
		}
	}
}

/**
 * e_config_set_target:
 * @config: An initialised EConfig.
 * @target: A target allocated from @config.
 *
 * Sets the target object for the config window.  Generally the target
 * is set only once, and will supply its own "changed" signal which
 * can be used to drive the modal.  This is a virtual method so that
 * the implementing class can connect to the changed signal and
 * initiate a e_config_target_changed() call where appropriate.
 **/
void
e_config_set_target (EConfig *config,
                     EConfigTarget *target)
{
	if (config->target != target)
		E_CONFIG_GET_CLASS (config)->set_target (config, target);
}

static void
ec_widget_destroy (GtkWidget *w,
                   EConfig *ec)
{
	if (ec->target) {
		e_config_target_free (ec, ec->target);
		ec->target = NULL;
	}

	g_object_unref (ec);
}

/**
 * e_config_create_widget:
 * @config: An initialised EConfig object.
 *
 * Create the #GtkNotebook described by @config.
 *
 * This object will be self-driving, but will not close itself once
 * complete.
 *
 * Unless reffed otherwise, the management object @config will be
 * finalized when the widget is.
 *
 * Return value: The widget, also available in @config.widget
 **/
GtkWidget *
e_config_create_widget (EConfig *config)
{
	EConfigPrivate *p = config->priv;
	GPtrArray *items = g_ptr_array_new ();
	GList *link;
	GSList *l;
	gint i;

	g_return_val_if_fail (config->target != NULL, NULL);

	ec_add_static_items (config);

	/* FIXME: need to override old ones with new names */
	link = p->menus;
	while (link != NULL) {
		struct _menu_node *mnode = link->data;

		for (l = mnode->menu; l; l = l->next) {
			struct _EConfigItem *item = l->data;
			struct _widget_node *wn = g_malloc0 (sizeof (*wn));

			wn->item = item;
			wn->context = mnode;
			wn->config = config;
			g_ptr_array_add (items, wn);
		}

		link = g_list_next (link);
	}

	qsort (items->pdata, items->len, sizeof (items->pdata[0]), ep_cmp);

	for (i = 0; i < items->len; i++)
		p->widgets = g_list_append (p->widgets, items->pdata[i]);

	g_ptr_array_free (items, TRUE);
	ec_rebuild (config);

	/* auto-unref it */
	g_signal_connect (
		config->widget, "destroy",
		G_CALLBACK (ec_widget_destroy), config);

	/* FIXME: for some reason ec_rebuild puts the widget on page 1, this is just to override that */
	gtk_notebook_set_current_page ((GtkNotebook *) config->widget, 0);

	return config->widget;
}

static void
ec_call_page_check (EConfig *config)
{
	if (config->window) {
		if (e_config_page_check (config, NULL)) {
			gtk_dialog_set_response_sensitive ((GtkDialog *) config->window, GTK_RESPONSE_OK, TRUE);
		} else {
			gtk_dialog_set_response_sensitive ((GtkDialog *) config->window, GTK_RESPONSE_OK, FALSE);
		}
	}
}

static gboolean
ec_idle_handler_for_rebuild (gpointer data)
{
	EConfig *config = (EConfig *) data;

	ec_rebuild (config);
	ec_call_page_check (config);

	return FALSE;
}

/**
 * e_config_target_changed:
 * @config: an #EConfig
 * @how: an enum value indicating how the target has changed
 *
 * Indicate that the target has changed.  This may be called by the
 * self-aware target itself, or by the driving code.  If @how is
 * %E_CONFIG_TARGET_CHANGED_REBUILD, then the entire configuration
 * widget may be recreated based on the changed target.
 *
 * This is used to sensitise Assistant next/back buttons and the Apply
 * button for the Notebook mode.
 **/
void
e_config_target_changed (EConfig *config,
                         e_config_target_change_t how)
{
	if (how == E_CONFIG_TARGET_CHANGED_REBUILD) {
		g_idle_add (ec_idle_handler_for_rebuild, config);
	} else {
		ec_call_page_check (config);
	}

	/* virtual method/signal? */
}

/**
 * e_config_abort:
 * @config: an #EConfig
 *
 * Signify that the stateful configuration changes must be discarded
 * to all listeners.  This is used by self-driven assistant or notebook, or
 * may be used by code using the widget directly.
 **/
void
e_config_abort (EConfig *config)
{
	g_return_if_fail (E_IS_CONFIG (config));

	g_signal_emit (config, signals[ABORT], 0);
}

/**
 * e_config_commit:
 * @config: an #EConfig
 *
 * Signify that the stateful configuration changes should be saved.
 * This is used by the self-driven assistant or notebook, or may be used
 * by code driving the widget directly.
 **/
void
e_config_commit (EConfig *config)
{
	g_return_if_fail (E_IS_CONFIG (config));

	g_signal_emit (config, signals[COMMIT], 0);
}

/**
 * e_config_page_check:
 * @config: an #EConfig
 * @pageid: the path of the page item
 *
 * Check that a given page is complete.  If @pageid is NULL, then check
 * the whole config.  No check is made that the page actually exists.
 *
 * Return value: FALSE if the data is inconsistent/incomplete.
 **/
gboolean
e_config_page_check (EConfig *config,
                     const gchar *pageid)
{
	GList *link;

	link = config->priv->checks;

	while (link != NULL) {
		struct _check_node *node = link->data;

		if ((pageid == NULL
		     || node->pageid == NULL
		     || strcmp (node->pageid, pageid) == 0)
		    && !node->func (config, pageid, node->data)) {
			return FALSE;
		}

		link = g_list_next (link);
	}

	return TRUE;
}

/* ********************************************************************** */

/**
 * e_config_class_add_factory:
 * @klass: Implementing class pointer.
 * @id: The name of the configuration window you're interested in.
 *      This may be NULL to be called for all windows.
 * @func: An EConfigFactoryFunc to call when the window @id is being created.
 * @user_data: Callback data.
 *
 * Add a config factory which will be called to add_items() any
 * extra items's if wants to, to the current Config window.
 *
 * TODO: Make the id a pattern?
 *
 * Return value: A handle to the factory.
 **/
EConfigFactory *
e_config_class_add_factory (EConfigClass *klass,
                            const gchar *id,
                            EConfigFactoryFunc func,
                            gpointer user_data)
{
	EConfigFactory *factory;

	g_return_val_if_fail (E_IS_CONFIG_CLASS (klass), NULL);
	g_return_val_if_fail (func != NULL, NULL);

	factory = g_slice_new0 (EConfigFactory);
	factory->id = g_strdup (id);
	factory->func = func;
	factory->user_data = user_data;

	klass->factories = g_list_append (klass->factories, factory);

	return factory;
}

/**
 * e_config_target_new:
 * @config: an #EConfig
 * @type: type, up to implementor
 * @size: size of object to allocate
 *
 * Allocate a new config target suitable for this class.  Implementing
 * classes will define the actual content of the target.
 **/
gpointer
e_config_target_new (EConfig *config,
                     gint type,
                     gsize size)
{
	EConfigTarget *target;

	if (size < sizeof (EConfigTarget)) {
		g_warning ("Size is less than size of EConfigTarget\n");
		size = sizeof (EConfigTarget);
	}

	target = g_malloc0 (size);
	target->config = g_object_ref (config);
	target->type = type;

	return target;
}

/**
 * e_config_target_free:
 * @config: an #EConfig
 * @target: the target to free
 *
 * Free a target.  The implementing class can override this method to
 * free custom targets.
 **/
void
e_config_target_free (EConfig *config,
                      gpointer target)
{
	E_CONFIG_GET_CLASS (config)->target_free (
		config, (EConfigTarget *) target);
}

/* ********************************************************************** */

/* Config menu plugin handler */

/*
 * <e-plugin
 *   class="org.gnome.mail.plugin.config:1.0"
 *   id="org.gnome.mail.plugin.config.item:1.0"
 *   type="shlib"
 *   location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
 *   name="imap"
 *   description="IMAP4 and IMAP4v1 mail store">
 *   <hook class="org.gnome.mail.configMenu:1.0"
 *         handler="HandleConfig">
 *   <menu id="any" target="select">
 *    <item
 *     type="item|toggle|radio|image|submenu|bar"
 *     active
 *     path="foo/bar"
 *     label="label"
 *     icon="foo"
 *     activate="ep_view_emacs"/>
 *   </menu>
 * </e-plugin>
 */

static const EPluginHookTargetKey config_hook_item_types[] = {
	{ "book", E_CONFIG_BOOK },

	{ "page", E_CONFIG_PAGE },
	{ "section", E_CONFIG_SECTION },
	{ "section_grid", E_CONFIG_SECTION_GRID },
	{ "item", E_CONFIG_ITEM },
	{ "item_grid", E_CONFIG_ITEM_GRID },
	{ NULL },
};

G_DEFINE_TYPE (
	EConfigHook,
	e_config_hook,
	E_TYPE_PLUGIN_HOOK)

static void
config_hook_commit (EConfig *ec,
                    EConfigHookGroup *group)
{
	if (group->commit && group->hook->hook.plugin->enabled)
		e_plugin_invoke (group->hook->hook.plugin, group->commit, ec->target);
}

static void
config_hook_abort (EConfig *ec,
                   EConfigHookGroup *group)
{
	if (group->abort && group->hook->hook.plugin->enabled)
		e_plugin_invoke (group->hook->hook.plugin, group->abort, ec->target);
}

static gboolean
config_hook_check (EConfig *ec,
                   const gchar *pageid,
                   gpointer data)
{
	EConfigHookGroup *group = data;
	EConfigHookPageCheckData hdata;

	if (!group->hook->hook.plugin->enabled)
		return TRUE;

	hdata.config = ec;
	hdata.target = ec->target;
	hdata.pageid = pageid ? pageid:"";

	return GPOINTER_TO_INT (e_plugin_invoke (group->hook->hook.plugin, group->check, &hdata));
}

static void
config_hook_factory (EConfig *config,
                     gpointer data)
{
	EConfigHookGroup *group = data;

	d (printf ("config factory called %s\n", group->id ? group->id:"all menus"));

	if (config->target->type != group->target_type
	    || !group->hook->hook.plugin->enabled)
		return;

	if (group->items) {
		e_config_add_items (config, group->items, NULL, group);
		g_signal_connect (
			config, "abort",
			G_CALLBACK (config_hook_abort), group);
		g_signal_connect (
			config, "commit",
			G_CALLBACK (config_hook_commit), group);
	}

	if (group->check)
		e_config_add_page_check (config, NULL, config_hook_check, group);
}

static void
config_hook_free_item (struct _EConfigItem *item)
{
	g_free (item->path);
	g_free (item->label);
	g_free (item->user_data);
	g_free (item);
}

static void
config_hook_free_group (EConfigHookGroup *group)
{
	g_slist_foreach (group->items, (GFunc) config_hook_free_item, NULL);
	g_slist_free (group->items);

	g_free (group->id);
	g_free (group);
}

static GtkWidget *
config_hook_widget_factory (EConfig *config,
                            EConfigItem *item,
                            GtkWidget *parent,
                            GtkWidget *old,
                            gint position,
                            gpointer data)
{
	EConfigHookGroup *group = data;
	EConfigHookItemFactoryData factory_data;
	EPlugin *plugin;

	factory_data.config = config;
	factory_data.item = item;
	factory_data.target = config->target;
	factory_data.parent = parent;
	factory_data.old = old;
	factory_data.position = position;

	plugin = group->hook->hook.plugin;
	return e_plugin_invoke (plugin, item->user_data, &factory_data);
}

static GtkWidget *
config_hook_section_factory (EConfig *config,
                             EConfigItem *item,
                             GtkWidget *parent,
                             GtkWidget *old,
                             gint position,
                             gpointer data,
                             GtkWidget **real_frame)
{
	EConfigHookGroup *group = data;
	GtkWidget *label = NULL;
	GtkWidget *widget;
	EPlugin *plugin;

	if (item->label != NULL) {
		const gchar *translated;
		gchar *markup;

		translated = gettext (item->label);
		markup = g_markup_printf_escaped ("<b>%s</b>", translated);

		label = gtk_label_new (markup);
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_label_set_xalign (GTK_LABEL (label), 0);
		gtk_widget_show (label);

		g_free (markup);
	}

	widget = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (widget), label);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (parent), widget, FALSE, FALSE, 0);

	*real_frame = widget;

	/* This is why we have a custom factory for sections.
	 * When the plugin is disabled the frame is invisible. */
	plugin = group->hook->hook.plugin;
	e_binding_bind_property (
		plugin, "enabled",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	parent = widget;

	switch (item->type) {
		case E_CONFIG_SECTION:
			widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
			break;

		case E_CONFIG_SECTION_GRID:
			widget = gtk_grid_new ();
			gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
			gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
			break;

		default:
			g_return_val_if_reached (NULL);
	}

	gtk_widget_set_margin_top (widget, 6);
	gtk_widget_set_margin_start (widget, 12);
	gtk_container_add (GTK_CONTAINER (parent), widget);
	gtk_widget_show (widget);

	return widget;
}

static struct _EConfigItem *
config_hook_construct_item (EPluginHook *eph,
                     EConfigHookGroup *menu,
                     xmlNodePtr root,
                     EConfigHookTargetMap *map)
{
	struct _EConfigItem *item;

	d (printf ("  loading config item\n"));
	item = g_malloc0 (sizeof (*item));
	if ((item->type = e_plugin_hook_id (root, config_hook_item_types, "type")) == E_CONFIG_INVALID)
		goto error;
	item->path = e_plugin_xml_prop (root, "path");
	item->label = e_plugin_xml_prop_domain (root, "label", eph->plugin->domain);
	item->user_data = e_plugin_xml_prop (root, "factory");

	if (item->path == NULL
	    || (item->label == NULL && item->user_data == NULL))
		goto error;

	if (item->user_data)
		item->factory = config_hook_widget_factory;
	else if (item->type == E_CONFIG_SECTION)
		item->factory = (EConfigItemFactoryFunc) config_hook_section_factory;
	else if (item->type == E_CONFIG_SECTION_GRID)
		item->factory = (EConfigItemFactoryFunc) config_hook_section_factory;

	d (printf ("   path=%s label=%s factory=%s\n", item->path, item->label, (gchar *) item->user_data));

	return item;
error:
	d (printf ("error!\n"));
	config_hook_free_item (item);
	return NULL;
}

static EConfigHookGroup *
config_hook_construct_menu (EPluginHook *eph,
                            xmlNodePtr root)
{
	EConfigHookGroup *menu;
	xmlNodePtr node;
	EConfigHookTargetMap *map;
	EConfigHookClass *class;
	gchar *tmp;

	class = E_CONFIG_HOOK_GET_CLASS (eph);

	d (printf (" loading menu\n"));
	menu = g_malloc0 (sizeof (*menu));

	tmp = (gchar *) xmlGetProp (root, (const guchar *)"target");
	if (tmp == NULL)
		goto error;
	map = g_hash_table_lookup (class->target_map, tmp);
	xmlFree (tmp);
	if (map == NULL)
		goto error;

	menu->target_type = map->id;
	menu->id = e_plugin_xml_prop (root, "id");
	if (menu->id == NULL) {
		g_warning (
			"Plugin '%s' missing 'id' field in group for '%s'\n",
			eph->plugin->name,
			E_PLUGIN_HOOK_CLASS (class)->id);
		goto error;
	}
	menu->check = e_plugin_xml_prop (root, "check");
	menu->commit = e_plugin_xml_prop (root, "commit");
	menu->abort = e_plugin_xml_prop (root, "abort");
	menu->hook = (EConfigHook *) eph;
	node = root->children;
	while (node) {
		if (0 == strcmp ((gchar *) node->name, "item")) {
			struct _EConfigItem *item;

			item = config_hook_construct_item (eph, menu, node, map);
			if (item)
				menu->items = g_slist_append (menu->items, item);
		}
		node = node->next;
	}

	return menu;
error:
	config_hook_free_group (menu);
	return NULL;
}

static gint
config_hook_construct (EPluginHook *eph,
                       EPlugin *ep,
                       xmlNodePtr root)
{
	xmlNodePtr node;
	EConfigClass *class;
	EConfigHook *config_hook;

	config_hook = (EConfigHook *) eph;

	d (printf ("loading config hook\n"));

	if (((EPluginHookClass *) e_config_hook_parent_class)->construct (eph, ep, root) == -1)
		return -1;

	class = E_CONFIG_HOOK_GET_CLASS (eph)->config_class;

	node = root->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "group") == 0) {
			EConfigHookGroup *group;

			group = config_hook_construct_menu (eph, node);
			if (group) {
				e_config_class_add_factory (class, group->id, config_hook_factory, group);
				config_hook->groups = g_slist_append (config_hook->groups, group);
			}
		}
		node = node->next;
	}

	eph->plugin = ep;

	return 0;
}

static void
config_hook_finalize (GObject *object)
{
	EConfigHook *config_hook = (EConfigHook *) object;

	g_slist_free_full (
		config_hook->groups,
		(GDestroyNotify) config_hook_free_group);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_config_hook_parent_class)->finalize (object);
}

static void
e_config_hook_class_init (EConfigHookClass *class)
{
	GObjectClass *object_class;
	EPluginHookClass *plugin_hook_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = config_hook_finalize;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->construct = config_hook_construct;

	/* this is actually an abstract implementation but list it anyway */
	plugin_hook_class->id = "org.gnome.evolution.config:1.0";

	class->target_map = g_hash_table_new (g_str_hash, g_str_equal);
	class->config_class = g_type_class_ref (e_config_get_type ());
}

static void
e_config_hook_init (EConfigHook *hook)
{
}

/**
 * e_config_hook_class_add_target_map:
 *
 * @hook_class: The dervied #EConfigHook class.
 * @map: A map used to describe a single EConfigTarget type for this class.
 *
 * Add a targe tmap to a concrete derived class of EConfig.  The
 * target map enumates the target types available for the implenting
 * class.
 **/
void
e_config_hook_class_add_target_map (EConfigHookClass *hook_class,
                                    const EConfigHookTargetMap *map)
{
	g_hash_table_insert (
		hook_class->target_map,
		(gpointer) map->type, (gpointer) map);
}
