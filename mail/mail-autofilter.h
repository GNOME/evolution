#ifndef _MAIL_AUTOFILTER_H
#define _MAIL_AUTOFILTER_H

#include "filter/filter-rule.h"
#include "filter/filter-context.h"
#include "filter/vfolder-context.h"
#include "camel/camel-mime-message.h"

enum {
	AUTO_SUBJECT = 1,
	AUTO_FROM = 2,
	AUTO_TO = 4
};

FilterRule *vfolder_rule_from_message(VfolderContext *context, CamelMimeMessage *msg, int flags, const char *source);
FilterRule *filter_rule_from_message(FilterContext *context, CamelMimeMessage *msg, int flags);

/* easiest place to put this */
void filter_gui_add_from_message(CamelMimeMessage *msg, int flags);

#endif
