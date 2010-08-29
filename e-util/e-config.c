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
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-config.h"
#include "e-binding.h"

#include <glib/gi18n.h>

#define E_CONFIG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CONFIG, EConfigPrivate))

#define d(x)

typedef GtkWidget * (*EConfigItemSectionFactoryFunc)(EConfig *ec, EConfigItem *, GtkWidget *parent, GtkWidget *old, gpointer data, GtkWidget **real_frame);

struct _EConfigFactory {
	gchar *id;
	EConfigFactoryFunc func;
	gpointer user_data;
};

struct _menu_node {
	GSList *menu;
	EConfigItemsFunc free;
	EConfigItemsFunc abort;
	EConfigItemsFunc commit;
	gpointer data;
};

struct _widget_node {
	EConfig *config;

	struct _menu_node *context;
	EConfigItem *item;
	GtkWidget *widget; /* widget created by the factory, if any */
	GtkWidget *frame; /* if created by us */
	GtkWidget *real_frame; /* used for sections and section tables, this is the real GtkFrame (whereas "frame" above is the internal vbox/table) */

	guint empty:1;		/* set if empty (i.e. hidden) */
};

struct _check_node {
	gchar *pageid;
	EConfigCheckFunc check;
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
	GList *finish_pages;
};

static GtkWidget *ech_config_section_factory (EConfig *config, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data, GtkWidget **real_frame);

G_DEFINE_TYPE (
	EConfig,
	e_config,
	G_TYPE_OBJECT)

