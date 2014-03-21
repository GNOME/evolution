Feature: Shortcuts

  Background:
    * Open Evolution and setup fake account

  @general_shortcuts
  Scenario: Ctrl-Q to quit application - two instances
    * Start a new Evolution instance
    * Press "<Control>Q"
    Then Evolution is closed

  @general_shortcuts
  Scenario: F1 to launch help
    * Press "<F1>"
    Then Help section "Evolution Mail and Calendar" is displayed

  @general_shortcuts
  Scenario: Shift-Ctrl-W to open a new window
    * Press "<Control><Shift>W"
    Then Evolution has 2 windows opened

  @general_shortcuts
  Scenario: Ctrl-W to close a window
    * Press "<Control><Shift>W"
    * Press "<Control>W"
    Then Evolution has 1 window opened

  @general_shortcuts
  Scenario: Ctrl-Shift-S to open Preferences
    * Press "<Control><Shift>S"
    Then Preferences dialog is opened

  @mail_shortcuts
  Scenario: Mail: Ctrl-Shift-M to compose new message
    * Open "Mail" section
    * Press "<Control><Shift>M"
    Then Message composer with title "Compose Message" is opened

  @contacts_shortcuts
  Scenario: Contacts: Ctrl-Shift-C to create new contact
    * Open "Contacts" section
    * Press "<Control><Shift>C"
    Then Contact editor window is opened

  @contacts_shortcuts
  Scenario: Contacts: Ctrl-Shift-L to create new contact list
    * Open "Contacts" section
    * Press "<Control><Shift>L"
    Then Contact List editor window is opened

  @calendar_shortcuts
  Scenario: Calendar: Ctrl-Shift-A to create new appointment
    * Open "Calendar" section
    * Press "<Control><Shift>A"
    Then Event editor with title "Appointment - No Summary" is displayed

  @calendar_shortcuts
  Scenario: Calendar: Ctrl-Shift-E to create new meeting
    * Open "Calendar" section
    * Press "<Control><Shift>E"
    Then Event editor with title "Meeting - No Summary" is displayed

  @calendar_shortcuts
  Scenario: Tasks: Ctrl-Shift-T to create new task
    * Open "Tasks" section
    * Press "<Control><Shift>T"
    Then Task editor with title "Task - No Summary" is opened

  @memos_shortcuts
  Scenario: Memos: Ctrl-Shift-O to create new memo
    * Open "Memos" section
    * Press "<Control><Shift>O"
    Then Memo editor with title "Memo - No Summary" is opened

  @memos_shortcuts
  Scenario: Memos: Ctrl-Shift-O to create new task
    * Open "Memos" section
    * Press "<Control><Shift>O"
    Then Shared memo editor with title "Memo - No Summary" is opened

  @view_shortcuts
  Scenario Outline: Ctrl+<1-5> to switch views
    * Press "<shortcut>"
    Then "<section>" view is opened

    Examples:
      | shortcut | section  |
      | <Ctrl>1  | Mail     |
      | <Ctrl>2  | Contacts |
      | <Ctrl>3  | Calendar |
      | <Ctrl>4  | Tasks    |
      | <Ctrl>5  | Memos    |

  @menu_shortcuts
  Scenario Outline: Menu shortcuts on all views
    * Open "<section>" section
    * Press "<shortcut>"
    Then "<menu>" menu is opened

    Examples:
      | section | shortcut | menu    |
      | Mail    | <Alt>F   | File    |
      | Mail    | <Alt>E   | Edit    |
      | Mail    | <Alt>V   | View    |
      | Mail    | <Alt>O   | Folder  |
      | Mail    | <Alt>M   | Message |
      | Mail    | <Alt>S   | Search  |
      | Mail    | <Alt>H   | Help    |

      | Contacts | <Alt>F   | File    |
      | Contacts | <Alt>E   | Edit    |
      | Contacts | <Alt>V   | View    |
      | Contacts | <Alt>A   | Actions |
      | Contacts | <Alt>S   | Search  |
      | Contacts | <Alt>H   | Help    |

      | Calendar | <Alt>F   | File    |
      | Calendar | <Alt>E   | Edit    |
      | Calendar | <Alt>V   | View    |
      | Calendar | <Alt>A   | Actions |
      | Calendar | <Alt>S   | Search  |
      | Calendar | <Alt>H   | Help    |

      | Tasks | <Alt>F | File    |
      | Tasks | <Alt>E | Edit    |
      | Tasks | <Alt>V | View    |
      | Tasks | <Alt>A | Actions |
      | Tasks | <Alt>S | Search  |
      | Tasks | <Alt>H | Help    |

      | Memos | <Alt>F | File    |
      | Memos | <Alt>E | Edit    |
      | Memos | <Alt>V | View    |
      | Memos | <Alt>S | Search  |
      | Memos | <Alt>H | Help    |
