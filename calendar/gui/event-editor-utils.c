#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include <gui/event-editor-utils.h>


/***************/
/*** storing ***/
/***************/

void
store_to_editable (GladeXML *gui, char *widget_name, gchar *content)
{
	GtkWidget *widget = glade_xml_get_widget (gui, widget_name);
	GtkEditable *editable = GTK_EDITABLE (widget);
	int position = 0;

	if (! content)
		return;

	gtk_editable_delete_text (editable, 0, -1);
	gtk_editable_insert_text (editable, content,
				  strlen (content), &position);
}


void
store_to_toggle (GladeXML *gui, char *widget_name, gboolean content)
{
	GtkWidget *widget = glade_xml_get_widget (gui, widget_name);
	GtkToggleButton *toggle = GTK_TOGGLE_BUTTON (widget);
	gtk_toggle_button_set_active (toggle, content);
}


void
store_to_spin (GladeXML *gui, char *widget_name, gint content)
{
	GtkWidget *widget;
	GtkSpinButton *spin;

	widget = glade_xml_get_widget (gui, widget_name);
	spin = GTK_SPIN_BUTTON (widget);
	gtk_spin_button_set_value (spin, (gfloat) content);
}


void
store_to_alarm_unit (GladeXML *gui, char *widget_name, enum AlarmUnit content)
{
	GtkWidget *widget;
	GtkOptionMenu *option;

	widget = glade_xml_get_widget (gui, widget_name);
	option = GTK_OPTION_MENU (widget);

	switch (content) {
	case ALARM_MINUTES:
		gtk_option_menu_set_history (option, 0); break;
	case ALARM_HOURS:
		gtk_option_menu_set_history (option, 1); break;
	case ALARM_DAYS:
		gtk_option_menu_set_history (option, 2); break;
	}
}


void
store_to_option (GladeXML *gui, char *widget_name, gint content)
{
	GtkWidget *widget;
	GtkOptionMenu *option;

	widget = glade_xml_get_widget (gui, widget_name);
	option = GTK_OPTION_MENU (widget);

	gtk_option_menu_set_history (option, content);
}


void
store_to_gnome_dateedit (GladeXML *gui, char *widget_name, time_t content)
{
	GtkWidget *widget;
	GnomeDateEdit *gde;

	widget = glade_xml_get_widget (gui, widget_name);
	gde = GNOME_DATE_EDIT (widget);
	gnome_date_edit_set_time (gde, content);
}



/******************/
/*** extracting ***/
/******************/


gchar *extract_from_editable (GladeXML *gui, char *widget_name)
{
	GtkWidget *widget;
	GtkEditable *editable;
	gchar *content;

	widget = glade_xml_get_widget (gui, widget_name);
	editable = GTK_EDITABLE (widget);
	content = gtk_editable_get_chars (editable, 0, -1);
	return content;
}


gboolean extract_from_toggle (GladeXML *gui, char *widget_name)
{
	GtkWidget *widget;
	GtkToggleButton *toggle;
	gboolean content;

	widget = glade_xml_get_widget (gui, widget_name);
	toggle = GTK_TOGGLE_BUTTON (widget);
	content = gtk_toggle_button_get_active (toggle);
	return content;
}


gint extract_from_spin (GladeXML *gui, char *widget_name)
{
	GtkWidget *widget;
	GtkSpinButton *spin;
	gint content;

	widget = glade_xml_get_widget (gui, widget_name);
	spin = GTK_SPIN_BUTTON (widget);
	content = gtk_spin_button_get_value_as_int (spin);
	return content;
}


#if 0
char *extract_from_option (GladeXML *gui, char *widget_name)
{
	GtkWidget *widget;
	GtkOptionMenu *option;
	GtkWidget *picked;
	GtkMenu *menu;
	GtkMenuItem *menu_item;
	//GList *children;
	GtkLabel *label;
	char *content = NULL;

	widget = glade_xml_get_widget (gui, widget_name);
	option = GTK_OPTION_MENU (widget);
	picked = gtk_option_menu_get_menu (option);
	menu = GTK_MENU (picked);
	menu_item = GTK_MENU_ITEM (gtk_menu_get_active (menu));
	label = GTK_LABEL (GTK_BIN (menu_item)->child);
	gtk_label_get (label, &content);

	return content;
}
#endif /* 0 */


#if 0
enum AlarmUnit
extract_from_alarm_unit (GladeXML *gui, char *widget_name)
{
	GtkWidget *option;
	enum AlarmUnit u;

	option = glade_xml_get_widget (gui, widget_name);
	u = (enum AlarmUnit) gtk_object_get_data (GTK_OBJECT (option), "unit");
	return u;
}
#endif /* 0 */


enum AlarmUnit
extract_from_alarm_unit (GladeXML *gui, char *widget_name)
{
	GtkWidget *widget;
	GtkOptionMenu *option;
	GtkMenu *menu;
	GtkMenuShell *menu_shell;
	GtkMenuItem *menu_item;
	enum AlarmUnit content;

	widget = glade_xml_get_widget (gui, widget_name);
	option = GTK_OPTION_MENU (widget);
	menu = GTK_MENU (gtk_option_menu_get_menu (option));
	menu_shell = GTK_MENU_SHELL (menu);
	menu_item = GTK_MENU_ITEM (gtk_menu_get_active (menu));

	content = g_list_index (menu_shell->children, (gpointer) menu_item);
	return (content >= 0) ? content : ALARM_MINUTES;
}


guint
extract_from_option (GladeXML *gui, char *widget_name)
{
	GtkWidget *widget;
	GtkOptionMenu *option;
	GtkMenu *menu;
	GtkMenuShell *menu_shell;
	GtkMenuItem *menu_item;
	enum AlarmUnit content;

	widget = glade_xml_get_widget (gui, widget_name);
	option = GTK_OPTION_MENU (widget);
	menu = GTK_MENU (gtk_option_menu_get_menu (option));
	menu_shell = GTK_MENU_SHELL (menu);
	menu_item = GTK_MENU_ITEM (gtk_menu_get_active (menu));

	content = g_list_index (menu_shell->children, (gpointer) menu_item);
	return (content >= 0) ? content : 0;
}


time_t extract_from_gnome_dateedit (GladeXML *gui, char *widget_name)
{
	GtkWidget *de;
	time_t t;

	de = glade_xml_get_widget (gui, widget_name);
	t = gnome_date_edit_get_date (GNOME_DATE_EDIT (de));
	return t;
}