static void
config_finalize (GObject *object)
{
	EConfig *emp = (EConfig *)object;
	EConfigPrivate *p = emp->priv;
	GList *link;

	d(printf("finalising EConfig %p\n", object));

	g_free (emp->id);

	link = p->menus;
	while (link != NULL) {
		struct _menu_node *node = link->data;

		if (node->free)
			node->free (emp, node->menu, node->data);

		g_free (node);

		link = g_list_delete_link (link, link);
	}

	link = p->widgets;
	while (link != NULL) {
		struct _widget_node *node = link->data;

		/* disconnect the gtk_widget_destroyed function from the widget */
		if (node->widget)
			g_signal_handlers_disconnect_matched (
				node->widget, G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, &node->widget);

		g_free (node);

		link = g_list_delete_link (link, link);
	}

	link = p->checks;
	while (link != NULL) {
		struct _check_node *node = link->data;

		g_free (node->pageid);
		g_free (node);

		link = g_list_delete_link (link, link);
	}

	link = p->finish_pages;
	while (link != NULL) {
		struct _finish_page_node *node = link->data;

		g_free (node->pageid);
		g_free (node);

		link = g_list_delete_link (link, link);
	}

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

	g_type_class_add_private (class, sizeof (EConfigPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = config_finalize;

	class->set_target = config_set_target;
	class->target_free = config_target_free;
}

static void
e_config_init (EConfig *config)
{
	config->priv = E_CONFIG_GET_PRIVATE (config);
}

/**
 * e_config_construct:
 * @ep: The instance to initialise.
 * @type: The type of configuration manager, @E_CONFIG_BOOK or
 * @E_CONFIG_ASSISTANT.
 * @id: The name of the configuration window this manager drives.
 *
 * Used by implementing classes to initialise base parameters.
 *
 * Return value: @ep is returned.
 **/
EConfig *
e_config_construct (EConfig *ep, gint type, const gchar *id)
{
	g_return_val_if_fail (type == E_CONFIG_BOOK || type == E_CONFIG_ASSISTANT, NULL);

	ep->type = type;
	ep->id = g_strdup (id);

	return ep;
}

/**
 * e_config_add_items:
 * @ec: An initialised implementing instance of EConfig.
 * @items: A list of EConfigItem's to add to the configuration manager
 * @ec.
 * @commitfunc: If supplied, called to commit the configuration items
 * to persistent storage.
 * @abortfunc: If supplied, called to abort/undo the storage of these
 * items permanently.
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
e_config_add_items (EConfig *ec, GSList *items, EConfigItemsFunc commitfunc, EConfigItemsFunc abortfunc, EConfigItemsFunc freefunc, gpointer data)
{
	struct _menu_node *node;

	node = g_malloc (sizeof (*node));
	node->menu = items;
	node->commit = commitfunc;
	node->abort = abortfunc;
	node->free = freefunc;
	node->data = data;

	ec->priv->menus = g_list_append (ec->priv->menus, node);
}

/**
 * e_config_add_page_check:
 * @ec: Initialised implemeting instance of EConfig.
 * @pageid: pageid to check.
 * @check: checking callback.
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
e_config_add_page_check (EConfig *ec, const gchar *pageid, EConfigCheckFunc check, gpointer data)
{
	struct _check_node *cn;

	cn = g_malloc0 (sizeof (*cn));
	cn->pageid = g_strdup (pageid);
	cn->check = check;
	cn->data = data;

	ec->priv->checks = g_list_append (ec->priv->checks, cn);
}

static struct _finish_page_node *
find_page_finish (EConfig *config, const gchar *pageid)
{
	GList *link;

	link = config->priv->finish_pages;

	while (link != NULL) {
		struct _finish_page_node *node = link->data;

		if (g_str_equal (node->pageid, pageid))
			return node;

		link = g_list_next (link);
	}

	return NULL;
}

/**
 * e_config_set_page_is_finish:
 * @ec: Initialised implementing instance of EConfig.
 * @pageid: pageid to change the value on.
 * @can_finish: whether the pageid can finish immediately or not.
 *
 * With is_finish set on the pageid the page is treated as the last page in an assistant.
 **/
void
e_config_set_page_is_finish (EConfig *ec, const gchar *pageid, gboolean is_finish)
{
	struct _finish_page_node *fp;

	fp = find_page_finish (ec, pageid);

	if (is_finish) {
		if (!fp) {
			fp = g_malloc0 (sizeof (*fp));
			fp->pageid = g_strdup (pageid);
			ec->priv->finish_pages = g_list_append (
				ec->priv->finish_pages, fp);
		}

		fp->is_finish = TRUE;
	} else {
		if (fp)
			fp->is_finish = FALSE;
	}
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
ep_cmp (gconstpointer ap, gconstpointer bp)
{
	struct _widget_node *a = *((gpointer *)ap);
	struct _widget_node *b = *((gpointer *)bp);

	return strcmp (a->item->path, b->item->path);
}

static GList *
ec_assistant_find_page (EConfig *ec, GtkWidget *page, gint *page_index)
{
	struct _widget_node *node = NULL;
	GList *link;

	g_return_val_if_fail (ec != NULL, NULL);
	g_return_val_if_fail (GTK_IS_ASSISTANT (ec->widget), NULL);
	g_return_val_if_fail (page != NULL, NULL);

	/* Assume failure, then if we do fail we can just return. */
	if (page_index != NULL)
		*page_index = -1;

	/* Find the page widget in our sorted widget node list. */
	for (link = ec->priv->widgets; link != NULL; link = link->next) {
		node = link->data;

		if (node->frame != page)
			continue;

		if (node->item->type == E_CONFIG_PAGE)
			break;

		if (node->item->type == E_CONFIG_PAGE_START)
			break;

		if (node->item->type == E_CONFIG_PAGE_FINISH)
			break;

		if (node->item->type == E_CONFIG_PAGE_PROGRESS)
			break;
	}

	/* FAIL: The widget is not in our list. */
	if (link == NULL)
		return NULL;

	/* Find the corresponding GtkAssistant page index. */
	if (page_index) {
		GtkAssistant *assistant;
		GtkWidget *nth_page;
		gint ii, n_pages;

		assistant = GTK_ASSISTANT (ec->widget);
		n_pages = gtk_assistant_get_n_pages (assistant);

		for (ii = 0; ii < n_pages; ii++) {
			nth_page = gtk_assistant_get_nth_page (assistant, ii);
			if (page == nth_page) {
				*page_index = ii;
				break;
			}
		}

		g_warn_if_fail (ii < n_pages);
	}

	return link;
}

static void
ec_assistant_check_current (EConfig *ec)
{
	struct _widget_node *wn;
	struct _finish_page_node *fp;
	GtkAssistant *assistant;
	GtkWidget *page;
	GList *link;
	gint page_no;

	g_return_if_fail (GTK_IS_ASSISTANT (ec->widget));

	assistant = GTK_ASSISTANT (ec->widget);
	page_no = gtk_assistant_get_current_page (assistant);

	/* no page selected yet */
	if (page_no == -1)
		return;

	page = gtk_assistant_get_nth_page (assistant, page_no);
	g_return_if_fail (page != NULL);

	link = ec_assistant_find_page (ec, page, NULL);
	g_return_if_fail (link != NULL);
	wn = link->data;

	/* this should come first, as the check function can change the finish state of the page */
	gtk_assistant_set_page_complete (assistant, page, e_config_page_check (ec, wn->item->path));

	fp = find_page_finish (ec, wn->item->path);
	if (fp) {
		GtkAssistantPageType pt = gtk_assistant_get_page_type (assistant, page);

		if (fp->is_finish && pt != GTK_ASSISTANT_PAGE_CONFIRM) {
			if (fp->orig_type == GTK_ASSISTANT_PAGE_CONTENT)
				fp->orig_type = pt;
			gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONFIRM);
		} else if (!fp->is_finish && pt != fp->orig_type) {
			gtk_assistant_set_page_type (assistant, page, fp->orig_type);
		}
	}

	gtk_assistant_update_buttons_state (assistant);
}

static gint
ec_assistant_forward (gint current_page, gpointer user_data)
{
	GtkAssistant *assistant;
	EConfig *ec = user_data;
	struct _widget_node *node;
	GtkWidget *page_widget;
	GList *link = NULL;
	gint next_page;

	/* As far as we're concerned, the GtkAssistant is just an unordered
	 * collection of pages.  Our sorted list of widget nodes determines
	 * the next page. */

	assistant = GTK_ASSISTANT (ec->widget);
	page_widget = gtk_assistant_get_nth_page (assistant, current_page);
	link = ec_assistant_find_page (ec, page_widget, NULL);

	g_return_val_if_fail (link != NULL, -1);
	node = (struct _widget_node *) link->data;

	/* If we're already on a FINISH page then we're done. */
	if (node->item->type == E_CONFIG_PAGE_FINISH)
		return -1;

	/* Find the next E_CONFIG_PAGE* type node. */
	for (link = link->next; link != NULL; link = link->next) {
		node = (struct _widget_node *) link->data;

		if (node->empty || node->frame == NULL)
			continue;

		if (node->item->type == E_CONFIG_PAGE)
			break;

		if (node->item->type == E_CONFIG_PAGE_START)
			break;

		if (node->item->type == E_CONFIG_PAGE_FINISH)
			break;

		if (node->item->type == E_CONFIG_PAGE_PROGRESS)
			break;
	}

	/* Find the corresponding GtkAssistant page number. */
	if (link != NULL) {
		node = (struct _widget_node *) link->data;
		ec_assistant_find_page (ec, node->frame, &next_page);
	} else
		next_page = -1;

	return next_page;
}

static void
ec_rebuild (EConfig *emp)
{
	EConfigPrivate *p = emp->priv;
	struct _widget_node *sectionnode = NULL, *pagenode = NULL;
	GtkWidget *book = NULL, *page = NULL, *section = NULL, *root = NULL, *assistant = NULL;
	gint pageno = 0, sectionno = 0, itemno = 0;
	gint n_visible_widgets = 0;
	GList *last_active_link = NULL;
	gboolean is_assistant;
	GList *link;

	d(printf("target changed, rebuilding:\n"));

	/* TODO: This code is pretty complex, and will probably just
	 * become more complex with time.  It could possibly be split
	 * into the two base types, but there would be a lot of code
	 * duplication */

	/* because rebuild destroys pages, and destroying active page causes crashes */
	is_assistant = emp->widget && GTK_IS_ASSISTANT (emp->widget);
	if (is_assistant) {
		GtkAssistant *assistant;
		gint page_index = gtk_assistant_get_current_page (GTK_ASSISTANT (emp->widget));

		assistant = GTK_ASSISTANT (emp->widget);
		page_index = gtk_assistant_get_current_page (assistant);

		if (page_index != -1) {
			GtkWidget *nth_page;

			nth_page = gtk_assistant_get_nth_page (
				GTK_ASSISTANT (emp->widget), page_index);
			last_active_link = ec_assistant_find_page (
				emp, nth_page, NULL);
		}
		gtk_assistant_set_current_page (GTK_ASSISTANT (emp->widget), 0);
	}

	for (link = p->widgets; link != NULL; link = g_list_next (link)) {
		struct _widget_node *wn = link->data;
		struct _EConfigItem *item = wn->item;
		const gchar *translated_label = NULL;
		GtkWidget *w;

		d(printf(" '%s'\n", item->path));

		if (item->label != NULL)
			translated_label = gettext (item->label);

		/* If the last section doesn't contain any visible widgets, hide it */
		if (sectionnode != NULL
		    && sectionnode->frame != NULL
		    && (item->type == E_CONFIG_PAGE
			|| item->type == E_CONFIG_PAGE_START
			|| item->type == E_CONFIG_PAGE_FINISH
			|| item->type == E_CONFIG_PAGE_PROGRESS
			|| item->type == E_CONFIG_SECTION
			|| item->type == E_CONFIG_SECTION_TABLE)) {
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

			d(printf("%s section '%s' [sections=%d]\n", sectionnode->empty?"hiding":"showing", sectionnode->item->path, sectionno));
		}

		/* If the last page doesn't contain anything, hide it */
		if (pagenode != NULL
		    && pagenode->frame != NULL
		    && (item->type == E_CONFIG_PAGE
			|| item->type == E_CONFIG_PAGE_START
			|| item->type == E_CONFIG_PAGE_FINISH
			|| item->type == E_CONFIG_PAGE_PROGRESS)) {
			if ((pagenode->empty = sectionno == 0)) {
				gtk_widget_hide (pagenode->frame);
				pageno--;
			} else
				gtk_widget_show (pagenode->frame);
			d(printf("%s page '%s' [section=%d]\n", pagenode->empty?"hiding":"showing", pagenode->item->path, pageno));
		}

		/* Now process the item */
		switch (item->type) {
		case E_CONFIG_BOOK:
		case E_CONFIG_ASSISTANT:
			/* Only one of BOOK or ASSISTANT may be define, it
			   is used by the defining code to mark the
			   type of the config window.  It is
			   cross-checked with the code's defined
			   type. */
			if (root != NULL) {
				g_warning("EConfig book/assistant redefined at: %s", item->path);
				break;
			}

			if (wn->widget == NULL) {
				if (item->type != emp->type) {
					g_warning("EConfig book/assistant type mismatch");
					break;
				}
				if (item->factory) {
					root = item->factory (emp, item, NULL, wn->widget, wn->context->data);
				} else if (item->type == E_CONFIG_BOOK) {
					root = gtk_notebook_new ();
					gtk_widget_show (root);
				} else if (item->type == E_CONFIG_ASSISTANT) {
					root = gtk_assistant_new ();
				} else
					abort ();

				if (item->type == E_CONFIG_ASSISTANT) {
					g_signal_connect_swapped (
						root, "apply",
						G_CALLBACK (e_config_commit), emp);
					g_signal_connect_swapped (
						root, "cancel",
						G_CALLBACK (e_config_abort), emp);
					g_signal_connect (
						root, "cancel",
						G_CALLBACK (gtk_widget_destroy), emp);
					g_signal_connect (
						root, "close",
						G_CALLBACK (gtk_widget_destroy), NULL);
					g_signal_connect_swapped (
						root, "prepare",
						G_CALLBACK (ec_assistant_check_current), emp);
					gtk_assistant_set_forward_page_func (
						GTK_ASSISTANT (root),
						ec_assistant_forward, emp, NULL);
				}

				emp->widget = root;
				wn->widget = root;
			} else {
				root = wn->widget;
			}

			if (item->type == E_CONFIG_BOOK)
				book = root;
			else
				assistant = root;

			page = NULL;
			pagenode = NULL;
			section = NULL;
			sectionnode = NULL;
			pageno = 0;
			sectionno = 0;
			break;
		case E_CONFIG_PAGE_START:
		case E_CONFIG_PAGE_FINISH:
			if (root == NULL) {
				g_warning("EConfig page defined before container widget: %s", item->path);
				break;
			}
			if (emp->type != E_CONFIG_ASSISTANT) {
				g_warning("EConfig assistant start/finish pages can't be used on E_CONFIG_BOOKs");
				break;
			}

			if (wn->widget == NULL) {
				if (item->factory) {
					page = item->factory (emp, item, root, wn->frame, wn->context->data);
				} else {
					page = gtk_vbox_new (FALSE, 0);
					gtk_container_set_border_width (GTK_CONTAINER (page), 12);
					if (pagenode) {
						/* put after */
						gint index = -1;
						ec_assistant_find_page (emp, pagenode->frame, &index);
						gtk_assistant_insert_page (GTK_ASSISTANT (assistant), page, index + 1);
					} else {
						gtk_assistant_prepend_page (GTK_ASSISTANT (assistant), page);
					}

					gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), page, item->type == E_CONFIG_PAGE_START ? GTK_ASSISTANT_PAGE_INTRO : GTK_ASSISTANT_PAGE_CONFIRM);
					gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, translated_label);
					gtk_widget_show_all (page);
				}

				if (wn->widget != NULL && wn->widget != page) {
					gtk_widget_destroy (wn->widget);
				}

				wn->frame = page;
				wn->widget = page;

				if (page) {
					const gchar *empty_xpm_img[] = {
						"75 1 2 1",
						" 	c None",
						".	c #FFFFFF",
						"                                                                           "};

					/* left side place with a blue background on a start and finish page */
					GdkPixbuf *spacer = gdk_pixbuf_new_from_xpm_data (empty_xpm_img);

					gtk_assistant_set_page_side_image (GTK_ASSISTANT (assistant), page, spacer);

					g_object_unref (spacer);
				}
			}

			pageno++;
			page = NULL;
			pagenode = wn; /* need this for previous page linking */
			section = NULL;
			sectionnode = NULL;
			sectionno = 1; /* never want to hide these */
			break;
		case E_CONFIG_PAGE:
		case E_CONFIG_PAGE_PROGRESS:
			/* CONFIG_PAGEs depend on the config type.
			   E_CONFIG_BOOK:
				The page is a VBox, stored in the notebook.
			   E_CONFIG_ASSISTANT
				The page is a VBox, stored in the GtkAssistant,
				any sections automatically added inside it. */
			sectionno = 0;
			if (root == NULL) {
				g_warning("EConfig page defined before container widget: %s", item->path);
				break;
			}
			if (item->type == E_CONFIG_PAGE_PROGRESS &&
			    emp->type != E_CONFIG_ASSISTANT) {
				g_warning("EConfig assistant progress pages can't be used on E_CONFIG_BOOKs");
				break;
			}

			if (item->factory) {
				page = item->factory (emp, item, root, wn->frame, wn->context->data);
				if (emp->type == E_CONFIG_ASSISTANT) {
					wn->frame = page;
				} else {
					wn->frame = page;
					if (page)
						gtk_notebook_reorder_child ((GtkNotebook *)book, page, pageno);
				}
				if (page)
					sectionno = 1;
			} else if (wn->widget == NULL) {
				if (emp->type == E_CONFIG_ASSISTANT) {
					page = gtk_vbox_new (FALSE, 0);
					gtk_container_set_border_width (GTK_CONTAINER (page), 12);
					if (pagenode) {
						/* put after */
						gint index = -1;
						ec_assistant_find_page (emp, pagenode->frame, &index);
						gtk_assistant_insert_page (GTK_ASSISTANT (assistant), page, index + 1);
					} else {
						gtk_assistant_prepend_page (GTK_ASSISTANT (assistant), page);
					}
					gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), page, item->type == E_CONFIG_PAGE ? GTK_ASSISTANT_PAGE_CONTENT : GTK_ASSISTANT_PAGE_PROGRESS);
					gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), page, translated_label);
					gtk_widget_show_all (page);

					wn->frame = page;
				} else {
					w = gtk_label_new_with_mnemonic (translated_label);
					gtk_widget_show (w);
					page = gtk_vbox_new (FALSE, 12);
					gtk_container_set_border_width ((GtkContainer *)page, 12);
					gtk_widget_show (page);
					gtk_notebook_insert_page ((GtkNotebook *)book, page, w, pageno);
					wn->frame = page;
				}
			} else
				page = wn->widget;

			d(printf("page %d:%s widget %p\n", pageno, item->path, page));

			if (wn->widget && wn->widget != page) {
				d(printf("destroy old widget for page '%s' (%p)\n", item->path, wn->widget));
				gtk_widget_destroy (wn->widget);
			}

			pageno++;
			pagenode = wn;
			section = NULL;
			sectionnode = NULL;
			wn->widget = page;
			if (page)
				g_signal_connect(page, "destroy", G_CALLBACK(gtk_widget_destroyed), &wn->widget);
			break;
		case E_CONFIG_SECTION:
		case E_CONFIG_SECTION_TABLE:
			/* The section factory is always called with
			   the parent vbox object.  Even for assistant pages. */
			if (page == NULL) {
				/*g_warning("EConfig section '%s' has no parent page", item->path);*/
				section = NULL;
				wn->widget = NULL;
				wn->frame = NULL;
				goto nopage;
			}

			itemno = 0;
			n_visible_widgets = 0;

			d(printf("Building section %s - '%s' - %s factory\n", item->path, item->label, item->factory ? "with" : "without"));

			if (item->factory) {
				/* For sections, we pass an extra argument to the usual EConfigItemFactoryFunc.
				 * If this is an automatically-generated section, that extra argument (real_frame from
				 * EConfigItemSectionFactoryFunc) will contain the actual GtkFrame upon returning.
				 */
				EConfigItemSectionFactoryFunc factory = (EConfigItemSectionFactoryFunc) item->factory;

				section = factory (emp, item, page, wn->widget, wn->context->data, &wn->real_frame);
				wn->frame = section;
				if (section)
					itemno = 1;

				if (factory != ech_config_section_factory) {
					/* This means there is a section that came from a user-specified factory,
					 * so we don't know what is inside the section.  In that case, we increment
					 * n_visible_widgets so that the section will not get hidden later (we don't know
					 * if the section is empty or not, so we cannot decide to hide it).
					 *
					 * For automatically-generated sections, we use a special ech_config_section_factory() -
					 * see emph_construct_item().
					 */
					n_visible_widgets++;
					d(printf ("  n_visible_widgets++ because there is a section factory -> frame=%p\n", section));
				}

				if (section
				    && ((item->type == E_CONFIG_SECTION && !GTK_IS_BOX (section))
					|| (item->type == E_CONFIG_SECTION_TABLE && !GTK_IS_TABLE (section))))
					g_warning("EConfig section type is wrong");
			} else {
				GtkWidget *frame;
				GtkWidget *label = NULL;

				if (wn->frame) {
					d(printf("Item %s, clearing generated section widget\n", wn->item->path));
					gtk_widget_destroy (wn->frame);
					wn->widget = NULL;
					wn->frame = NULL;
				}

				if (translated_label != NULL) {
					gchar *txt = g_markup_printf_escaped("<span weight=\"bold\">%s</span>", translated_label);

					label = g_object_new (gtk_label_get_type (),
							     "label", txt,
							     "use_markup", TRUE,
							     "xalign", 0.0, NULL);
					g_free (txt);
				}

				if (item->type == E_CONFIG_SECTION)
					section = gtk_vbox_new (FALSE, 6);
				else {
					section = gtk_table_new (1, 1, FALSE);
					gtk_table_set_col_spacings ((GtkTable *)section, 6);
					gtk_table_set_row_spacings ((GtkTable *)section, 6);
				}

				frame = g_object_new (gtk_frame_get_type (),
						     "shadow_type", GTK_SHADOW_NONE,
						     "label_widget", label,
						     "child", g_object_new(gtk_alignment_get_type(),
									   "left_padding", 12,
									   "top_padding", 6,
									   "child", section, NULL),
						     NULL);
				gtk_widget_show_all (frame);
				gtk_box_pack_start ((GtkBox *)page, frame, FALSE, FALSE, 0);
				wn->frame = frame;
			}
		nopage:
			if (wn->widget && wn->widget != section) {
				d(printf("destroy old widget for section '%s'\n", item->path));
				gtk_widget_destroy (wn->widget);
			}

			d(printf("Item %s, setting section widget\n", wn->item->path));

			sectionno++;
			wn->widget = section;
			if (section)
				g_signal_connect(section, "destroy", G_CALLBACK(gtk_widget_destroyed), &wn->widget);
			sectionnode = wn;
			break;
		case E_CONFIG_ITEM:
		case E_CONFIG_ITEM_TABLE:
			/* generated sections never retain their widgets on a rebuild */
			if (sectionnode->item->factory == NULL)
				wn->widget = NULL;

			/* ITEMs are called with the section parent.
			   The type depends on the section type,
			   either a GtkTable, or a GtkVBox */
			w = NULL;
			if (section == NULL) {
				wn->widget = NULL;
				wn->frame = NULL;
				g_warning("EConfig item has no parent section: %s", item->path);
			} else if ((item->type == E_CONFIG_ITEM && !GTK_IS_BOX (section))
				 || (item->type == E_CONFIG_ITEM_TABLE && !GTK_IS_TABLE (section)))
				g_warning("EConfig item parent type is incorrect: %s", item->path);
			else if (item->factory)
				w = item->factory (emp, item, section, wn->widget, wn->context->data);

			d(printf("item %d:%s widget %p\n", itemno, item->path, w));

			d(printf ("  item %s: (%s - %s)\n",
				  item->path,
				  g_type_name_from_instance ((GTypeInstance *) w),
				  gtk_widget_get_visible (w) ? "visible" : "invisible"));

			if (wn->widget && wn->widget != w) {
				d(printf("destroy old widget for item '%s'\n", item->path));
				gtk_widget_destroy (wn->widget);
			}

			wn->widget = w;
			if (w) {
				g_signal_connect(w, "destroy", G_CALLBACK(gtk_widget_destroyed), &wn->widget);
				itemno++;

				if (gtk_widget_get_visible (w))
					n_visible_widgets++;
			}
			break;
		}
	}

	/* If the last section doesn't contain any visible widgets, hide it */
	if (sectionnode != NULL && sectionnode->frame != NULL) {
		d(printf ("Section %s - %d visible widgets (frame=%p)\n", sectionnode->item->path, n_visible_widgets, sectionnode->frame));
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
		d(printf("%s section '%s' [sections=%d]\n", sectionnode->empty?"hiding":"showing", sectionnode->item->path, sectionno));
	}

	/* If the last page doesn't contain anything, hide it */
	if (pagenode != NULL && pagenode->frame != NULL) {
		if ((pagenode->empty = sectionno == 0)) {
			gtk_widget_hide (pagenode->frame);
			pageno--;
		} else
			gtk_widget_show (pagenode->frame);
		d(printf("%s page '%s' [section=%d]\n", pagenode->empty?"hiding":"showing", pagenode->item->path, pageno));
	}

	if (book) {
		/* make this depend on flags?? */
		if (gtk_notebook_get_n_pages ((GtkNotebook *)book) == 1) {
			gtk_notebook_set_show_tabs ((GtkNotebook *)book, FALSE);
			gtk_notebook_set_show_border ((GtkNotebook *)book, FALSE);
		}
	}

	if (is_assistant && last_active_link != NULL) {
		GtkAssistant *assistant;
		struct _widget_node *wn;
		gint page_index = -1;

		wn = last_active_link->data;
		assistant = GTK_ASSISTANT (emp->widget);
		ec_assistant_find_page (emp, wn->frame, &page_index);
		gtk_assistant_set_current_page (assistant, page_index);
	}
}

