
#include <string.h>
#include <stdlib.h>

#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>

#include <libgnome/gnome-url.h>

#include <glib.h>

#include "em-popup.h"
#include "e-util/e-msgport.h"
#include "em-utils.h"

#include <camel/camel-folder.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-string-utils.h>

static void emp_standard_menu_factory(EMPopup *emp, EMPopupTarget *target, void *data);

struct _EMPopupFactory {
	struct _EMPopupFactory *next, *prev;

	char *menuid;
	EMPopupFactoryFunc factory;
	void *factory_data;
};

struct _menu_node {
	struct _menu_node *next, *prev;

	GSList *menu;
	GDestroyNotify freefunc;
};

struct _EMPopupPrivate {
	EDList menus;
};

static EDList emp_factories = E_DLIST_INITIALISER(emp_factories);

static GObjectClass *emp_parent;

static void
emp_init(GObject *o)
{
	EMPopup *emp = (EMPopup *)o;
	struct _EMPopupPrivate *p;

	p = emp->priv = g_malloc0(sizeof(struct _EMPopupPrivate));

	e_dlist_init(&p->menus);
}

static void
emp_finalise(GObject *o)
{
	EMPopup *emp = (EMPopup *)o;
	struct _EMPopupPrivate *p = emp->priv;
	struct _menu_node *mnode, *nnode;

	g_free(emp->menuid);

	mnode = (struct _menu_node *)p->menus.head;
	nnode = mnode->next;
	while (nnode) {
		if (mnode->freefunc)
			mnode->freefunc(mnode->menu);

		g_free(mnode);
		mnode = nnode;
		nnode = nnode->next;
	}

	g_free(p);

	((GObjectClass *)emp_parent)->finalize(o);
}

static void
emp_class_init(GObjectClass *klass)
{
	klass->finalize = emp_finalise;
}

GType
em_popup_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMPopupClass),
			NULL, NULL,
			(GClassInitFunc)emp_class_init,
			NULL, NULL,
			sizeof(EMPopup), 0,
			(GInstanceInitFunc)emp_init
		};
		emp_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EMPopup", &info, 0);

		/* FIXME: this should probably sit somewhere in global setup */
		em_popup_static_add_factory(NULL, (EMPopupFactoryFunc)emp_standard_menu_factory, NULL);
	}

	return type;
}

EMPopup *em_popup_new(const char *menuid)
{
	EMPopup *emp = g_object_new(em_popup_get_type(), 0);

	emp->menuid = g_strdup(menuid);

	return emp;
}

/**
 * em_popup_add_items:
 * @emp: 
 * @items: 
 * @freefunc: 
 * 
 * Add new EMPopupItems to the menu's.  Any with the same path
 * will override previously defined menu items, at menu building
 * time.
 **/
void
em_popup_add_items(EMPopup *emp, GSList *items, GDestroyNotify freefunc)
{
	struct _menu_node *node;

	node = g_malloc(sizeof(*node));
	node->menu = items;
	node->freefunc = freefunc;
	e_dlist_addtail(&emp->priv->menus, (EDListNode *)node);
}

/**
 * em_popup_add_static_items:
 * @emp: 
 * @target: Target of this menu.
 * 
 * Will load up any matching menu items from an installed
 * popup factory.  If the menuid of @emp is NULL, then this
 * has no effect.
 *
 **/
void
em_popup_add_static_items(EMPopup *emp, EMPopupTarget *target)
{
	struct _EMPopupFactory *f;

	if (emp->menuid == NULL || target == NULL)
		return;

	/* setup the menu itself */
	f = (struct _EMPopupFactory *)emp_factories.head;
	while (f->next) {
		if (f->menuid == NULL
		    || !strcmp(f->menuid, emp->menuid)) {
			f->factory(emp, target, f->factory_data);
		}
		f = f->next;
	}
}

static int
emp_cmp(const void *ap, const void *bp)
{
	struct _EMPopupItem *a = *((void **)ap);
	struct _EMPopupItem *b = *((void **)bp);

	return strcmp(a->path, b->path);
}

/**
 * em_popup_create:
 * @menuitems: 
 * @hide_mask: used to hide menu items, not sure of it's utility,
 * since you could just 'not add them' in the first place.  Saves
 * copying logic anyway.
 * @disable_mask: used to disable menu items.
 * 
 * TEMPORARY code to create a menu from a list of items.
 * 
 * The menu items are merged based on their path element, and
 * built into a menu tree.
 *
 * Return value: 
 **/
