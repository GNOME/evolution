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
    Scenario: Create a contact with full data
      * Create a new contact
      * Set "Full Name..." in contact editor to "Jimmy Doe"
      * Set "Nickname:" in contact editor to "Unknown"
      * Set emails in contact editor to
        | Field | Value                    |
        | Work  | jimmy.doe@company.com    |
        | Home  | jimmy_doe_72@gmail.com   |
        | Other | jimmydoe72@yahoo.com     |
        | Other | xxjimmyxx@free_email.com |
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
      * Set IMs in contact editor to
        | Field     | Value |
        | AIM       | 123   |
        | Jabber    | 234   |
        | Yahoo     | 345   |
        | Gadu-Gadu | 456   |
      * Switch to "Personal Information" tab in contact editor
      * Set the following properties in contact editor
        | Field       | Value                              |
        | Home Page:  | http://anna-doe.com                |
        | Blog:       | http://blog.anna-doe.com           |
        | Calendar:   | caldav://anna-doe.com/calendar.ics |
        | Free/Busy:  | http://anna-doe.com/free-busy      |
        | Video Chat: | http://anna-doe.com/video-chat     |
        | Profession: | QA Engineer                        |
        | Title:      | Dr.                                |
        | Company:    | Something Ltd.                     |
        | Department: | Desktop QA                         |
        | Manager:    | John Doe                           |
        | Assistant:  | Anna Doe                           |
        | Office:     | 221b                               |
        | Spouse:     | Jack Doe                           |
      * Switch to "Mailing Address" tab in contact editor
      * Set the following properties in "Home" section of contact editor
        | Field            | Value             |
        | City:            | Brno              |
        | Zip/Postal Code: | 61245             |
        | State/Province:  | Brno-Kralovo Pole |
        | Country:         | Czech Republic    |
        | PO Box:          | 123456            |
        | Address:         | Purkynova 99/71   |
      * Set the following properties in "Work" section of contact editor
        | Field            | Value             |
        | City:            | Brno              |
        | Zip/Postal Code: | 61245             |
        | State/Province:  | Brno-Kralovo Pole |
        | Country:         | Czech Republic    |
        | PO Box:          | 123456            |
        | Address:         | Purkynova 99/71   |
      * Set the following properties in "Other" section of contact editor
        | Field            | Value             |
        | City:            | Brno              |
        | Zip/Postal Code: | 61245             |
        | State/Province:  | Brno-Kralovo Pole |
        | Country:         | Czech Republic    |
        | PO Box:          | 123456            |
        | Address:         | Purkynova 99/71   |
      * Switch to "Notes" tab in contact editor
      * Set the following note for the contact:
      """
      Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed dignissim gravida elit, nec facilisis augue commodo quis.

      Sed ac metus quis tellus aliquet posuere non quis elit. Quisque non ante congue urna blandit accumsan.

      In vitae ligula risus. Nunc venenatis leo vel leo facilisis porta. Nam sed magna urna, venenatis.
      """
      * Refresh addressbook
      * Select "Doe, Jimmy" contact
      * Open contact editor for selected contact
      Then "Nickname:" property is set to "Unknown"
       And Emails are set to
        | Field | Value                    |
        | Work  | jimmy.doe@company.com    |
        | Home  | jimmy_doe_72@gmail.com   |
        | Other | jimmydoe72@yahoo.com     |
        | Other | xxjimmyxx@free_email.com |
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
       And IMs are set to
        | Field     | Value |
        | AIM       | 123   |
        | Jabber    | 234   |
        | Yahoo     | 345   |
        | Gadu-Gadu | 456   |
      * Switch to "Personal Information" tab in contact editor
      Then The following properties in contact editor are set
        | Field       | Value                              |
        | Home Page:  | http://anna-doe.com                |
        | Blog:       | http://blog.anna-doe.com           |
        | Calendar:   | caldav://anna-doe.com/calendar.ics |
        | Free/Busy:  | http://anna-doe.com/free-busy      |
        | Video Chat: | http://anna-doe.com/video-chat     |
        | Field       | Value                              |
        | Profession: | QA Engineer                        |
        | Title:      | Dr.                                |
        | Company:    | Something Ltd.                     |
        | Department: | Desktop QA                         |
        | Manager:    | John Doe                           |
        | Assistant:  | Anna Doe                           |
        | Office:     | 221b                               |
        | Spouse:     | Jack Doe                           |
      * Switch to "Mailing Address" tab in contact editor
      Then The following properties in "Home" section of contact editor are set
        | Field            | Value             |
        | City:            | Brno              |
        | Zip/Postal Code: | 61245             |
        | State/Province:  | Brno-Kralovo Pole |
        | Country:         | Czech Republic    |
        | PO Box:          | 123456            |
        | Address:         | Purkynova 99/71   |
       And The following properties in "Work" section of contact editor are set
        | Field            | Value             |
        | City:            | Brno              |
        | Zip/Postal Code: | 61245             |
        | State/Province:  | Brno-Kralovo Pole |
        | Country:         | Czech Republic    |
        | PO Box:          | 123456            |
        | Address:         | Purkynova 99/71   |
       And The following properties in "Other" section of contact editor are set
        | Field            | Value             |
        | City:            | Brno              |
        | Zip/Postal Code: | 61245             |
        | State/Province:  | Brno-Kralovo Pole |
        | Country:         | Czech Republic    |
        | PO Box:          | 123456            |
        | Address:         | Purkynova 99/71   |
      * Switch to "Notes" tab in contact editor
      Then The following note is set for the contact:
      """
      Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed dignissim gravida elit, nec facilisis augue commodo quis.

      Sed ac metus quis tellus aliquet posuere non quis elit. Quisque non ante congue urna blandit accumsan.

      In vitae ligula risus. Nunc venenatis leo vel leo facilisis porta. Nam sed magna urna, venenatis.
      """

    @addressbook_contacts
    Scenario: Create a contact with different "file under" field
      * Create a new contact
      * Set "Full Name..." in contact editor to "Jackie Doe"
      * Set "File under:" in contact editor to "Jackie Doe"
      * Save the contact
      * Refresh addressbook
      * Select "Jackie Doe" contact
      * Open contact editor for selected contact
      Then "Full Name..." property is set to "Jackie Doe"

    @addressbook_contacts
    Scenario: Create a contact with all phones and IM set (part 2)
      * Create a new contact
      * Set "Full Name..." in contact editor to "Kevin Doe"
      * Set IMs in contact editor to
        | Field     | Value |
        | MSN       | 123   |
        | ICQ       | 234   |
        | GroupWise | 345   |
        | Skype     | 456   |
      * Set phones in contact editor to
        | Field         | Value |
        | ISDN          | 123   |
        | Mobile Phone  | 234   |
        | Other Phone   | 345   |
        | Other Fax     | 456   |
        | Pager         | 567   |
        | Primary Phone | 678   |
        | Radio         | 789   |
        | Telex         | 890   |

      * Save the contact
      * Refresh addressbook
      * Select "Doe, Kevin" contact
      * Open contact editor for selected contact
      Then Phones are set to
        | Field         | Value |
        | ISDN          | 123   |
        | Mobile Phone  | 234   |
        | Other Phone   | 345   |
        | Other Fax     | 456   |
        | Pager         | 567   |
        | Primary Phone | 678   |
        | Radio         | 789   |
        | Telex         | 890   |
      And IMs are set to
        | Field     | Value |
        | MSN       | 123   |
        | ICQ       | 234   |
        | GroupWise | 345   |
        | Skype     | 456   |

    @addressbook_contacts
    Scenario: Create a contact with all IM set (part 2)
      * Create a new contact
      * Set "Full Name..." in contact editor to "Mary Doe"
      * Set IMs in contact editor to
        | Field   | Value |
        | Twitter | 123   |
        | ICQ     | 234   |
        | Jabber  | 345   |
        | Skype   | 456   |

      * Save the contact
      * Refresh addressbook
      * Select "Doe, Mary" contact
      * Open contact editor for selected contact
      Then IMs are set to
        | Field   | Value |
        | Twitter | 123   |
        | ICQ     | 234   |
        | Jabber  | 345   |
        | Skype   | 456   |