/**
 * e_config_set_target:
 * @emp: An initialised EConfig.
 * @target: A target allocated from @emp.
 *
 * Sets the target object for the config window.  Generally the target
 * is set only once, and will supply its own "changed" signal which
 * can be used to drive the modal.  This is a virtual method so that
 * the implementing class can connect to the changed signal and
 * initiate a e_config_target_changed() call where appropriate.
 **/
void
e_config_set_target (EConfig *emp, EConfigTarget *target)
{
	if (emp->target != target)
		((EConfigClass *)G_OBJECT_GET_CLASS (emp))->set_target (emp, target);
}

static void
ec_widget_destroy (GtkWidget *w, EConfig *ec)
{
	if (ec->target) {
		e_config_target_free (ec, ec->target);
		ec->target = NULL;
	}

	g_object_unref (ec);
}

/**
 * e_config_create_widget:
 * @emp: An initialised EConfig object.
 *
 * Create the widget described by @emp.  Only the core widget
 * appropriate for the given type is created, i.e. a GtkNotebook for
 * the E_CONFIG_BOOK type and a GtkAssistant for the E_CONFIG_ASSISTANT
 * type.
 *
 * This object will be self-driving, but will not close itself once
 * complete.
 *
 * Unless reffed otherwise, the management object @emp will be
 * finalized when the widget is.
 *
 * Return value: The widget, also available in @emp.widget
 **/
