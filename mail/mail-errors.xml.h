/* mail:camel-service-auth-invalid primary */
char *s = N_("Invalid authentication");
/* mail:camel-service-auth-invalid secondary */
char *s = N_("This server does not support this type of authentication and may not support authentication at all.");
/* mail:camel-service-auth-failed primary */
char *s = N_("Your login to your server \"{0}\" as \"{0}\" failed.");
/* mail:camel-service-auth-failed secondary */
char *s = N_("Check to make sure your password is spelled correctly.  Remember that many passwords are case sensitive; your caps lock might be on.");
/* mail:ask-send-html primary */
char *s = N_("Are you sure you want to send a message in HTML format?");
/* mail:ask-send-html secondary */
char *s = N_("Please make sure the following recipients are willing and able to receive HTML email:\n"
	"{0}\n"
	"Send anyway?");
char *s = N_("_Send");
/* mail:ask-send-no-subject primary */
char *s = N_("Are you sure you want to send a message without a subject?");
/* mail:ask-send-no-subject secondary */
char *s = N_("Adding a meaningful Subject line to your messages will give your recipients an idea of what your mail is about.");
char *s = N_("_Send");
/* mail:ask-send-only-bcc-contact primary */
char *s = N_("Are you sure you want to send a message with only BCC recipients?");
/* mail:ask-send-only-bcc-contact secondary */
char *s = N_("The contact list you are sending to is configured to hide list recipients.\n"
	"\n"
	"Many email systems add an Apparently-To header to messages that only have BCC recipients. This header, if added, will list all of your recipients in your message. To avoid this, you should add at least one To: or CC: recipient. ");
char *s = N_("_Send");
/* mail:ask-send-only-bcc primary */
char *s = N_("Are you sure you want to send a message with only BCC recipients?");
/* mail:ask-send-only-bcc secondary */
char *s = N_("Many email systems add an Apparently-To header to messages that only have BCC recipients. This header, if added, will list all of your recipients to your message anyway. To avoid this, you should add at least one To: or CC: recipient.");
char *s = N_("_Send");
/* mail:send-no-recipients primary */
char *s = N_("This message cannot be sent because you have not specified any Recipients");
/* mail:send-no-recipients secondary */
char *s = N_("Please enter a valid email address in the To: field. You can search for email addresses by clicking on the To: button next to the entry box.");
/* mail:ask-default-drafts primary */
char *s = N_("Use default drafts folder?");
/* mail:ask-default-drafts secondary */
char *s = N_("Unable to open the drafts folder for this account.  Use the system drafts folder instead?");
char *s = N_("Use _Default");
/* mail:ask-expunge primary */
char *s = N_("Are you sure you want to permanently remove all the deleted message in folder \"{0}\"?");
/* mail:ask-expunge secondary */
char *s = N_("If you continue, you will not be able to recover these messages.");
char *s = N_("_Expunge");
/* mail:ask-empty-trash primary */
char *s = N_("Are you sure you want to permanently remove all the deleted messages in all folders?");
/* mail:ask-empty-trash secondary */
char *s = N_("If you continue, you will not be able to recover these messages.");
char *s = N_("_Empty Trash");
/* mail:exit-unsaved primary */
char *s = N_("You have unsent messages, do you wish to quit anyway?");
/* mail:exit-unsaved secondary */
char *s = N_("If you quit, these messages will not be sent until Evolution is started again.");
/* mail:camel-exception primary */
char *s = N_("Your message with the subject \"{0}\" was not delivered.");
/* mail:camel-exception secondary */
char *s = N_("The message was sent via the \"sendmail\" external application.  Sendmail reports the following error: status 67: mail not sent.\n"
	"The message is stored in the Outbox folder.  Check the message for errors and resend.");