GtkMenu *
em_popup_create_menu(EMPopup *emp, guint32 hide_mask, guint32 disable_mask)
{
	struct _EMPopupPrivate *p = emp->priv;
	struct _menu_node *mnode, *nnode;
	GPtrArray *items = g_ptr_array_new();
	GSList *l;
	GString *ppath = g_string_new("");
	GtkMenu *topmenu;
	GHashTable *menu_hash = g_hash_table_new(g_str_hash, g_str_equal),
		*group_hash = g_hash_table_new(g_str_hash, g_str_equal);
	/*char *domain = NULL;*/
	int i;

	/* FIXME: need to override old ones with new names */
	mnode = (struct _menu_node *)p->menus.head;
	nnode = mnode->next;
	while (nnode) {
		for (l=mnode->menu; l; l = l->next)
			g_ptr_array_add(items, l->data);
		mnode = nnode;
		nnode = nnode->next;
	}

	qsort(items->pdata, items->len, sizeof(items->pdata[0]), emp_cmp);

	topmenu = (GtkMenu *)gtk_menu_new();
	for (i=0;i<items->len;i++) {
		GtkWidget *label;
		struct _EMPopupItem *item = items->pdata[i];
		GtkMenu *thismenu;
		GtkMenuItem *menuitem;
		char *tmp;

		/* for bar's, the mask is exclusive or */
		if (item->mask) {
			if ((item->type & EM_POPUP_TYPE_MASK) == EM_POPUP_BAR) {
				if ((item->mask & hide_mask) == item->mask)
					continue;
			} else if (item->mask & hide_mask)
				continue;
		}

		g_string_truncate(ppath, 0);
		tmp = strrchr(item->path, '/');
		if (tmp) {
			g_string_append_len(ppath, item->path, tmp-item->path);
			thismenu = g_hash_table_lookup(menu_hash, ppath->str);
			if (thismenu == NULL)
				continue;
		} else {
			thismenu = topmenu;
		}

		switch (item->type & EM_POPUP_TYPE_MASK) {
		case EM_POPUP_ITEM:
			if (item->image) {
				char *path;
				GtkWidget *image;

				path = g_build_filename(EVOLUTION_IMAGES, (char *)item->image, NULL);
				image = gtk_image_new_from_file(path);
				g_free(path);

				gtk_widget_show(image);
				menuitem = (GtkMenuItem *)gtk_image_menu_item_new();
				gtk_image_menu_item_set_image((GtkImageMenuItem *)menuitem, image);
			} else {
				menuitem = (GtkMenuItem *)gtk_menu_item_new();
			}
			break;
		case EM_POPUP_TOGGLE:
			menuitem = (GtkMenuItem *)gtk_check_menu_item_new();
			gtk_check_menu_item_set_active((GtkCheckMenuItem *)menuitem, item->type & EM_POPUP_ACTIVE);
			break;
		case EM_POPUP_RADIO:
			menuitem = (GtkMenuItem *)gtk_radio_menu_item_new(g_hash_table_lookup(group_hash, ppath->str));
			g_hash_table_insert(group_hash, ppath->str, gtk_radio_menu_item_get_group((GtkRadioMenuItem *)menuitem));
			gtk_check_menu_item_set_active((GtkCheckMenuItem *)menuitem, item->type & EM_POPUP_ACTIVE);
			break;
		case EM_POPUP_IMAGE:
			menuitem = (GtkMenuItem *)gtk_image_menu_item_new();
			gtk_image_menu_item_set_image((GtkImageMenuItem *)menuitem, item->image);
			break;
		case EM_POPUP_SUBMENU: {
			GtkMenu *submenu = (GtkMenu *)gtk_menu_new();

			g_hash_table_insert(menu_hash, item->path, submenu);
			menuitem = (GtkMenuItem *)gtk_menu_item_new();
			gtk_menu_item_set_submenu(menuitem, (GtkWidget *)submenu);
			break; }
		case EM_POPUP_BAR:
			/* TODO: double-bar, end-bar stuff? */
			menuitem = (GtkMenuItem *)gtk_separator_menu_item_new();
			break;
		default:
			continue;
		}

		if (item->label) {
			label = gtk_label_new_with_mnemonic(item->label);
			gtk_misc_set_alignment((GtkMisc *)label, 0.0, 0.5);
			gtk_widget_show(label);
			gtk_container_add((GtkContainer *)menuitem, label);
		}

		if (item->activate)
			g_signal_connect(menuitem, "activate", item->activate, item->activate_data);

		gtk_menu_shell_append((GtkMenuShell *)thismenu, (GtkWidget *)menuitem);

		if (item->mask & disable_mask)
			gtk_widget_set_sensitive((GtkWidget *)menuitem, FALSE);

		gtk_widget_show((GtkWidget *)menuitem);
	}

	g_string_free(ppath, TRUE);
	g_ptr_array_free(items, TRUE);
	g_hash_table_destroy(menu_hash);
	g_hash_table_destroy(group_hash);

	return topmenu;
}