GtkWidget *
e_config_create_widget (EConfig *emp)
{
	EConfigPrivate *p = emp->priv;
	GPtrArray *items = g_ptr_array_new ();
	GList *link;
	GSList *l;
	/*char *domain = NULL;*/
	gint i;

	g_return_val_if_fail (emp->target != NULL, NULL);

	ec_add_static_items (emp);

	/* FIXME: need to override old ones with new names */
	link = p->menus;
	while (link != NULL) {
		struct _menu_node *mnode = link->data;

		for (l=mnode->menu; l; l = l->next) {
			struct _EConfigItem *item = l->data;
			struct _widget_node *wn = g_malloc0 (sizeof (*wn));

			wn->item = item;
			wn->context = mnode;
			wn->config = emp;
			g_ptr_array_add (items, wn);
		}

		link = g_list_next (link);
	}

	qsort (items->pdata, items->len, sizeof (items->pdata[0]), ep_cmp);

	for (i=0;i<items->len;i++)
		p->widgets = g_list_append (p->widgets, items->pdata[i]);

	g_ptr_array_free (items, TRUE);
	ec_rebuild (emp);

	/* auto-unref it */
	g_signal_connect(emp->widget, "destroy", G_CALLBACK(ec_widget_destroy), emp);

	/* FIXME: for some reason ec_rebuild puts the widget on page 1, this is just to override that */
	if (emp->type == E_CONFIG_BOOK)
		gtk_notebook_set_current_page ((GtkNotebook *)emp->widget, 0);
	else {
		gtk_window_set_position (GTK_WINDOW (emp->widget), GTK_WIN_POS_CENTER);
		gtk_widget_show (emp->widget);
	}

	return emp->widget;
}