/* mail:async-error primary */
char *s = N_("Error while {0}.");
/* mail:async-error secondary */
char *s = N_("{1}.");
/* mail:async-error-nodescribe primary */
char *s = N_("Error while performing operation.");
/* mail:async-error-nodescribe secondary */
char *s = N_("{0}.");
/* mail:session-message-info secondary */
char *s = N_("{0}");
/* mail:session-message-info-cancel secondary */
char *s = N_("{0}");
/* mail:session-message-warning secondary */
char *s = N_("{0}");
/* mail:session-message-warning-cancel secondary */
char *s = N_("{0}");
/* mail:session-message-error secondary */
char *s = N_("{0}");
/* mail:session-message-error-cancel secondary */
char *s = N_("{0}");
/* mail:ask-session-password primary */
char *s = N_("Enter password.");
/* mail:ask-session-password secondary */
char *s = N_("{0}");
/* mail:filter-load-error primary */
char *s = N_("Error loading filter definitions.");
/* mail:filter-load-error secondary */
char *s = N_("{0}");
/* mail:no-save-path primary */
char *s = N_("Cannot save to directory \"{0}\".");
/* mail:no-save-path secondary */
char *s = N_("{1}");
/* mail:no-create-path primary */
char *s = N_("Cannot save to file \"{0}\".");
/* mail:no-create-path secondary */
char *s = N_("Cannot create the save directory, because \"{1}\"");
/* mail:no-create-tmp-path primary */
char *s = N_("Cannot create temporary save directory.");
/* mail:no-create-tmp-path secondary */
char *s = N_("Because \"{1}\".");
/* mail:no-write-path-exists primary */
char *s = N_("Cannot save to file \"{0}\".");
/* mail:no-write-path-exists secondary */
char *s = N_("File exists but cannot overwrite it.");
/* mail:no-write-path-notfile primary */
char *s = N_("Cannot save to file \"{0}\".");
/* mail:no-write-path-notfile secondary */
char *s = N_("File exists but is not a regular file.");
/* mail:no-delete-folder primary */
char *s = N_("Cannot delete folder \"{0}\".");
/* mail:no-delete-folder secondary */
char *s = N_("Because \"{1}\".");
/* mail:no-delete-special-folder primary */
char *s = N_("Cannot delete system folder \"{0}\".");
/* mail:no-delete-special-folder secondary */
char *s = N_("System folders are required for Ximian Evolution to function correctly and cannot be renamed, moved, or deleted.");
/* mail:no-rename-special-folder primary */
char *s = N_("Cannot rename or move system folder \"{0}\".");
/* mail:no-rename-special-folder secondary */
char *s = N_("System folders are required for Ximian Evolution to function correctly and cannot be renamed, moved, or deleted.");
/* mail:ask-delete-folder title */
char *s = N_("Delete \"{0}\"?");
/* mail:ask-delete-folder primary */
char *s = N_("Really delete folder \"{0}\" and all of its subfolders?");
/* mail:ask-delete-folder secondary */
char *s = N_("If you delete the folder, all of its contents and its subfolders contents will be deleted permanently.");
/* mail:no-rename-folder-exists primary */
char *s = N_("Cannot rename \"{0}\" to \"{1}\".");
/* mail:no-rename-folder-exists secondary */
char *s = N_("A folder named \"{1}\" already exists. Please use a different name.");
/* mail:no-rename-folder primary */
char *s = N_("Cannot rename \"{0}\" to \"{1}\".");
/* mail:no-rename-folder secondary */
char *s = N_("Because \"{2}\".");
/* mail:no-move-folder-nostore primary */
char *s = N_("Cannot move folder \"{0}\" to \"{1}\".");
/* mail:no-move-folder-nostore secondary */
char *s = N_("Cannot open source \"{2}\".");
/* mail:no-move-folder-to-nostore primary */
char *s = N_("Cannot move folder \"{0}\" to \"{1}\".");
/* mail:no-move-folder-to-nostore secondary */
char *s = N_("Cannot open target \"{2}\".");
/* mail:no-copy-folder-nostore primary */
char *s = N_("Cannot copy folder \"{0}\" to \"{1}\".");
/* mail:no-copy-folder-nostore secondary */
char *s = N_("Cannot open source \"{2}\".");
/* mail:no-copy-folder-to-nostore primary */
char *s = N_("Cannot copy folder \"{0}\" to \"{1}\".");
/* mail:no-copy-folder-to-nostore secondary */
char *s = N_("Cannot open target \"{2}\".");
/* mail:no-create-folder-nostore primary */
char *s = N_("Cannot create folder \"{0}\".");
/* mail:no-create-folder-nostore secondary */
char *s = N_("Cannot open source \"{1}\"");
/* mail:account-incomplete primary */
char *s = N_("Cannot save changes to account.");
/* mail:account-incomplete secondary */
char *s = N_("You have not filled in all of the required information.");
/* mail:account-notunique primary */
char *s = N_("Cannot save changes to account.");
/* mail:account-notunique secondary */
char *s = N_("You may not create two accounts with the same name.");
/* mail:ask-delete-account title */
char *s = N_("Delete account?");
/* mail:ask-delete-account primary */
char *s = N_("Are you sure you want to delete this account?");
/* mail:ask-delete-account secondary */
char *s = N_("If you proceed, the account information will be deleted permanently.");
char *s = N_("Don't delete");
/* mail:no-save-signature primary */
char *s = N_("Could not save signature file.");
/* mail:no-save-signature secondary */
char *s = N_("Because \"{0}\".");
/* mail:signature-notscript primary */
char *s = N_("Cannot set signature script \"{0}\".");
/* mail:signature-notscript secondary */
char *s = N_("The script file must exist and be executable.");
/* mail:ask-signature-changed title */
char *s = N_("Discard changed?");
/* mail:ask-signature-changed primary */
char *s = N_("Do you wish to save your changes?");
/* mail:ask-signature-changed secondary */
char *s = N_("This signature has been changed, but has not been saved.");
char *s = N_("_Discard changes");
/* mail:vfolder-notexist primary */
char *s = N_("Cannot edit vFolder \"{0}\" as it does not exist.");
/* mail:vfolder-notexist secondary */
char *s = N_("This folder may have been added implictly, go to the virtual folder editor to add it explictly, if required.");
/* mail:vfolder-notunique primary */
char *s = N_("Cannot add vFolder \"{0}\".");
/* mail:vfolder-notunique secondary */
char *s = N_("A folder named \"{1}\" already exists. Please use a different name.");
/* mail:vfolder-updated primary */
char *s = N_("vFolders automatically updated.");
/* mail:vfolder-updated secondary */
char *s = N_("The following vFolder(s):\n"
	"{0}\n"
	"Used the now removed folder:\n"
	"    \"{1}\"\n"
	"And have been updated.");
