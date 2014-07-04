Feature: Addressbook: File: Create contacts

  Background:
    * Open Evolution and setup fake account
    * Open "Contacts" section
    * Select "Personal" addressbook
    * Change categories view to "Any Category"
    * Delete all contacts containing "Doe"

    @addressbook_contacts
    Scenario: Create a simple contact
      * Create a new contact
      * Set "Full Name..." in contact editor to "John Doe"
      * Save the contact
      * Refresh addressbook
      * Select "Doe, John" contact
      * Open contact editor for selected contact
      Then "Full Name..." property is set to "John Doe"

    @addressbook_contacts
    Scenario: Create a new contact with data
      * Create a new contact
      * Set "Full Name..." in contact editor to "Jimmy Doe"
      * Set "Nickname:" in contact editor to "Unknown"
      * Set emails in contact editor to
        | Field       | Value                    |
        | Work Email  | jimmy.doe@company.com    |
        | Home Email  | jimmy_doe_72@gmail.com   |
        | Other Email | jimmydoe72@yahoo.com     |
        | Other Email | xxjimmyxx@free_email.com |
      * Tick "Wants to receive HTML mail" checkbox
      * Set phones in contact editor to
        | Field           | Value |
        | Assistant Phone | 123   |
        | Business Phone  | 234   |
        | Business Fax    | 345   |
        | Callback Phone  | 456   |
        | Car Phone       | 567   |
        | Company Phone   | 678   |
        | Home Phone      | 789   |
        | Home Fax        | 890   |
        | ISDN            | 123   |
        | Mobile Phone    | 234   |
        | Other Phone     | 345   |
        | Other Fax       | 456   |
        | Pager           | 567   |
        | Primary Phone   | 678   |
        | Radio           | 789   |
        | Telex           | 890   |
      * Set IMs in contact editor to
        | Field     | Value     |
        | AIM       | 123       |
        | Jabber    | 234       |
        | Yahoo     | 345       |
        | Gadu-Gadu | 456       |
        | MSN       | 123       |
        | ICQ       | 234       |
        | GroupWise | 345       |
        | Skype     | jimmy.doe |
        | Twitter   | @jimmydoe |
      * Save the contact
      * Refresh addressbook
      * Select "Doe, Jimmy" contact
      * Open contact editor for selected contact
      Then "Nickname:" property is set to "Unknown"
       And Emails are set to
        | Field       | Value                    |
        | Work Email  | jimmy.doe@company.com    |
        | Home Email  | jimmy_doe_72@gmail.com   |
        | Other Email | jimmydoe72@yahoo.com     |
        | Other Email | xxjimmyxx@free_email.com |
       And "Wants to receive HTML mail" checkbox is ticked
       And Phones are set to
        | Field           | Value |
        | Assistant Phone | 123   |
        | Business Phone  | 234   |
        | Business Fax    | 345   |
        | Callback Phone  | 456   |
        | Car Phone       | 567   |
        | Company Phone   | 678   |
        | Home Phone      | 789   |
        | Home Fax        | 890   |
        | ISDN            | 123   |
        | Mobile Phone    | 234   |
        | Other Phone     | 345   |
        | Other Fax       | 456   |
        | Pager           | 567   |
        | Primary Phone   | 678   |
        | Radio           | 789   |
        | Telex           | 890   |
       And IMs are set to
        | Field     | Value     |
        | AIM       | 123       |
        | Jabber    | 234       |
        | Yahoo     | 345       |
        | Gadu-Gadu | 456       |
        | MSN       | 123       |
        | ICQ       | 234       |
        | GroupWise | 345       |
        | Skype     | jimmy.doe |
        | Twitter   | @jimmydoe |