static void
ec_dialog_response (GtkWidget *d, gint id, EConfig *ec)
{
	if (id == GTK_RESPONSE_OK)
		e_config_commit (ec);
	else
		e_config_abort (ec);

	gtk_widget_destroy (d);
}

/**
 * e_config_create_window:
 * @emp: Initialised and configured EMConfig derived instance.
 * @parent: Parent window or NULL.
 * @title: Title of window or dialog.
 *
 * Create a managed GtkWindow object from @emp.  This window will be
 * fully driven by the EConfig @emp.  If @emp.type is
 * @E_CONFIG_ASSISTANT, then this will be a toplevel GtkWindow containing
 * a GtkAssistant.  If it is @E_CONFIG_BOOK then it will be a GtkDialog
 * containing a Notebook.
 *
 * Unless reffed otherwise, the management object @emp will be
 * finalized when the widget is.
 *
 * Return value: The window widget.  This is also stored in @emp.window.
 **/
GtkWidget *
e_config_create_window (EConfig *emp, GtkWindow *parent, const gchar *title)
{
	GtkWidget *w;

	e_config_create_widget (emp);

	if (emp->type == E_CONFIG_BOOK) {
		w = gtk_dialog_new_with_buttons (title, parent,
						GTK_DIALOG_DESTROY_WITH_PARENT |
						GTK_DIALOG_NO_SEPARATOR,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OK, GTK_RESPONSE_OK,
						NULL);
		g_signal_connect(w, "response", G_CALLBACK(ec_dialog_response), emp);

		gtk_widget_ensure_style (w);
		gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (w))), 0);
		gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_action_area (GTK_DIALOG (w))), 12);

		gtk_box_pack_start ((GtkBox *)gtk_dialog_get_content_area (((GtkDialog *)w)), emp->widget, TRUE, TRUE, 0);
	} else {
		/* response is handled directly by the assistant stuff */
		w = emp->widget;
		gtk_window_set_title ((GtkWindow *)w, title);
	}

	emp->window = w;
	gtk_widget_show (w);

	return w;
}