/* mail:filter-updated primary */
char *s = N_("Mail filters automatically updated.");
/* mail:filter-updated secondary */
char *s = N_("The following filter rule(s):\n"
	"{0}\n"
	"Used the now removed folder:\n"
	"    \"{1}\"\n"
	"And have been updated.");
/* mail:no-folder primary */
char *s = N_("Missing folder.");
/* mail:no-folder secondary */
char *s = N_("You must specify a folder.");
/* mail:no-name-vfolder primary */
char *s = N_("Missing name.");
/* mail:no-name-vfolder secondary */
char *s = N_("You must name this vFolder.");
/* mail:vfolder-no-source primary */
char *s = N_("No sources selected.");
/* mail:vfolder-no-source secondary */
char *s = N_("You must specify at least one folder as a source.\n"
	"Either by selecting the folders individually, and/or by selecting\n"
	"all local folders, all remote folders, or both.");
/* mail:ask-migrate-existing primary */
char *s = N_("Problem migrating old mail folder \"{0}\".");
/* mail:ask-migrate-existing secondary */
char *s = N_("A non-empty folder at \"{1}\" already exists.\n"
	"\n"
	"You can choose to ignore this folder, overwrite or append its contents, or quit.\n"
	"");
char *s = N_("Ignore");
char *s = N_("_Overwrite");
char *s = N_("_Append");
/* mail:no-load-license primary */
char *s = N_("Unable to read license file.");
/* mail:no-load-license secondary */
char *s = N_("Cannot read the license file \"{0}\", due to an\n"
	"      installation problem.  You will not be able to use this provider until\n"
	"      you can accept its license.");
/* mail:checking-service primary */
char *s = N_("Please wait.");
/* mail:checking-service secondary */
char *s = N_("Querying server for a list of supported authentication mechanisms.");
/* mail:gw-accountsetup-error primary */
char *s = N_("Unable to connect to the GroupWise\n"
	"server.");
/* mail:gw-accountsetup-error secondary */
char *s = N_("\n"
	"Please check your account settings and try again.\n"
	"");