static void
emp_popup_done(GtkWidget *w, EMPopup *emp)
{
	gtk_widget_destroy(w);
	g_object_unref(emp);
}

/**
 * em_popup_create_menu_once:
 * @emp: EMPopup, once the menu is shown, this cannot be
 * considered a valid pointer.
 * @target: If set, the target of the selection.  Static menu
 * items will be added.  The target will be freed once complete.
 * @hide_mask: 
 * @disable_mask: 
 * 
 * Like popup_create_menu, but automatically sets up the menu
 * so that it is destroyed once a selection takes place, and
 * the EMPopup is unreffed.
 * 
 * Return value: A menu, to popup.
 **/
GtkMenu *
em_popup_create_menu_once(EMPopup *emp, EMPopupTarget *target, guint32 hide_mask, guint32 disable_mask)
{
	GtkMenu *menu;

	if (target)
		em_popup_add_static_items(emp, target);

	menu = em_popup_create_menu(emp, hide_mask, disable_mask);

	g_signal_connect_swapped(menu, "selection_done", G_CALLBACK(em_popup_target_free), target);
	g_signal_connect(menu, "selection_done", G_CALLBACK(emp_popup_done), emp);

	return menu;
}

/* ********************************************************************** */

/**
 * em_popup_static_add_factory:
 * @menuid: 
 * @func: 
 * @data: 
 * 
 * Add a popup factory which will be called to add_items() any
 * extra menu's if wants to do the current PopupTarget.
 *
 * TODO: Make the menuid a pattern?
 * 
 * Return value: A handle to the factory.
 **/
EMPopupFactory *
em_popup_static_add_factory(const char *menuid, EMPopupFactoryFunc func, void *data)
{
	struct _EMPopupFactory *f = g_malloc0(sizeof(*f));

	f->menuid = g_strdup(menuid);
	f->factory = func;
	f->factory_data = data;
	e_dlist_addtail(&emp_factories, (EDListNode *)f);

	return f;
}

/**
 * em_popup_static_remove_factory:
 * @f: 
 * 
 * Remove a popup factory.
 **/
void
em_popup_static_remove_factory(EMPopupFactory *f)
{
	e_dlist_remove((EDListNode *)f);
	g_free(f->menuid);
	g_free(f);
}

/**
 * em_popup_target_new_select:
 * @folder: The selection will ref this for the life of it.
 * @folder_uri: 
 * @uids: The selection will free this when done with it.
 * 
 * Create a new selection popup target.
 * 
 * Return value: 
 **/
EMPopupTarget *
em_popup_target_new_select(struct _CamelFolder *folder, const char *folder_uri, GPtrArray *uids)
{
	EMPopupTarget *t = g_malloc0(sizeof(*t));
	guint32 mask = ~0;
	int i;
	const char *tmp;

	t->type = EM_POPUP_TARGET_SELECT;
	t->data.select.uids = uids;
	t->data.select.folder = folder;
	camel_object_ref(folder);
	t->data.select.folder_uri = g_strdup(folder_uri);

	if (em_utils_folder_is_sent(folder, folder_uri))
		mask &= ~EM_POPUP_SELECT_RESEND;

	if (!(em_utils_folder_is_drafts(folder, folder_uri)
	      || em_utils_folder_is_outbox(folder, folder_uri)))
		mask &= ~EM_POPUP_SELECT_ADD_SENDER;

	if (uids->len == 1)
		mask &= ~EM_POPUP_SELECT_ONE;

	if (uids->len >= 1)
		mask &= ~EM_POPUP_SELECT_MANY;

	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info = camel_folder_get_message_info(folder, uids->pdata[i]);

		if (info == NULL)
			continue;

		if (info->flags & CAMEL_MESSAGE_SEEN)
			mask &= ~EM_POPUP_SELECT_MARK_UNREAD;
		else
			mask &= ~EM_POPUP_SELECT_MARK_READ;
		
		if (info->flags & CAMEL_MESSAGE_DELETED)
			mask &= ~EM_POPUP_SELECT_UNDELETE;
		else
			mask &= ~EM_POPUP_SELECT_DELETE;

		if (info->flags & CAMEL_MESSAGE_FLAGGED)
			mask &= ~EM_POPUP_SELECT_MARK_UNIMPORTANT;
		else
			mask &= ~EM_POPUP_SELECT_MARK_IMPORTANT;

		if (info->flags & CAMEL_MESSAGE_JUNK)
			mask &= ~EM_POPUP_SELECT_MARK_NOJUNK;
		else
			mask &= ~EM_POPUP_SELECT_MARK_JUNK;
			
		tmp = camel_tag_get (&info->user_tags, "follow-up");
		if (tmp && *tmp) {
			mask &= ~EM_POPUP_SELECT_FLAG_CLEAR;
			tmp = camel_tag_get(&info->user_tags, "completed-on");
			if (tmp == NULL || *tmp == 0)
				mask &= ~EM_POPUP_SELECT_FLAG_COMPLETED;
		} else
			mask &= ~EM_POPUP_SELECT_FLAG_FOLLOWUP;

		if (i == 0 && uids->len == 1
		    && (tmp = camel_message_info_mlist(info))
		    && tmp[0] != 0)
			mask &= ~EM_POPUP_SELECT_MAILING_LIST;

		camel_folder_free_message_info(folder, info);
	}

	t->mask = mask;

	return t;
}