static void
ec_call_page_check (EConfig *emp)
{
	if (emp->type == E_CONFIG_ASSISTANT) {
		ec_assistant_check_current (emp);
	} else {
		if (emp->window) {
			if (e_config_page_check (emp, NULL)) {
				gtk_dialog_set_response_sensitive ((GtkDialog *)emp->window, GTK_RESPONSE_OK, TRUE);
			} else {
				gtk_dialog_set_response_sensitive ((GtkDialog *)emp->window, GTK_RESPONSE_OK, FALSE);
			}
		}
	}
}

static gboolean
ec_idle_handler_for_rebuild (gpointer data)
{
	EConfig *emp = (EConfig*) data;

	ec_rebuild (emp);
	ec_call_page_check (emp);

	return FALSE;
}

/**
 * e_config_target_changed:
 * @emp: an #EConfig
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
e_config_target_changed (EConfig *emp, e_config_target_change_t how)
{
	if (how == E_CONFIG_TARGET_CHANGED_REBUILD) {
		g_idle_add (ec_idle_handler_for_rebuild, emp);
	} else {
		ec_call_page_check (emp);
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
	GList *link;

	g_return_if_fail (E_IS_CONFIG (config));

	/* TODO: should these just be signals? */

	link = config->priv->menus;

	while (link != NULL) {
		struct _menu_node *node = link->data;

		if (node->abort != NULL)
			node->abort (config, node->menu, node->data);

		link = g_list_next (link);
	}
}

/**
 * e_config_commit:
 * @ec: an #EConfig
 *
 * Signify that the stateful configuration changes should be saved.
 * This is used by the self-driven assistant or notebook, or may be used
 * by code driving the widget directly.
 **/
