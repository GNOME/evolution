/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include <gtk/gtknotebook.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkalignment.h>

#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page-standard.h>
#include <libgnomeui/gnome-druid-page-edge.h>

#include "e-config.h"

#include <e-util/e-icon-factory.h>

#include <libgnome/gnome-i18n.h>

struct _EConfigFactory {
	struct _EConfigFactory *next, *prev;

	char *id;
	EConfigFactoryFunc factory;
	void *factory_data;
};

struct _menu_node {
	struct _menu_node *next, *prev;

	GSList *menu;
	EConfigItemsFunc free;
	EConfigItemsFunc abort;
	EConfigItemsFunc commit;
	void *data;
};

struct _widget_node {
	struct _widget_node *next, *prev;

	EConfig *config;

	struct _menu_node *context;
	EConfigItem *item;
	struct _GtkWidget *widget; /* widget created by the factory, if any */
	struct _GtkWidget *frame; /* if created by us */

	int empty:1;		/* set if empty (i.e. hidden) */
};

struct _check_node {
	struct _check_node *next, *prev;

	char *pageid;
	EConfigCheckFunc check;
	void *data;
};

struct _EConfigPrivate {
	EDList menus;
	EDList widgets;
	EDList checks;

	struct _widget_node *druid_page; /* current druid page if using the druid */
};

static GObjectClass *ep_parent;

static void
ep_init(GObject *o)
{
	EConfig *emp = (EConfig *)o;
	struct _EConfigPrivate *p;

	p = emp->priv = g_malloc0(sizeof(struct _EConfigPrivate));

	e_dlist_init(&p->menus);
	e_dlist_init(&p->widgets);
	e_dlist_init(&p->checks);
}

static void
ep_finalise(GObject *o)
{
	EConfig *emp = (EConfig *)o;
	struct _EConfigPrivate *p = emp->priv;
	struct _menu_node *mnode;
	struct _widget_node *wn;
	struct _check_node *cn;

	printf("finalising EConfig %p\n", o);

	g_free(emp->id);

	while ((mnode = (struct _menu_node *)e_dlist_remhead(&p->menus))) {
		if (mnode->free)
			mnode->free(emp, mnode->menu, mnode->data);

		g_free(mnode);
	}

	while ( (wn = (struct _widget_node *)e_dlist_remhead(&p->widgets)) ) {
		g_free(wn);
	}

	while ( (cn = (struct _check_node *)e_dlist_remhead(&p->widgets)) ) {
		g_free(cn->pageid);
		g_free(cn);
	}

	g_free(p);

	((GObjectClass *)ep_parent)->finalize(o);
}

static void
ec_target_free(EConfig *ep, EConfigTarget *t)
{
	g_free(t);
	g_object_unref(ep);
}

static void
ec_set_target(EConfig *emp, EConfigTarget *target)
{
	if (emp->target)
		e_config_target_free(emp, target);

	emp->target = target;
}

static void
ep_class_init(GObjectClass *klass)
{
	printf("EConfig class init %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type));

	klass->finalize = ep_finalise;
	((EConfigClass *)klass)->set_target = ec_set_target;
	((EConfigClass *)klass)->target_free = ec_target_free;
}

static void
ep_base_init(GObjectClass *klass)
{
	e_dlist_init(&((EConfigClass *)klass)->factories);
}

/**
 * e_config_get_type:
 * 
 * Standard GObject method.  Used to subclass for the concrete
 * implementations.
 * 
 * Return value: EConfig type.
 **/
GType
e_config_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EConfigClass),
			(GBaseInitFunc)ep_base_init, NULL,
			(GClassInitFunc)ep_class_init, NULL, NULL,
			sizeof(EConfig), 0,
			(GInstanceInitFunc)ep_init
		};
		ep_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EConfig", &info, 0);
	}

	return type;
}

/**
 * e_config_construct:
 * @ep: The instance to initialise.
 * @type: The type of configuration manager, @E_CONFIG_BOOK or
 * @E_CONFIG_DRUID.
 * @id: The name of the configuration window this manager drives.
 * 
 * Used by implementing classes to initialise base parameters.
 * 
 * Return value: @ep is returned.
 **/
