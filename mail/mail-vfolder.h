
#ifndef _MAIL_VFOLDER_H
#define _MAIL_VFOLDER_H

#include <bonobo.h>

#include "Evolution.h"
#include "evolution-storage.h"
#include "evolution-shell-component.h"

#include "camel/camel-folder.h"
#include "camel/camel-mime-message.h"
#include "filter/vfolder-rule.h"
#include "filter/filter-part.h"

void vfolder_create_storage(EvolutionShellComponent *shell_component);

CamelFolder *vfolder_uri_to_folder(const char *uri, CamelException *ex);
void vfolder_edit(void);
FilterPart *vfolder_create_part(const char *name);
void vfolder_gui_add_rule(VfolderRule *rule);
void vfolder_gui_add_from_message(CamelMimeMessage *msg, int flags, const char *source);

#endif