EMPopupTarget *
em_popup_target_new_uri(const char *uri)
{
	EMPopupTarget *t = g_malloc0(sizeof(*t));
	guint32 mask = ~0;

	t->type = EM_POPUP_TARGET_URI;
	t->data.uri = g_strdup(uri);

	if (g_ascii_strncasecmp(uri, "http:", 5) == 0
	    || g_ascii_strncasecmp(uri, "https:", 6) == 0)
		mask &= ~EM_POPUP_URI_HTTP;
	if (g_ascii_strncasecmp(uri, "mailto:", 7) == 0)
		mask &= ~EM_POPUP_URI_MAILTO;
	else
		mask &= ~EM_POPUP_URI_NOT_MAILTO;

	t->mask = mask;

	return t;
}

EMPopupTarget *
em_popup_target_new_part(struct _CamelMimePart *part, const char *mime_type)
{
	EMPopupTarget *t = g_malloc0(sizeof(*t));
	guint32 mask = ~0;

	t->type = EM_POPUP_TARGET_PART;
	t->data.part.part = part;
	camel_object_ref(part);
	if (mime_type)
		t->data.part.mime_type = g_strdup(mime_type);
	else
		t->data.part.mime_type = camel_data_wrapper_get_mime_type((CamelDataWrapper *)part);

	camel_strdown(t->data.part.mime_type);

	if (CAMEL_IS_MIME_MESSAGE(camel_medium_get_content_object((CamelMedium *)part)))
		mask &= ~EM_POPUP_PART_MESSAGE;

	if (strncmp(t->data.part.mime_type, "image/", 6) == 0)
		mask &= ~EM_POPUP_PART_IMAGE;

	t->mask = mask;

	return t;
}

void
em_popup_target_free(EMPopupTarget *t)
{
	switch (t->type) {
	case EM_POPUP_TARGET_SELECT:
		camel_object_unref(t->data.select.folder);
		g_free(t->data.select.folder_uri);
		if (t->data.select.uids)
			em_utils_uids_free(t->data.select.uids);
		break;
	case EM_POPUP_TARGET_URI:
		g_free(t->data.uri);
		break;
	case EM_POPUP_TARGET_PART:
		camel_object_unref(t->data.part.part);
		g_free(t->data.part.mime_type);
		break;
	}

	g_free(t);
}

/* ********************************************************************** */

#if 0
/* TODO: flesh these out where possible */
static void
emp_popup_open(GtkWidget *w, EMFolderView *emfv)
{
	em_folder_view_open_selected(emfv);
}

static void
emp_popup_resend(GtkWidget *w, EMPopupTarget *t)
{
	if (!em_utils_check_user_can_send_mail(t->widget))
		return;
	
	em_utils_edit_messages(t->widget, t->data.select.folder, em_utils_uids_copy(t->data.select.uids));
}

static void
emp_popup_saveas(GtkWidget *w, EMPopupTarget *t)
{
	em_utils_save_messages(t->widget, t->data.select.folder, em_utils_uids_copy(t->data.select.uids));
}

