
#ifndef __E_LDAP_SERVER_DIALOG_H__
#define __E_LDAP_SERVER_DIALOG_H__

typedef struct {
	char *description;
	char *host;
	int port;
	char *rootdn;
} ELDAPServer;

void e_ldap_server_editor_show(ELDAPServer *server);

#endif /* __E_LDAP_SERVER_DIALOG_H__ */
