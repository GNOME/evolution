
#ifndef _MAIL_VFOLDER_H
#define _MAIL_VFOLDER_H

#include "Evolution.h"

#include "camel/camel-folder.h"
#include "camel/camel-mime-message.h"
#include "em-vfolder-rule.h"
#include "filter/filter-part.h"

void vfolder_load_storage(void);
void vfolder_revert(void);

void vfolder_edit (void);
void vfolder_edit_rule(const char *name);
FilterPart *vfolder_create_part (const char *name);
FilterRule *vfolder_clone_rule (FilterRule *in);
void vfolder_gui_add_rule (EMVFolderRule *rule);
void vfolder_gui_add_from_message (CamelMimeMessage *msg, int flags, const char *source);

/* add a uri that is now (un)available to vfolders in a transient manner */
void mail_vfolder_add_uri(CamelStore *store, const char *uri, int remove);

/* note that a folder has changed name (uri) */
void mail_vfolder_rename_uri(CamelStore *store, const char *from, const char *to);

/* remove a uri that should be removed from vfolders permanently */
void mail_vfolder_delete_uri(CamelStore *store, const char *uri);

/* close up, clean up */
void mail_vfolder_shutdown (void);

#endif
