
#ifndef _FILTER_XML_H
#define _FILTER_XML_H

enum filter_xml_token {
	FILTER_XML_TEXT=0,
	FILTER_XML_RULE,
	FILTER_XML_CODE,
	FILTER_XML_DESC,
	FILTER_XML_RULESET,
	FILTER_XML_OPTION,
	FILTER_XML_OPTIONRULE,
	FILTER_XML_OPTIONSET,
	FILTER_XML_OPTIONVALUE,
	FILTER_XML_SOURCE,
	FILTER_XML_SEND,
	FILTER_XML_RECEIVE,
	FILTER_XML_ADDRESS,
	FILTER_XML_FOLDER,
	FILTER_XML_NAME,
	FILTER_XML_MATCH,
	FILTER_XML_ACTION,
	FILTER_XML_EXCEPT
};

struct filter_desc {
	int type;
	char *data;
	char *varname;		/* for named types */
	int vartype;
};

struct filter_rule {
	int type;
	char *name;
	char *code;
	GList *description;
};

struct filter_optionrule {
	struct filter_rule *rule;
	GList *args;		/* FilterArg objects */
};

struct filter_option {
	int type;		/* 'send' 'receive'? */
	GList *description;	/* filter_desc */
	GList *options;		/* option_rule */
};

#endif /* ! _FILTER_XML_H */