static EMPopupItem emp_standard_select_popups[] = {
	/*{ EM_POPUP_ITEM, "00.select.00", N_("_Open"), G_CALLBACK(emp_popup_open), NULL, NULL, 0 },*/
	{ EM_POPUP_ITEM, "00.select.01", N_("_Edit as New Message..."), G_CALLBACK(emp_popup_resend), NULL, NULL, EM_POPUP_SELECT_RESEND },
	{ EM_POPUP_ITEM, "00.select.02", N_("_Save As..."), G_CALLBACK(emp_popup_saveas), NULL, "save-as-16.png", 0 },	
};
#endif

/* ********************************************************************** */

static void
emp_part_popup_saveas(GtkWidget *w, EMPopupTarget *t)
{
	em_utils_save_part(w, _("Save As..."), t->data.part.part);
}

static void
emp_part_popup_set_background(GtkWidget *w, EMPopupTarget *t)
{
	/* set as background ... */
	printf("UNIMPLEMENTED: set background, but it would be cool, no?\n");
}

static void
emp_part_popup_reply_sender(GtkWidget *w, EMPopupTarget *t)
{
	em_utils_reply_to_message(t->widget,
				  (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)t->data.part.part),
				  REPLY_MODE_SENDER);
}

static void
emp_part_popup_reply_list(GtkWidget *w, EMPopupTarget *t)
{
	em_utils_reply_to_message(t->widget,
				  (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)t->data.part.part),
				  REPLY_MODE_LIST);
}

static void
emp_part_popup_reply_all(GtkWidget *w, EMPopupTarget *t)
{
	em_utils_reply_to_message(t->widget,
				  (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)t->data.part.part),
				  REPLY_MODE_ALL);
}

static void
emp_part_popup_forward(GtkWidget *w, EMPopupTarget *t)
{
	em_utils_forward_message(t->widget,
				 (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)t->data.part.part));
}

static EMPopupItem emp_standard_object_popups[] = {
	{ EM_POPUP_ITEM, "00.part.00", N_("_Save As..."), G_CALLBACK(emp_part_popup_saveas), NULL, "save-as-16.png", 0 },
	{ EM_POPUP_ITEM, "00.part.10", N_("Set as _Background"), G_CALLBACK(emp_part_popup_set_background), NULL, NULL, EM_POPUP_PART_IMAGE },
	{ EM_POPUP_BAR, "10.part", NULL, NULL, NULL, NULL, EM_POPUP_PART_MESSAGE },
	{ EM_POPUP_ITEM, "10.part.00", N_("_Reply to sender"), G_CALLBACK(emp_part_popup_reply_sender), NULL, "reply.xpm" , EM_POPUP_PART_MESSAGE },
	{ EM_POPUP_ITEM, "10.part.01", N_("Reply to _List"), G_CALLBACK(emp_part_popup_reply_list), NULL, NULL, EM_POPUP_PART_MESSAGE},
	{ EM_POPUP_ITEM, "10.part.03", N_("Reply to _All"), G_CALLBACK(emp_part_popup_reply_all), NULL, "reply_to_all.xpm", EM_POPUP_PART_MESSAGE},
	{ EM_POPUP_BAR, "20.part", NULL, NULL, NULL, NULL, EM_POPUP_PART_MESSAGE },
	{ EM_POPUP_ITEM, "20.part.00", N_("_Forward"), G_CALLBACK(emp_part_popup_forward), NULL, "forward.xpm", EM_POPUP_PART_MESSAGE },

};

static const EMPopupItem emp_standard_part_apps_bar = { EM_POPUP_BAR, "99.object" };

/* ********************************************************************** */

static void
emp_uri_popup_link_open(GtkWidget *w, EMPopupTarget *t)
{
	GError *err = NULL;
		
	gnome_url_show(t->data.uri, &err);
	if (err) {
		g_warning("gnome_url_show: %s", err->message);
		g_error_free(err);
	}
}

static void
emp_uri_popup_address_send(GtkWidget *w, EMPopupTarget *t)
{
	em_utils_compose_new_message_with_mailto(t->widget, t->data.uri);
}

static void
emp_uri_popup_address_add(GtkWidget *w, EMPopupTarget *t)
{
	printf("UNIMPLEMENTED: Add address '%s'\n", t->data.uri);
}