void
e_config_commit (EConfig *config)
{
	GList *link;

	g_return_if_fail (E_IS_CONFIG (config));

	/* TODO: should these just be signals? */

	link = config->priv->menus;

	while (link != NULL) {
		struct _menu_node *node = link->data;

		if (node->commit != NULL)
			node->commit (config, node->menu, node->data);

		link = g_list_next (link);
	}
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
e_config_page_check (EConfig *config, const gchar *pageid)
{
	GList *link;

	link = config->priv->checks;

	while (link != NULL) {
		struct _check_node *node = link->data;

		if ((pageid == NULL
		     || node->pageid == NULL
		     || strcmp (node->pageid, pageid) == 0)
		    && !node->check (config, pageid, node->data)) {
			return FALSE;
		}

		link = g_list_next (link);
	}

	return TRUE;
}

/**
 * e_config_page_get:
 * @ec:
 * @pageid: The path of the page item.
 *
 * Retrieve the page widget corresponding to @pageid.
 *
 * Return value: The page widget.  It will be the root GtkNotebook
 * container or the GtkVBox object inside the assistant.
 **/
GtkWidget *
e_config_page_get (EConfig *ec, const gchar *pageid)
{
	GList *link;

	link = ec->priv->widgets;

	while (link != NULL) {
		struct _widget_node *wn = link->data;

		if (!wn->empty
		    && (wn->item->type == E_CONFIG_PAGE
			|| wn->item->type == E_CONFIG_PAGE_START
			|| wn->item->type == E_CONFIG_PAGE_FINISH
			|| wn->item->type == E_CONFIG_PAGE_PROGRESS)
		    && !strcmp (wn->item->path, pageid))
			return wn->frame;

		link = g_list_next (link);
	}

	return NULL;
}

/**
 * e_config_page_next:
 * @ec:
 * @pageid: The path of the page item.
 *
 * Find the path of the next visible page after @pageid.  If @pageid
 * is NULL then find the first visible page.
 *
 * Return value: The path of the next page, or @NULL if @pageid was the
 * last configured and visible page.
 **/
const gchar *
e_config_page_next (EConfig *ec, const gchar *pageid)
{
	GList *link;
	gint found;

	link = g_list_first (ec->priv->widgets);
	found = pageid == NULL ? 1:0;

	while (link != NULL) {
		struct _widget_node *wn = link->data;

		if (!wn->empty
		    && (wn->item->type == E_CONFIG_PAGE
			|| wn->item->type == E_CONFIG_PAGE_START
			|| wn->item->type == E_CONFIG_PAGE_FINISH
			|| wn->item->type == E_CONFIG_PAGE_PROGRESS)) {
			if (found)
				return wn->item->path;
			else if (strcmp (wn->item->path, pageid) == 0)
				found = 1;
		}

		link = g_list_next (link);
	}

	return NULL;
}

/**
 * e_config_page_next:
 * @ec: an #EConfig
 * @pageid: The path of the page item.
 *
 * Find the path of the previous visible page before @pageid.  If @pageid
 * is NULL then find the last visible page.
 *
 * Return value: The path of the previous page, or @NULL if @pageid was the
 * first configured and visible page.
 **/
const gchar *
e_config_page_prev (EConfig *ec, const gchar *pageid)
{
	GList *link;
	gint found;

	link = g_list_last (ec->priv->widgets);
	found = pageid == NULL ? 1:0;

	while (link != NULL) {
		struct _widget_node *wn = link->data;

		if (!wn->empty
		    && (wn->item->type == E_CONFIG_PAGE
			|| wn->item->type == E_CONFIG_PAGE_START
			|| wn->item->type == E_CONFIG_PAGE_FINISH
			|| wn->item->type == E_CONFIG_PAGE_PROGRESS)) {
			if (found)
				return wn->item->path;
			else if (strcmp (wn->item->path, pageid) == 0)
				found = 1;
		}

		link = g_list_previous (link);
	}

	return NULL;
}

/* ********************************************************************** */

/**
 * e_config_class_add_factory:
 * @class: Implementing class pointer.
 * @id: The name of the configuration window you're interested in.
 * This may be NULL to be called for all windows.
 * @func: An EConfigFactoryFunc to call when the window @id is being
 * created.
 * @data: Callback data.
 *
 * Add a config factory which will be called to add_items() any
 * extra items's if wants to, to the current Config window.
 *
 * TODO: Make the id a pattern?
 *
 * Return value: A handle to the factory.
 **/
EConfigFactory *
e_config_class_add_factory (EConfigClass *class,
                            const gchar *id,
                            EConfigFactoryFunc func,
                            gpointer user_data)
{
	EConfigFactory *factory;

	g_return_val_if_fail (E_IS_CONFIG_CLASS (class), NULL);
	g_return_val_if_fail (func != NULL, NULL);

	factory = g_slice_new0 (EConfigFactory);
	factory->id = g_strdup (id);
	factory->func = func;
	factory->user_data = user_data;

	class->factories = g_list_append (class->factories, factory);

	return factory;
}

/**
 * e_config_class_remove_factory:
 * @factory: an #EConfigFactory
 *
 * Removes a config factory.
 **/
void
e_config_class_remove_factory (EConfigClass *class,
                               EConfigFactory *factory)
{
	g_return_if_fail (E_IS_CONFIG_CLASS (class));
	g_return_if_fail (factory != NULL);

	class->factories = g_list_remove (class->factories, factory);

	g_free (factory->id);

	g_slice_free (EConfigFactory, factory);
}

/**
 * e_config_target_new:
 * @ep: Parent EConfig object.
 * @type: type, up to implementor
 * @size: Size of object to allocate.
 *
 * Allocate a new config target suitable for this class.  Implementing
 * classes will define the actual content of the target.
 **/
gpointer e_config_target_new (EConfig *ep, gint type, gsize size)
{
	EConfigTarget *t;

	if (size < sizeof (EConfigTarget)) {
		g_warning ("Size is less than size of EConfigTarget\n");
		size = sizeof (EConfigTarget);
	}

	t = g_malloc0 (size);
	t->config = ep;
	g_object_ref (ep);
	t->type = type;

	return t;
}

/**
 * e_config_target_free:
 * @ep: Parent EConfig object.
 * @o: The target to fre.
 *
 * Free a target.  The implementing class can override this method to
 * free custom targets.
 **/
void
e_config_target_free (EConfig *ep, gpointer o)
{
	EConfigTarget *t = o;

	((EConfigClass *)G_OBJECT_GET_CLASS (ep))->target_free (ep, t);
}

/* ********************************************************************** */

/* Config menu plugin handler */

/*
<e-plugin
  class="org.gnome.mail.plugin.config:1.0"
  id="org.gnome.mail.plugin.config.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="org.gnome.mail.configMenu:1.0"
        handler="HandleConfig">
  <menu id="any" target="select">
   <item
    type="item|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    activate="ep_view_emacs"/>
  </menu>
  </extension>

*/

#define emph ((EConfigHook *)eph)

static const EPluginHookTargetKey ech_item_types[] = {
	{ "book", E_CONFIG_BOOK },
	{ "assistant", E_CONFIG_ASSISTANT },

	{ "page", E_CONFIG_PAGE },
	{ "page_start", E_CONFIG_PAGE_START },
	{ "page_finish", E_CONFIG_PAGE_FINISH },
	{ "section", E_CONFIG_SECTION },
	{ "section_table", E_CONFIG_SECTION_TABLE },
	{ "item", E_CONFIG_ITEM },
	{ "item_table", E_CONFIG_ITEM_TABLE },
	{ NULL },
};

G_DEFINE_TYPE (
	EConfigHook,
	e_config_hook,
	E_TYPE_PLUGIN_HOOK)

static void
ech_commit (EConfig *ec, GSList *items, gpointer data)
{
	struct _EConfigHookGroup *group = data;

	if (group->commit && group->hook->hook.plugin->enabled)
		e_plugin_invoke (group->hook->hook.plugin, group->commit, ec->target);
}

static void
ech_abort (EConfig *ec, GSList *items, gpointer data)
{
	struct _EConfigHookGroup *group = data;

	if (group->abort && group->hook->hook.plugin->enabled)
		e_plugin_invoke (group->hook->hook.plugin, group->abort, ec->target);
}

static gboolean
ech_check (EConfig *ec, const gchar *pageid, gpointer data)
{
	struct _EConfigHookGroup *group = data;
	EConfigHookPageCheckData hdata;

	if (!group->hook->hook.plugin->enabled)
		return TRUE;

	hdata.config = ec;
	hdata.target = ec->target;
	hdata.pageid = pageid?pageid:"";

	return GPOINTER_TO_INT (e_plugin_invoke (group->hook->hook.plugin, group->check, &hdata));
}

static void
ech_config_factory (EConfig *emp, gpointer data)
{
	struct _EConfigHookGroup *group = data;

	d(printf("config factory called %s\n", group->id?group->id:"all menus"));

	if (emp->target->type != group->target_type
	    || !group->hook->hook.plugin->enabled)
		return;

	if (group->items)
		e_config_add_items (emp, group->items, ech_commit, ech_abort, NULL, group);

	if (group->check)
		e_config_add_page_check (emp, NULL, ech_check, group);
}

static void
emph_free_item (struct _EConfigItem *item)
{
	g_free (item->path);
	g_free (item->label);
	g_free (item->user_data);
	g_free (item);
}

static void
emph_free_group (struct _EConfigHookGroup *group)
{
	g_slist_foreach (group->items, (GFunc)emph_free_item, NULL);
	g_slist_free (group->items);

	g_free (group->id);
	g_free (group);
}

static GtkWidget *
ech_config_widget_factory (EConfig *config,
                           EConfigItem *item,
                           GtkWidget *parent,
                           GtkWidget *old,
                           gpointer data)
{
	struct _EConfigHookGroup *group = data;
	EConfigHookItemFactoryData factory_data;
	EPlugin *plugin;

	factory_data.config = config;
	factory_data.item = item;
	factory_data.target = config->target;
	factory_data.parent = parent;
	factory_data.old = old;

	plugin = group->hook->hook.plugin;
	return e_plugin_invoke (plugin, item->user_data, &factory_data);
}

static GtkWidget *
ech_config_section_factory (EConfig *config,
                            EConfigItem *item,
                            GtkWidget *parent,
                            GtkWidget *old,
                            gpointer data,
			    GtkWidget **real_frame)
{
	struct _EConfigHookGroup *group = data;
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
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
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
	e_binding_new (plugin, "enabled", widget, "visible");

	parent = widget;

	widget = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (parent), widget);
	gtk_widget_show (widget);

	parent = widget;

	switch (item->type) {
		case E_CONFIG_SECTION:
			widget = gtk_vbox_new (FALSE, 6);
			break;

		case E_CONFIG_SECTION_TABLE:
			widget = gtk_table_new (1, 1, FALSE);
			gtk_table_set_col_spacings (GTK_TABLE (widget), 6);
			gtk_table_set_row_spacings (GTK_TABLE (widget), 6);
			break;

		default:
			g_return_val_if_reached (NULL);
	}

	gtk_container_add (GTK_CONTAINER (parent), widget);
	gtk_widget_show (widget);

	return widget;
}