EConfig *e_config_construct(EConfig *ep, int type, const char *id)
{
	g_assert(type == E_CONFIG_BOOK || type == E_CONFIG_DRUID);

	ep->type = type;
	ep->id = g_strdup(id);

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
e_config_add_items(EConfig *ec, GSList *items, EConfigItemsFunc commitfunc, EConfigItemsFunc abortfunc, EConfigItemsFunc freefunc, void *data)
{
	struct _menu_node *node;

	node = g_malloc(sizeof(*node));
	node->menu = items;
	node->commit = commitfunc;
	node->abort = abortfunc;
	node->free = freefunc;
	node->data = data;
	e_dlist_addtail(&ec->priv->menus, (EDListNode *)node);
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
 * the druid to continue or the notebook to close.
 **/
void
e_config_add_page_check(EConfig *ec, const char *pageid, EConfigCheckFunc check, void *data)
{
	struct _check_node *cn;

	cn = g_malloc0(sizeof(*cn));
	cn->pageid = g_strdup(pageid);
	cn->check = check;
	cn->data = data;

	e_dlist_addtail(&ec->priv->checks, (EDListNode *)cn);
}

static void
ec_add_static_items(EConfig *ec)
{
	struct _EConfigFactory *f;
	EConfigClass *klass = (EConfigClass *)G_OBJECT_GET_CLASS(ec);

	f = (struct _EConfigFactory *)klass->factories.head;
	while (f->next) {
		if (f->id == NULL
		    || !strcmp(f->id, ec->id)) {
			f->factory(ec, f->factory_data);
		}
		f = f->next;
	}
}

static int
ep_cmp(const void *ap, const void *bp)
{
	struct _widget_node *a = *((void **)ap);
	struct _widget_node *b = *((void **)bp);

	return strcmp(a->item->path, b->item->path);
}

static void
ec_druid_check_current(EConfig *ec)
{
	g_return_if_fail(ec->priv->druid_page != NULL);

	if (e_config_page_check(ec, ec->priv->druid_page->item->path)) {
		gtk_widget_set_sensitive(((GnomeDruid *)ec->widget)->next, TRUE);
	} else {
		gtk_widget_set_sensitive(((GnomeDruid *)ec->widget)->next, FALSE);
	}
}

static void
ec_druid_cancel(GnomeDruid *druid, struct _widget_node *wn)
{
	printf("finishing druid, calling abort\n");
	e_config_abort(wn->config);

	if (wn->config->window)
		gtk_widget_destroy(wn->config->window);
}

static void
ec_druid_finish(GnomeDruidPage *page, GnomeDruid *druid, struct _widget_node *wn)
{
	printf("finishing druid, calling commit\n");
	e_config_commit(wn->config);

	/* TODO: allow the commit to fail?  Do we care? */
	if (wn->config->window)
		gtk_widget_destroy(wn->config->window);
}

static void
ec_druid_prepare(GnomeDruidPage *page, GnomeDruid *druid, struct _widget_node *wn)
{
	printf("prepare page '%s'\n", wn->item->path);
	wn->config->priv->druid_page = wn;
	ec_druid_check_current(wn->config);
}

static gboolean
ec_druid_prev(GnomeDruidPage *page, GnomeDruid *druid, struct _widget_node *wn)
{
	EConfig *ec = wn->config;

	printf("prev page from '%s'\n", wn->item->path);
	if (wn->prev) {
		for (wn = wn->prev;wn->prev;wn=wn->prev) {
			if (!wn->empty
			    && (wn->item->type == E_CONFIG_PAGE
				|| wn->item->type == E_CONFIG_PAGE_START
				|| wn->item->type == E_CONFIG_PAGE_FINISH))
				break;
		}
	}

	if (wn->prev) {
		printf(" is %s\n",wn->item->path);
		gnome_druid_set_page((GnomeDruid *)ec->widget, (GnomeDruidPage *)wn->frame);
		ec->priv->druid_page = wn;
	} else {
		/* do we need to indicate first? */
		ec->priv->druid_page = NULL;
	}

	return wn->prev != NULL;
}

static gboolean
ec_druid_next(GnomeDruidPage *page, GnomeDruid *druid, struct _widget_node *wn)
{
	EConfig *ec = wn->config;

	printf("next page from '%s'\n", wn->item->path);
	if (wn->next) {
		for (wn = wn->next;wn->next;wn=wn->next) {
			if (!wn->empty
			    && (wn->item->type == E_CONFIG_PAGE
				|| wn->item->type == E_CONFIG_PAGE_START
				|| wn->item->type == E_CONFIG_PAGE_FINISH))
				break;
		}
	}

	if (wn->next) {
		printf(" is %s\n",wn->item->path);
		gnome_druid_set_page((GnomeDruid *)ec->widget, (GnomeDruidPage *)wn->frame);
		ec->priv->druid_page = wn;
	} else {
		/* do we need to indicate last? */
		ec->priv->druid_page = NULL;
	}

	return wn->next != NULL;
}

static void
ec_rebuild(EConfig *emp)
{
	struct _EConfigPrivate *p = emp->priv;
	struct _widget_node *wn, *sectionnode = NULL, *pagenode = NULL;
	GtkWidget *book = NULL, *page = NULL, *section = NULL, *root = NULL, *druid = NULL;
	int pageno = 0, sectionno = 0, itemno = 0;

	printf("target changed, rebuilding:\n");

	/* TODO: This code is pretty complex, and will probably just
	 * become more complex with time.  It could possibly be split
	 * into the two base types, but there would be a lot of code
	 * duplication */

	for (wn = (struct _widget_node *)p->widgets.head;wn->next;wn=wn->next) {
		struct _EConfigItem *item = wn->item;
		GtkWidget *w;

		printf(" '%s'\n", item->path);

		/* If the last section doesn't contain anything, hide it */
		if (sectionnode != NULL
		    && (item->type == E_CONFIG_PAGE_START
			|| item->type == E_CONFIG_PAGE_FINISH
			|| item->type == E_CONFIG_PAGE
			|| item->type == E_CONFIG_SECTION
			|| item->type == E_CONFIG_SECTION_TABLE)) {
			if ( (sectionnode->empty = itemno == 0) ) {
				gtk_widget_hide(sectionnode->frame);
				sectionno--;
			} else
				gtk_widget_show(sectionnode->frame);
			printf("%s section '%s' [sections=%d]\n", sectionnode->empty?"hiding":"showing", sectionnode->item->path, sectionno);
		}

		/* If the last page doesn't contain anything, hide it */
		if (pagenode != NULL
		    && (item->type == E_CONFIG_PAGE_START
			|| item->type == E_CONFIG_PAGE_FINISH
			|| item->type == E_CONFIG_PAGE)) {
			if ( (pagenode->empty = sectionno == 0) ) {
				gtk_widget_hide(pagenode->frame);
				pageno--;
			} else
				gtk_widget_show(pagenode->frame);
			printf("%s page '%s' [section=%d]\n", pagenode->empty?"hiding":"showing", pagenode->item->path, pageno);
		}

		/* Now process the item */
		switch (item->type) {
		case E_CONFIG_BOOK:
		case E_CONFIG_DRUID:
			/* Only one of BOOK or DRUID may be define, it
			   is used by the defining code to mark the
			   type of the config window.  It is
			   cross-checked with the code's defined
			   type. */
			if (root != NULL) {
				g_warning("EConfig book/druid redefined at: %s", item->path);
				break;
			}

			if (wn->widget == NULL) {
				if (item->type != emp->type) {
					g_warning("EConfig book/druid type mismatch");
					break;
				}
				if (item->factory) {
					root = item->factory(emp, item, NULL, wn->widget, wn->context->data);
				} else if (item->type == E_CONFIG_BOOK) {
					root = book = gtk_notebook_new();
					gtk_widget_show(book);
				} else if (item->type == E_CONFIG_DRUID) {
					root = druid = gnome_druid_new();
				} else
					abort();

				if (item->type == E_CONFIG_DRUID)
					g_signal_connect(root, "cancel", G_CALLBACK(ec_druid_cancel), wn);

				emp->widget = root;
				wn->widget = root;
			} else {
				root = wn->widget;
			}

			if (item->type == E_CONFIG_BOOK)
				book = root;
			else
				druid = root;

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
			if (emp->type != E_CONFIG_DRUID) {
				g_warning("EConfig druid start/finish pages can't be used on E_CONFIG_BOOKs");
				break;
			}

			if (wn->widget == NULL) {
				if (item->factory) {
					page = item->factory(emp, item, root, wn->frame, wn->context->data);
				} else {
					page = gnome_druid_page_edge_new(item->type == E_CONFIG_PAGE_START?GNOME_EDGE_START:GNOME_EDGE_FINISH);
					gtk_widget_show(page);
					gnome_druid_page_edge_set_title((GnomeDruidPageEdge *)page, item->label);
					gnome_druid_insert_page((GnomeDruid *)druid, pagenode?(GnomeDruidPage *)pagenode->frame:NULL, (GnomeDruidPage *)page);
				}
				if (item->type == E_CONFIG_PAGE_FINISH) {
					g_signal_connect(page, "back", G_CALLBACK(ec_druid_prev), wn);
					g_signal_connect(page, "finish", G_CALLBACK(ec_druid_finish), wn);
				} else
					g_signal_connect(page, "next", G_CALLBACK(ec_druid_next), wn);
				wn->frame = page;
				wn->widget = page;
			}
			pageno++;
			page = NULL;
			pagenode = wn; /* need this for previous page linking */
			section = NULL;
			sectionnode = NULL;
			sectionno = 1; /* never want to hide these */
			break;
		case E_CONFIG_PAGE: {
			int connect = 0; /* connect druid signals */

			/* CONFIG_PAGEs depend on the config type.
			   E_CONFIG_BOOK:
			   	The page is a VBox, stored in the notebook.
			   E_CONFIG_DRUID
			   	The page is a GnomeDruidPageStandard,
				any sections automatically added are added to
				the vbox inside it. */
			sectionno = 0;
			if (root == NULL) {
				g_warning("EConfig page defined before container widget: %s", item->path);
				break;
			}

			if (item->factory) {
				page = item->factory(emp, item, root, wn->frame, wn->context->data);
				if (emp->type == E_CONFIG_DRUID) {
					if (page) {
						g_assert(GNOME_IS_DRUID_PAGE_STANDARD(page));
						connect = wn->frame != page;
						wn->frame = page;
						page = ((GnomeDruidPageStandard *)page)->vbox;
					} else
						wn->frame = page;
				} else {
					wn->frame = page;
				}
				if (page)
					sectionno = 1;
			} else if (wn->widget == NULL) {
				if (emp->type == E_CONFIG_DRUID) {
					w = gnome_druid_page_standard_new();
					gtk_widget_show(w);
					gnome_druid_page_standard_set_title((GnomeDruidPageStandard *)w, item->label);
					gnome_druid_insert_page((GnomeDruid *)druid, pagenode?(GnomeDruidPage *)pagenode->frame:NULL, (GnomeDruidPage *)w);
					wn->frame = w;
					page = ((GnomeDruidPageStandard *)w)->vbox;
					connect = TRUE;
				} else {
					w = gtk_label_new(item->label);
					gtk_widget_show(w);
					page = gtk_vbox_new(FALSE, 12);
					gtk_container_set_border_width((GtkContainer *)page, 12);
					gtk_widget_show(page);
					gtk_notebook_insert_page((GtkNotebook *)book, page, w, pageno);
					wn->frame = page;
				}
			} else
				page = wn->widget;

			printf("page %d:%s widget %p\n", pageno, item->path, w);

			if (wn->widget && wn->widget != page) {
				printf("destroy old widget for page '%s'\n", item->path);
				gtk_widget_destroy(wn->widget);
			}

			if (connect) {
				g_signal_connect(wn->frame, "next", G_CALLBACK(ec_druid_next), wn);
				g_signal_connect(wn->frame, "back", G_CALLBACK(ec_druid_prev), wn);
				/* GnomeDruid bug, need to connect_after */
				g_signal_connect_after(wn->frame, "prepare", G_CALLBACK(ec_druid_prepare), wn);
			}

			pageno++;
			pagenode = wn;
			section = NULL;
			sectionnode = NULL;
			wn->widget = page;
			break; }
		case E_CONFIG_SECTION:
		case E_CONFIG_SECTION_TABLE:
			/* The section factory is always called with
			   the parent vbox object.  Even for druid
			   pages. */
			if (page == NULL) {
				g_warning("EConfig section '%s' has no parent page", item->path);
				section = NULL;
				goto nopage;
			}

			itemno = 0;
			if (item->factory) {
				section = item->factory(emp, item, page, wn->widget, wn->context->data);
				wn->frame = section;
				itemno = 1;

				if (section
				    && ((item->type == E_CONFIG_SECTION && !GTK_IS_BOX(section))
					|| (item->type == E_CONFIG_SECTION_TABLE && !GTK_IS_TABLE(section))))
					g_warning("ECofnig section type is wrong");
			} else if (wn->widget == NULL) {
				GtkWidget *frame;
				GtkWidget *label = NULL;

				if (item->label) {
					char *txt = g_strdup_printf("<span weight=\"bold\">%s</span>", item->label);

					label = g_object_new(gtk_label_get_type(),
							     "label", txt,
							     "use_markup", TRUE,
							     "xalign", 0.0, NULL);
					g_free(txt);
				}

				if (item->type == E_CONFIG_SECTION)
					section = gtk_vbox_new(FALSE, 6);
				else
					section = gtk_table_new(1, 1, FALSE);

				frame = g_object_new(gtk_frame_get_type(),
						     "shadow_type", GTK_SHADOW_NONE, 
						     "label_widget", label,
						     "child", g_object_new(gtk_alignment_get_type(),
									   "left_padding", 12,
									   "top_padding", 6,
									   "child", section, NULL),
						     NULL);
				gtk_widget_show_all(frame);
				gtk_box_pack_start((GtkBox *)page, frame, FALSE, FALSE, 0);
				wn->frame = frame;
			} else {
				section = wn->widget;
			}
		nopage:
			if (wn->widget && wn->widget != section) {
				printf("destroy old widget for section '%s'\n", item->path);
				gtk_widget_destroy(wn->widget);
			}

			sectionno++;
			wn->widget = section;
			sectionnode = wn;
			break;
		case E_CONFIG_ITEM:
		case E_CONFIG_ITEM_TABLE:
			/* ITEMs are called with the section parent.
			   The type depends on the section type,
			   either a GtkTable, or a GtkVBox */
			w = NULL;
			if (section == NULL)
				g_warning("EConfig item has no parent section: %s", item->path);
			else if ((item->type == E_CONFIG_ITEM && !GTK_IS_BOX(section))
				 || (item->type == E_CONFIG_ITEM_TABLE && !GTK_IS_TABLE(section)))
				g_warning("EConfig item parent type is incorrect: %s", item->path);
			else if (item->factory)
				w = item->factory(emp, item, section, wn->widget, wn->context->data);

			printf("item %d:%s widget %p\n", itemno, item->path, w);

			if (wn->widget && wn->widget != w) {
				printf("destroy old widget for item '%s'\n", item->path);
				gtk_widget_destroy(wn->widget);
			}

			wn->widget = w;
			if (w)
				itemno++;
			break;
		}
	}

	if (book) {
		/* make this depend on flags?? */
		if (gtk_notebook_get_n_pages((GtkNotebook *)book) == 1) {
			gtk_notebook_set_show_tabs((GtkNotebook *)book, FALSE);
			gtk_notebook_set_show_border((GtkNotebook *)book, FALSE);
		}
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
e_config_set_target(EConfig *emp, EConfigTarget *target)
{
	if (emp->target != target)
		((EConfigClass *)G_OBJECT_GET_CLASS(emp))->set_target(emp, target);
}

static void
ec_widget_destroy(GtkWidget *w, EConfig *ec)
{
	if (ec->target) {
		e_config_target_free(ec, ec->target);
		ec->target = NULL;
	}

	g_object_unref(ec);
}

/**
 * e_config_create_widget:
 * @emp: An initialised EConfig object.
 * 
 * Create the widget described by @emp.  Only the core widget
 * appropriate for the given type is created, i.e. a GtkNotebook for
 * the E_CONFIG_BOOK type and a GnomeDruid for the E_CONFIG_DRUID
 * type.
 * 
 * This object will be self-driving, but will not close itself once
 * complete.
 *
 * Unless reffed otherwise, the management object @emp will be
 * finalised when the widget is.
 *
 * Return value: The widget, also available in @emp.widget
 **/
GtkWidget *
e_config_create_widget(EConfig *emp)
{
	struct _EConfigPrivate *p = emp->priv;
	struct _menu_node *mnode;
	GPtrArray *items = g_ptr_array_new();
	GSList *l;
	/*char *domain = NULL;*/
	int i;

	g_assert(emp->target != NULL);

	ec_add_static_items(emp);

	/* FIXME: need to override old ones with new names */
	for (mnode = (struct _menu_node *)p->menus.head;mnode->next;mnode=mnode->next)
		for (l=mnode->menu; l; l = l->next) {
			struct _EConfigItem *item = l->data;
			struct _widget_node *wn = g_malloc0(sizeof(*wn));

			wn->item = item;
			wn->context = mnode;
			wn->config = emp;
			g_ptr_array_add(items, wn);
		}

	qsort(items->pdata, items->len, sizeof(items->pdata[0]), ep_cmp);

	for (i=0;i<items->len;i++) {
		struct _widget_node *wn = items->pdata[i];

		e_dlist_addtail(&p->widgets, (EDListNode *)wn);
	}

	g_ptr_array_free(items, TRUE);
	ec_rebuild(emp);

	/* auto-unref it */
	g_signal_connect(emp->widget, "destroy", G_CALLBACK(ec_widget_destroy), emp);

	/* FIXME: for some reason ec_rebuild puts the widget on page 1, this is just to override that */
	if (emp->type == E_CONFIG_BOOK)
		gtk_notebook_set_current_page((GtkNotebook *)emp->widget, 0);

	return emp->widget;
}

static void
ec_dialog_response(GtkWidget *d, int id, EConfig *ec)
{
	if (id == GTK_RESPONSE_OK)
		e_config_commit(ec);
	else
		e_config_abort(ec);

	gtk_widget_destroy(d);
}

/**
 * e_config_create_window:
 * @emp: Initialised and configured EMConfig derived instance.
 * @parent: Parent window or NULL.
 * @title: Title of window or dialog.
 * 
 * Create a managed GtkWindow object from @emp.  This window will be
 * fully driven by the EConfig @emp.  If @emp.type is
 * @E_CONFIG_DRUID, then this will be a toplevel GtkWindow containing
 * a GnomeDruid.  If it is @E_CONFIG_BOOK then it will be a GtkDialog
 * containing a Nnotebook.
 *
 * Unless reffed otherwise, the management object @emp will be
 * finalised when the widget is.
 * 
 * Return value: The window widget.  This is also stored in @emp.window.
 **/
GtkWidget *
e_config_create_window(EConfig *emp, struct _GtkWindow *parent, const char *title)
{
	GtkWidget *w;

	e_config_create_widget(emp);

	if (emp->type == E_CONFIG_BOOK) {
		w = gtk_dialog_new_with_buttons(title, parent, GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OK, GTK_RESPONSE_OK,
						NULL);
		g_signal_connect(w, "response", G_CALLBACK(ec_dialog_response), emp);
		gtk_box_pack_start((GtkBox *)((GtkDialog *)w)->vbox, emp->widget, TRUE, TRUE, 0);
	} else {
		/* response is handled directly by the druid stuff */
		w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_container_add((GtkContainer *)w, emp->widget);
		gtk_window_set_type_hint((GtkWindow *)w, GDK_WINDOW_TYPE_HINT_DIALOG);
	}

	emp->window = w;
	gtk_widget_show(w);

	return w;
}

/**
 * e_config_target_changed:
 * @emp: 
 * @how: 
 * 
 * Indicate that the target has changed.  This may be called by the
 * self-aware target itself, or by the driving code.  If @how is
 * %E_CONFIG_TARGET_CHANGED_REBUILD, then the entire configuration
 * widget may be recreated based on the changed target.
 *
 * This is used to sensitise Druid next/back buttons and the Apply
 * button for the Notebook mode.
 **/
void e_config_target_changed(EConfig *emp, e_config_target_change_t how)
{
	if (how == E_CONFIG_TARGET_CHANGED_REBUILD)
		ec_rebuild(emp);

	if (emp->type == E_CONFIG_DRUID) {
		if (emp->priv->druid_page)
			ec_druid_check_current(emp);
	} else {
		if (emp->window) {
			if (e_config_page_check(emp, NULL)) {
				gtk_dialog_set_response_sensitive((GtkDialog *)emp->window, GTK_RESPONSE_OK, TRUE);
			} else {
				gtk_dialog_set_response_sensitive((GtkDialog *)emp->window, GTK_RESPONSE_OK, FALSE);
			}
		}
	}

	/* virtual method/signal? */
}

/**
 * e_config_abort:
 * @ec: 
 * 
 * Signify that the stateful configuration changes must be discarded
 * to all listeners.  This is used by self-driven druid or notebook, or
 * may be used by code using the widget directly.
 **/
void e_config_abort(EConfig *ec)
{
	struct _EConfigPrivate *p = ec->priv;
	struct _menu_node *mnode;

	/* TODO: should these just be signals? */

	for (mnode = (struct _menu_node *)p->menus.head;mnode->next;mnode=mnode->next)
		if (mnode->abort)
			mnode->abort(ec, mnode->menu, mnode->data);
}

/**
 * e_config_commit:
 * @ec: 
 * 
 * Signify that the stateful configuration changes should be saved.
 * This is used by the self-driven druid or notebook, or may be used
 * by code driving the widget directly.
 **/
void e_config_commit(EConfig *ec)
{
	struct _EConfigPrivate *p = ec->priv;
	struct _menu_node *mnode;

	/* TODO: should these just be signals? */

	for (mnode = (struct _menu_node *)p->menus.head;mnode->next;mnode=mnode->next)
		if (mnode->commit)
			mnode->commit(ec, mnode->menu, mnode->data);
}

/**
 * e_config_page_check:
 * @ec: 
 * @pageid: The path of the page item.
 * 
 * Check that a given page is complete.  If @pageid is NULL, then check
 * the whole config.  No check is made that the page actually exists.
 * 
 * Return value: FALSE if the data is inconsistent/incomplete.
 **/
gboolean e_config_page_check(EConfig *ec, const char *pageid)
{
	struct _EConfigPrivate *p = ec->priv;
	struct _check_node *mnode;

	for (mnode = (struct _check_node *)p->checks.head;mnode->next;mnode=mnode->next)
		if ((pageid == NULL
		     || mnode->pageid == NULL
		     || strcmp(mnode->pageid, pageid) == 0)
		    && !mnode->check(ec, pageid, mnode->data))
			return FALSE;

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
 * container or the GnomeDruidPage object.
 **/
GtkWidget *e_config_page_get(EConfig *ec, const char *pageid)
{
	struct _widget_node *wn;

	for (wn = (struct _widget_node *)ec->priv->widgets.head;wn->next;wn=wn->next)
		if (!wn->empty
		    && (wn->item->type == E_CONFIG_PAGE
			|| wn->item->type == E_CONFIG_PAGE_START
			|| wn->item->type == E_CONFIG_PAGE_FINISH)
		    && !strcmp(wn->item->path, pageid))
			return wn->frame;

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
const char *e_config_page_next(EConfig *ec, const char *pageid)
{
	struct _widget_node *wn;
	int found;

	found = pageid == NULL ? 1:0;
	for (wn = (struct _widget_node *)ec->priv->widgets.head;wn->next;wn=wn->next)
		if (!wn->empty
		    && (wn->item->type == E_CONFIG_PAGE
			|| wn->item->type == E_CONFIG_PAGE_START
			|| wn->item->type == E_CONFIG_PAGE_FINISH)) {
			if (found)
				return wn->item->path;
			else if (strcmp(wn->item->path, pageid) == 0)
				found = 1;
		}

	return NULL;
}

/**
 * e_config_page_next:
 * @ec: 
 * @pageid: The path of the page item.
 * 
 * Find the path of the previous visible page before @pageid.  If @pageid
 * is NULL then find the last visible page.
 * 
 * Return value: The path of the previous page, or @NULL if @pageid was the
 * first configured and visible page.
 **/
const char *e_config_page_prev(EConfig *ec, const char *pageid)
{
	struct _widget_node *wn;
	int found;

	found = pageid == NULL ? 1:0;
	for (wn = (struct _widget_node *)ec->priv->widgets.tailpred;wn->prev;wn=wn->prev)
		if (!wn->empty
		    && (wn->item->type == E_CONFIG_PAGE
			|| wn->item->type == E_CONFIG_PAGE_START
			|| wn->item->type == E_CONFIG_PAGE_FINISH)) {
			if (found)
				return wn->item->path;
			else if (strcmp(wn->item->path, pageid) == 0)
				found = 1;
		}

	return NULL;
}

/* ********************************************************************** */

/**
 * e_config_class_add_factory:
 * @klass: Implementing class pointer.
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
e_config_class_add_factory(EConfigClass *klass, const char *id, EConfigFactoryFunc func, void *data)
{
	struct _EConfigFactory *f = g_malloc0(sizeof(*f));

	f->id = g_strdup(id);
	f->factory = func;
	f->factory_data = data;
	e_dlist_addtail(&klass->factories, (EDListNode *)f);

	return f;
}

/**
 * e_config_class_remove_factory:
 * @f: Handle from :class_add_factory() call.
 * 
 * Remove a config factory.  The handle @f may only be removed once.
 **/
void
e_config_class_remove_factory(EConfigClass *klass, EConfigFactory *f)
{
	e_dlist_remove((EDListNode *)f);
	g_free(f->id);
	g_free(f);
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
void *e_config_target_new(EConfig *ep, int type, size_t size)
{
	EConfigTarget *t;

	g_assert(size >= sizeof(EConfigTarget));

	t = g_malloc0(size);
	t->config = ep;
	g_object_ref(ep);
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
e_config_target_free(EConfig *ep, void *o)
{
	EConfigTarget *t = o;

	((EConfigClass *)G_OBJECT_GET_CLASS(ep))->target_free(ep, t);
}

/* ********************************************************************** */

/* Config menu plugin handler */

/*
<e-plugin
  class="com.ximian.mail.plugin.config:1.0"
  id="com.ximian.mail.plugin.config.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="com.ximian.mail.configMenu:1.0"
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

static void *emph_parent_class;
#define emph ((EConfigHook *)eph)

static const EPluginHookTargetKey ech_item_types[] = {
	{ "book", E_CONFIG_BOOK },
	{ "druid", E_CONFIG_DRUID },

	{ "page", E_CONFIG_PAGE },
	{ "page_start", E_CONFIG_PAGE_START },
	{ "page_finish", E_CONFIG_PAGE_FINISH },
	{ "section", E_CONFIG_SECTION },
	{ "section_table", E_CONFIG_SECTION_TABLE },
	{ "item", E_CONFIG_ITEM },
	{ "item_table", E_CONFIG_ITEM_TABLE },
	{ 0 },
};

static void
ech_commit(EConfig *ec, GSList *items, void *data)
{
	struct _EConfigHookGroup *group = data;

	if (group->commit)
		e_plugin_invoke(group->hook->hook.plugin, group->commit, ec->target);
}

static void
ech_abort(EConfig *ec, GSList *items, void *data)
{
	struct _EConfigHookGroup *group = data;

	if (group->abort)
		e_plugin_invoke(group->hook->hook.plugin, group->abort, ec->target);
}

static gboolean
ech_check(EConfig *ec, const char *pageid, void *data)
{
	struct _EConfigHookGroup *group = data;
	EConfigHookPageCheckData hdata;

	hdata.config = ec;
	hdata.target = ec->target;
	hdata.pageid = pageid?pageid:"";

	return GPOINTER_TO_INT(e_plugin_invoke(group->hook->hook.plugin, group->check, &hdata));
}

static void
ech_config_factory(EConfig *emp, void *data)
{
	struct _EConfigHookGroup *group = data;

	printf("config factory called %s\n", group->id?group->id:"all menus");

	if (emp->target->type != group->target_type)
		return;

	if (group->items)
		e_config_add_items(emp, group->items, ech_commit, ech_abort, NULL, group);

	if (group->check)
		e_config_add_page_check(emp, NULL, ech_check, group);
}

static void
emph_free_item(struct _EConfigItem *item)
{
	g_free(item->path);
	g_free(item->label);
	g_free(item->user_data);
	g_free(item);
}

static void
emph_free_group(struct _EConfigHookGroup *group)
{
	g_slist_foreach(group->items, (GFunc)emph_free_item, NULL);
	g_slist_free(group->items);

	g_free(group->id);
	g_free(group);
}

static struct _GtkWidget *
ech_config_widget_factory(EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, void *data)
{
	struct _EConfigHookGroup *group = data;
	EConfigHookItemFactoryData hdata;

	hdata.config = ec;
	hdata.item = item;
	hdata.target = ec->target;
	hdata.parent = parent;
	hdata.old = old;

	return (struct _GtkWidget *)e_plugin_invoke(group->hook->hook.plugin, (char *)item->user_data, &hdata);
}

static struct _EConfigItem *
emph_construct_item(EPluginHook *eph, EConfigHookGroup *menu, xmlNodePtr root, EConfigHookTargetMap *map)
{
	struct _EConfigItem *item;

	printf("  loading config item\n");
	item = g_malloc0(sizeof(*item));
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

	printf("   path=%s label=%s factory=%s\n", item->path, item->label, (char *)item->user_data);

	return item;
error:
	printf("error!\n");
	emph_free_item(item);
	return NULL;
}

static struct _EConfigHookGroup *
emph_construct_menu(EPluginHook *eph, xmlNodePtr root)
{
	struct _EConfigHookGroup *menu;
	xmlNodePtr node;
	EConfigHookTargetMap *map;
	EConfigHookClass *klass = (EConfigHookClass *)G_OBJECT_GET_CLASS(eph);
	char *tmp;

	printf(" loading menu\n");
	menu = g_malloc0(sizeof(*menu));

	tmp = xmlGetProp(root, "target");
	if (tmp == NULL)
		goto error;
	map = g_hash_table_lookup(klass->target_map, tmp);
	xmlFree(tmp);
	if (map == NULL)
		goto error;

	menu->target_type = map->id;
	menu->id = e_plugin_xml_prop(root, "id");
	menu->check = e_plugin_xml_prop(root, "check");
	menu->commit = e_plugin_xml_prop(root, "commit");
	menu->abort = e_plugin_xml_prop(root, "abort");
	menu->hook = (EConfigHook *)eph;
	node = root->children;
	while (node) {
		if (0 == strcmp(node->name, "item")) {
			struct _EConfigItem *item;

			item = emph_construct_item(eph, menu, node, map);
			if (item)
				menu->items = g_slist_append(menu->items, item);
		}
		node = node->next;
	}

	return menu;
error:
	emph_free_group(menu);
	return NULL;
}

static int
emph_construct(EPluginHook *eph, EPlugin *ep, xmlNodePtr root)
{
	xmlNodePtr node;
	EConfigClass *klass;

	printf("loading config hook\n");

	if (((EPluginHookClass *)emph_parent_class)->construct(eph, ep, root) == -1)
		return -1;

	klass = ((EConfigHookClass *)G_OBJECT_GET_CLASS(eph))->config_class;

	node = root->children;
	while (node) {
		if (strcmp(node->name, "group") == 0) {
			struct _EConfigHookGroup *group;

			group = emph_construct_menu(eph, node);
			if (group) {
				e_config_class_add_factory(klass, group->id, ech_config_factory, group);
				emph->groups = g_slist_append(emph->groups, group);
			}
		}
		node = node->next;
	}

	eph->plugin = ep;

	return 0;
}

static void
emph_finalise(GObject *o)
{
	EPluginHook *eph = (EPluginHook *)o;

	g_slist_foreach(emph->groups, (GFunc)emph_free_group, NULL);
	g_slist_free(emph->groups);

	((GObjectClass *)emph_parent_class)->finalize(o);
}

static void
emph_class_init(EPluginHookClass *klass)
{
	((GObjectClass *)klass)->finalize = emph_finalise;
	klass->construct = emph_construct;

	/* this is actually an abstract implementation but list it anyway */
	klass->id = "com.ximian.evolution.config:1.0";

	printf("EConfigHook: init class %p '%s'\n", klass, g_type_name(((GObjectClass *)klass)->g_type_class.g_type));

	((EConfigHookClass *)klass)->target_map = g_hash_table_new(g_str_hash, g_str_equal);
	((EConfigHookClass *)klass)->config_class = g_type_class_ref(e_config_get_type());
}

/**
 * e_config_hook_get_type:
 * 
 * Standard GObject function to get the object type.
 * 
 * Return value: The EConfigHook class type.
 **/
GType
e_config_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EConfigHookClass), NULL, NULL, (GClassInitFunc) emph_class_init, NULL, NULL,
			sizeof(EConfigHook), 0, (GInstanceInitFunc) NULL,
		};

		emph_parent_class = g_type_class_ref(e_plugin_hook_get_type());
		type = g_type_register_static(e_plugin_hook_get_type(), "EConfigHook", &info, 0);
	}
	
	return type;
}

/**
 * e_config_hook_class_add_target_map:
 *
 * @klass: The dervied EconfigHook class.
 * @map: A map used to describe a single EConfigTarget type for this
 * class.
 * 
 * Add a targe tmap to a concrete derived class of EConfig.  The
 * target map enumates the target types available for the implenting
 * class.
 **/
void e_config_hook_class_add_target_map(EConfigHookClass *klass, const EConfigHookTargetMap *map)
{
	g_hash_table_insert(klass->target_map, (void *)map->type, (void *)map);
}