static EMPopupItem emp_standard_uri_popups[] = {
	{ EM_POPUP_ITEM, "00.uri.00", N_("_Open Link in Browser"), G_CALLBACK(emp_uri_popup_link_open), NULL, NULL, EM_POPUP_URI_NOT_MAILTO },
	{ EM_POPUP_ITEM, "00.uri.10", N_("Se_nd message to..."), G_CALLBACK(emp_uri_popup_address_send), NULL, NULL, EM_POPUP_URI_MAILTO },
	{ EM_POPUP_ITEM, "00.uri.20", N_("_Add to Addressbook"), G_CALLBACK(emp_uri_popup_address_add), NULL, NULL, EM_POPUP_URI_MAILTO },
};

/* ********************************************************************** */

#define LEN(x) (sizeof(x)/sizeof(x[0]))

#include <libgnomevfs/gnome-vfs-mime-handlers.h>

struct _open_in_item {
	EMPopupItem item;
	EMPopupTarget *target;
	GnomeVFSMimeApplication *app;
};

static void
emp_apps_open_in(GtkWidget *w, struct _open_in_item *item)
{
	char *path;

	path = em_utils_temp_save_part(item->target->widget, item->target->data.part.part);
	if (path) {
		char *command;
		int douri = (item->app->expects_uris == GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS);
				
		command = g_strdup_printf(douri?"%s file://%s &":"%s %s &", item->app->command, path);

		/* FIXME: Do not use system here */
		system(command);
		g_free(command);
		g_free(path);
	}
}

static void
emp_apps_popup_free(GSList *free_list)
{
	while (free_list) {
		GSList *n = free_list->next;
		struct _open_in_item *item = free_list->data;

		g_free(item->item.path);
		g_free(item->item.label);
		g_free(item);
		g_slist_free_1(free_list);

		free_list = n;
	}
}

static void
emp_standard_menu_factory(EMPopup *emp, EMPopupTarget *target, void *data)
{
	int i, len;
	EMPopupItem *items;
	GSList *menus = NULL;

	switch (target->type) {
	case EM_POPUP_TARGET_SELECT:
		return;
#if 0
		items = emp_standard_select_popups;
		len = LEN(emp_standard_select_popups);
		break;
#endif
	case EM_POPUP_TARGET_URI:
		items = emp_standard_uri_popups;
		len = LEN(emp_standard_uri_popups);
		break;
	case EM_POPUP_TARGET_PART: {
		GList *apps = gnome_vfs_mime_get_short_list_applications(target->data.part.mime_type);

		/* FIXME: use the snoop_part stuff from em-format.c */
		if (apps == NULL && strcmp(target->data.part.mime_type, "application/octet-stream") == 0) {
			const char *filename, *name_type;
			
			filename = camel_mime_part_get_filename(target->data.part.part);

			if (filename) {
				/* GNOME-VFS will misidentify TNEF attachments as MPEG */
				if (!strcmp (filename, "winmail.dat"))
					name_type = "application/vnd.ms-tnef";
				else
					name_type = gnome_vfs_mime_type_from_name(filename);
				if (name_type)
					apps = gnome_vfs_mime_get_short_list_applications(name_type);
			}
		}

		if (apps) {
			GString *label = g_string_new("");
			GSList *open_menus = NULL;
			GList *l;

			menus = g_slist_prepend(menus, (void *)&emp_standard_part_apps_bar);

			for (l = apps, i = 0; l; l = l->next, i++) {
				GnomeVFSMimeApplication *app = l->data;
				struct _open_in_item *item;

				if (app->requires_terminal)
					continue;

				item = g_malloc0(sizeof(*item));
				item->item.type = EM_POPUP_ITEM;
				item->item.path = g_strdup_printf("99.object.%02d", i);
				item->item.label = g_strdup_printf(_("Open in %s..."), app->name);
				item->item.activate = G_CALLBACK(emp_apps_open_in);
				item->item.activate_data = item;
				item->target = target;
				item->app = app;

				open_menus = g_slist_prepend(open_menus, item);
			}

			if (open_menus)
				em_popup_add_items(emp, open_menus, (GDestroyNotify)emp_apps_popup_free);

			g_string_free(label, TRUE);
			g_list_free(apps);
		}

		items = emp_standard_object_popups;
		len = LEN(emp_standard_object_popups);
		break; }
	default:
		items = NULL;
		len = 0;
		g_assert_not_reached ();
	}

	for (i=0;i<len;i++) {
		if ((items[i].mask & target->mask) == 0) {
			items[i].activate_data = target;
			menus = g_slist_prepend(menus, &items[i]);
		}
	}

	if (menus)
		em_popup_add_items(emp, menus, (GDestroyNotify)g_slist_free);
}
