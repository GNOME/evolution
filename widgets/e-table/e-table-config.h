#ifndef _E_TABLE_CONFIG_H
#define _E_TABLE_CONFIG_H

GnomeDialog *e_table_gui_config    (ETable *etable);
void         e_table_do_gui_config (GtkWidget *, ETable *etable);

void e_table_gui_config_accept (GtkWidget *widget, ETable *etable);
void e_table_gui_config_cancel (GtkWidget *widget, ETable *etable);


#endif /* _E_TABLE_CONFIG_H */