static struct _EConfigItem *
emph_construct_item (EPluginHook *eph, EConfigHookGroup *menu, xmlNodePtr root, EConfigHookTargetMap *map)
{
	struct _EConfigItem *item;

	d(printf("  loading config item\n"));
	item = g_malloc0 (sizeof (*item));
	if ((item->type = e_plugin_hook_id(root, ech_item_types, "type")) == -1)
		goto error;
	item->path = e_plugin_xml_prop(root, "path");
	item->label = e_plugin_xml_prop_domain(root, "label", eph->plugin->domain);
	item->user_data = e_plugin_xml_prop(root, "factory");

	if (item->path == NULL
	    || (item->label == NULL && item->user_data == NULL))
		goto error;

	if (item->user_data)
		item->factory = ech_config_widget_factory;
	else if (item->type == E_CONFIG_SECTION)
		item->factory = (EConfigItemFactoryFunc) ech_config_section_factory;
	else if (item->type == E_CONFIG_SECTION_TABLE)
		item->factory = (EConfigItemFactoryFunc) ech_config_section_factory;

	d(printf("   path=%s label=%s factory=%s\n", item->path, item->label, (gchar *)item->user_data));

	return item;
error:
	d(printf("error!\n"));
	emph_free_item (item);
	return NULL;
}

static struct _EConfigHookGroup *
emph_construct_menu (EPluginHook *eph, xmlNodePtr root)
{
	struct _EConfigHookGroup *menu;
	xmlNodePtr node;
	EConfigHookTargetMap *map;
	EConfigHookClass *class = (EConfigHookClass *)G_OBJECT_GET_CLASS (eph);
	gchar *tmp;

	d(printf(" loading menu\n"));
	menu = g_malloc0 (sizeof (*menu));

	tmp = (gchar *)xmlGetProp(root, (const guchar *)"target");
	if (tmp == NULL)
		goto error;
	map = g_hash_table_lookup (class->target_map, tmp);
	xmlFree (tmp);
	if (map == NULL)
		goto error;

	menu->target_type = map->id;
	menu->id = e_plugin_xml_prop(root, "id");
	if (menu->id == NULL) {
		g_warning("Plugin '%s' missing 'id' field in group for '%s'\n", eph->plugin->name,
			  ((EPluginHookClass *)G_OBJECT_GET_CLASS (eph))->id);
		goto error;
	}
	menu->check = e_plugin_xml_prop(root, "check");
	menu->commit = e_plugin_xml_prop(root, "commit");
	menu->abort = e_plugin_xml_prop(root, "abort");
	menu->hook = (EConfigHook *)eph;
	node = root->children;
	while (node) {
		if (0 == strcmp((gchar *)node->name, "item")) {
			struct _EConfigItem *item;

			item = emph_construct_item (eph, menu, node, map);
			if (item)
				menu->items = g_slist_append (menu->items, item);
		}
		node = node->next;
	}

	return menu;
error:
	emph_free_group (menu);
	return NULL;
}

static gint
emph_construct (EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;
	EConfigClass *class;

	d(printf("loading config hook\n"));

	if (((EPluginHookClass *)e_config_hook_parent_class)->construct (eph, ep, root) == -1)
		return -1;

	class = ((EConfigHookClass *)G_OBJECT_GET_CLASS (eph))->config_class;

	node = root->children;
	while (node) {
		if (strcmp((gchar *)node->name, "group") == 0) {
			struct _EConfigHookGroup *group;

			group = emph_construct_menu (eph, node);
			if (group) {
				e_config_class_add_factory (class, group->id, ech_config_factory, group);
				emph->groups = g_slist_append (emph->groups, group);
			}
		}
		node = node->next;
	}

	eph->plugin = ep;

	return 0;
}

static void
emph_finalize (GObject *o)
{
	EPluginHook *eph = (EPluginHook *)o;

	g_slist_foreach (emph->groups, (GFunc)emph_free_group, NULL);
	g_slist_free (emph->groups);

	((GObjectClass *)e_config_hook_parent_class)->finalize (o);
}

static void
e_config_hook_class_init (EConfigHookClass *class)
{
	GObjectClass *object_class;
	EPluginHookClass *plugin_hook_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = emph_finalize;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->construct = emph_construct;

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
 * @class: The dervied EconfigHook class.
 * @map: A map used to describe a single EConfigTarget type for this
 * class.
 *
 * Add a targe tmap to a concrete derived class of EConfig.  The
 * target map enumates the target types available for the implenting
 * class.
 **/
void
e_config_hook_class_add_target_map (EConfigHookClass *class,
                                    const EConfigHookTargetMap *map)
{
	g_hash_table_insert (class->target_map, (gpointer)map->type, (gpointer)map);
}
