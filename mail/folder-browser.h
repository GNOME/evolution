/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#ifndef _FOLDER_BROWSER_H_
#define _FOLDER_BROWSER_H_

#include <gtk/gtktable.h>
#include "camel/camel-stream.h"
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-ui-component.h>
#include <widgets/misc/e-filter-bar.h>
#include "widgets/menus/gal-view-menus.h"
#include "filter/filter-rule.h"
#include "filter/filter-context.h" /*eek*/
#include "message-list.h"
#include "mail-display.h"
#include "mail-types.h"
#include "shell/Evolution.h"


#define FOLDER_BROWSER_TYPE        (folder_browser_get_type ())
#define FOLDER_BROWSER(o)          (GTK_CHECK_CAST ((o), FOLDER_BROWSER_TYPE, FolderBrowser))
#define FOLDER_BROWSER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), FOLDER_BROWSER_TYPE, FolderBrowserClass))
#define IS_FOLDER_BROWSER(o)       (GTK_CHECK_TYPE ((o), FOLDER_BROWSER_TYPE))
#define IS_FOLDER_BROWSER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), FOLDER_BROWSER_TYPE))

typedef enum _FolderBrowserSelectionState {
	FB_SELSTATE_NONE,
	FB_SELSTATE_SINGLE,
	FB_SELSTATE_MULTIPLE,
	FB_SELSTATE_UNDEFINED
} FolderBrowserSelectionState;

struct  _FolderBrowser {
	GtkTable parent;
	
	BonoboPropertyBag *properties;
	
	GNOME_Evolution_Shell shell;
	BonoboUIComponent *uicomp;
	
	/*
	 * The current URI being displayed by the FolderBrowser
	 */
	char        *uri;
	CamelFolder *folder;
	int          unread_count; /* last known unread message count */
	
	/* async loading stuff */
	char	    *loading_uid;/* what uid am i loading now */
	char	    *pending_uid; /* what uid should i load next */
	char	    *new_uid;	/* place to save the next uid during idle timeout */
	char	    *loaded_uid; /* what we have loaded */
	guint	     loading_id, seen_id;
	
	/* a folder we are expunging, dont use other than to compare the pointer value */
	CamelFolder *expunging;
	
	MessageList *message_list;
	MailDisplay *mail_display;
	GtkWidget   *vpaned;
	
	EFilterBar  *search;
	FilterRule  *search_full; /* if we have a full search active */
	
	gboolean     preview_shown;
	gboolean     threaded;
	gboolean     pref_master;

	FolderBrowserSelectionState selection_state;
	GSList *sensitize_changes;
	int sensitize_timeout_id;

	/* View collection and the menu handler object */
	GalViewCollection *view_collection;
	GalViewMenus *view_menus;
	
	GtkWidget *invisible;
	GByteArray *clipboard_selection;
};


typedef struct {
	GtkTableClass parent_class;
	
	/* signals */
	void (*folder_loaded)  (FolderBrowser *fb, const char *uri);
	void (*message_loaded) (FolderBrowser *fb, const char *uid);
} FolderBrowserClass;

struct fb_ondemand_closure {
	FilterRule *rule;
	FolderBrowser *fb;
	gchar *path;
};

GtkType    folder_browser_get_type             (void);
GtkWidget *folder_browser_new                  (const GNOME_Evolution_Shell  shell);

void       folder_browser_set_ui_component     (FolderBrowser *fb, 
						BonoboUIComponent *uicomp);

gboolean   folder_browser_set_uri              (FolderBrowser         *folder_browser,
						const char            *uri);

void       folder_browser_set_message_preview  (FolderBrowser         *folder_browser,
						gboolean               show_message_preview);
void       folder_browser_clear_search         (FolderBrowser         *fb);

void       folder_browser_cut                  (GtkWidget *widget, FolderBrowser *fb);
void       folder_browser_copy                 (GtkWidget *widget, FolderBrowser *fb);
void       folder_browser_paste                (GtkWidget *widget, FolderBrowser *fb);

/* callbacks for functions on the folder-browser */
void vfolder_subject   (GtkWidget *w, FolderBrowser *fb);
void vfolder_sender    (GtkWidget *w, FolderBrowser *fb);
void vfolder_recipient (GtkWidget *w, FolderBrowser *fb);
void vfolder_mlist     (GtkWidget *w, FolderBrowser *fb);

void filter_subject    (GtkWidget *w, FolderBrowser *fb);
void filter_sender     (GtkWidget *w, FolderBrowser *fb);
void filter_recipient  (GtkWidget *w, FolderBrowser *fb);
void filter_mlist      (GtkWidget *w, FolderBrowser *fb);

void hide_read(GtkWidget *w, FolderBrowser *fb);
void hide_deleted(GtkWidget *w, FolderBrowser *fb);
void hide_selected(GtkWidget *w, FolderBrowser *fb);
void hide_none(GtkWidget *w, FolderBrowser *fb);
void hide_subject(GtkWidget *w, FolderBrowser *fb);
void hide_sender(GtkWidget *w, FolderBrowser *fb);

void folder_browser_toggle_preview (BonoboUIComponent           *component,
				    const char                  *path,
				    Bonobo_UIComponent_EventType type,
				    const char                  *state,
				    gpointer                     user_data);

void folder_browser_toggle_threads (BonoboUIComponent           *component,
				    const char                  *path,
				    Bonobo_UIComponent_EventType type,
				    const char                  *state,
				    gpointer                     user_data);

void folder_browser_toggle_hide_deleted (BonoboUIComponent           *component,
					 const char                  *path,
					 Bonobo_UIComponent_EventType type,
					 const char                  *state,
					 gpointer                     user_data);

void folder_browser_set_message_display_style (BonoboUIComponent           *component,
					       const char                  *path,
					       Bonobo_UIComponent_EventType type,
					       const char                  *state,
					       gpointer                     user_data);

gboolean folder_browser_is_drafts (FolderBrowser *fb);
gboolean folder_browser_is_sent   (FolderBrowser *fb);
gboolean folder_browser_is_outbox (FolderBrowser *fb);

#endif /* _FOLDER_BROWSER_H_ */
