
#ifndef _MAIL_VFOLDER_H
#define _MAIL_VFOLDER_H

#include "Evolution.h"
#include "evolution-storage.h"
#include "evolution-shell-component.h"

#include "camel/camel-folder.h"
#include "camel/camel-mime-message.h"
#include "filter/vfolder-rule.h"
#include "filter/filter-part.h"

void vfolder_load_storage(GNOME_Evolution_Shell shell);

void vfolder_edit (void);
FilterPart *vfolder_create_part (const char *name);
FilterRule *vfolder_clone_rule (FilterRule *in);
void vfolder_gui_add_rule (VfolderRule *rule);
void vfolder_gui_add_from_message (CamelMimeMessage *msg, int flags, const char *source);
void vfolder_gui_add_from_mlist (CamelMimeMessage *msg, const char *mlist, const char *source);

/* add a uri that is now available to vfolders */
void mail_vfolder_add_uri(CamelStore *store, const char *uri);

/* remove a uri that should be removed from vfolders */
void mail_vfolder_remove_uri(CamelStore *store, const char *uri);

EvolutionStorage *mail_vfolder_get_vfolder_storage (void);

#endif
