# -*- coding: UTF-8 -*-
from behave import step, then
from common_steps import wait_until
from dogtail.predicate import GenericPredicate
from dogtail.rawinput import keyCombo
from time import time, sleep
from gi.repository import GLib
import pyatspi


@step(u'Select "{name}" addressbook')
def select_addressbook(context, name, password=None):
    cells = context.app.findChildren(
        GenericPredicate(name=name, roleName='table cell'))
    visible_cells = filter(lambda x: x.showing, cells)
    if visible_cells == []:
        raise RuntimeError("Cannot find addressbook '%s'" % name)
    visible_cells[0].click()
    # Wait for addressbook to load
    try:
        spinner = context.app.findChild(
            GenericPredicate(name='Spinner'), retry=False, requireResult=False)
        if spinner:
            start_time = time()
            while spinner.showing:
                sleep(1)
                if (time() - start_time) > 180:
                    raise RuntimeError("Contacts take too long to synchronize")
    except (GLib.GError, TypeError):
        pass


@step(u'Change categories view to "{category}"')
def change_categories_view(context, category):
    labels = context.app.findChildren(
        lambda x: x.labeller.name == 'Show:' and x.showing)
    if labels == []:
        raise RuntimeError("Cannot find category switcher")
    labels[0].combovalue = category


@step(u'Delete selected contact')
def delete_selected_contact(context):
    context.app.menu('Edit').click()
    mnu = context.app.menu('Edit').menuItem("Delete Contact")
    if pyatspi.STATE_ENABLED in mnu.getState().getStates():
        context.app.menu('Edit').menuItem("Delete Contact").click()

        alert = context.app.child(roleName='alert', name='Question')
        alert.button('Delete').click()
    context.execute_steps(u"* Wait for email to synchronize")


@step(u'Delete all contacts containing "{part}"')
def delete_all_contacts_containing(context, part):
    context.app.search_bar.grab_focus()
    for attempts in range(0, 10):
        try:
            context.app.search_bar.text = part
            break
        except (GLib.GError, AttributeError):
            sleep(0.1)
            continue
    keyCombo("<Enter>")
    context.execute_steps(u"* Wait for email to synchronize")
    context.app.search_bar.grab_focus()
    keyCombo("<Tab>")
    sleep(3)
    heading = context.app.findChild(
        GenericPredicate(roleName='heading'),
        retry=False, requireResult=False)
    if heading:
        keyCombo("<Control>a")
        context.execute_steps(u"* Delete selected contact")
        sleep(3)


@step(u'Create a new contact')
def create_a_new_contact(context):
    context.app.menu('File').click()
    context.app.menu('File').menu('New').point()
    context.app.menu('File').menu('New').menuItem("Contact").click()
    context.execute_steps(u"Then Contact editor window is opened")


def get_element_by_name(contact_editor, name, section=None):
    """Get a field object by name in section (if specified)"""
    element = None
    if section:
        panel = contact_editor.findChild(
            GenericPredicate(roleName='panel', name=section), retry=False, requireResult=False)
        if not panel:
            # Other section is not a panel, but a toggle button
            panel = contact_editor.child(roleName='toggle button', name=section)
        element = panel.childLabelled(name)
    else:
        label = contact_editor.findChild(
            GenericPredicate(label=name), retry=False, requireResult=False)
        if not label:
            # In case childLabelled is missing
            # Find a filler with this name and get its text child
            element = contact_editor.child(
                roleName='filler', name=name).child(roleName='text')
        else:
            element = contact_editor.childLabelled(name)
    if element:
        return element
    else:
        raise RuntimeError("Cannot find element named '%s' in section '%s'" % (
            name, section))


@step(u'Set "{field_name}" in contact editor to "{field_value}"')
def set_field_to_value(context, field_name, field_value):
    element = get_element_by_name(context.app.contact_editor, field_name)
    if element.roleName == "text":
        element.text = field_value
    elif element.roleName == "combo box":
        if element.combovalue != field_value:
            element.combovalue = field_value


@step(u'Save the contact')
def save_contact(context):
    context.app.contact_editor.button('Save').click()
    assert wait_until(lambda x: not x.showing, context.app.contact_editor),\
        "Contact Editor was not hidden"
    assert wait_until(lambda x: x.dead, context.app.contact_editor),\
        "Contact Editor was not closed"
    context.app.contact_editor = None


@step(u'Refresh addressbook')
def refresh_addressbook(context):
    #Clear the search
    icons = context.app.search_bar.findChildren(lambda x: x.roleName == 'icon')
    if icons != []:
        icons[-1].click()
    else:
        for attempts in range(0, 10):
            try:
                context.app.search_bar.text = ''
                break
            except (GLib.GError, AttributeError):
                sleep(0.1)
                continue
        context.app.search_bar.grab_focus()
        keyCombo('<Enter>')
    context.execute_steps(u"* Wait for email to synchronize")


@step(u'Select "{contact_name}" contact list')
@step(u'Select "{contact_name}" contact')
def select_contact_with_name(context, contact_name):
    # heading shows the name of currently selected contact
    # We have to keep on pressing Tab to select the next contact
    # Until we meet the first contact
    # WARNING - what if we will have two identical contacts?
    fail = False
    selected_contact = None

    # HACK
    # To make the contact table appear
    # we need to focus on search window
    # and send Tabs to have the first contact focused
    context.app.search_bar.grab_focus()
    sleep(0.1)
    # Switch to 'Any field contains' (not reachable in 3.6)
    icons = context.app.search_bar.findChildren(GenericPredicate(roleName='icon'))

    if icons != []:
        icons[0].click()
        wait_until(lambda x: x.findChildren(
            GenericPredicate(roleName='check menu item', name='Any field contains')) != [],
            context.app)
        context.app.menuItem('Any field contains').click()
        for attempts in range(0, 10):
            try:
                context.app.search_bar.text = contact_name
                break
            except (GLib.GError, AttributeError):
                sleep(0.1)
                continue
        keyCombo("<Enter>")
        context.app.search_bar.grab_focus()

    keyCombo("<Tab>")
    first_contact_name = context.app.child(roleName='heading').text

    while True:
        selected_contact = context.app.child(roleName='heading')
        if selected_contact.text == contact_name:
            fail = False
            break
        keyCombo("<Tab>")
        # Wait until contact data is being rendered
        sleep(1)
        if first_contact_name == selected_contact.text:
            fail = True
            break

    context.assertion.assertFalse(
        fail, "Can't find contact named '%s'" % contact_name)
    context.selected_contact_text = selected_contact.text


@step(u'Open contact editor for selected contact')
def open_contact_editor_for_selected_contact(context):
    context.app.menu('File').click()
    context.app.menu('File').menuItem('Open Contact').click()
    context.execute_steps(u"""
        Then Contact editor window with title "Contact Editor - %s" is opened
        """ % context.selected_contact_text)


@then(u'"{field}" property is set to "{expected}"')
def property_in_contact_window_is_set_to(context, field, expected):
    element = get_element_by_name(context.app.contact_editor, field)
    actual = None
    if element.roleName == "text":
        actual = element.text
    elif element.roleName == "combo box":
        actual = element.combovalue
        if actual == '':
            actual = element.textentry('').text
    assert unicode(actual) == expected, "Incorrect value"


def get_combobox_textbox_object(contact_editor, section, scroll_to_bottom=True):
    """Get a list of paired 'combobox-textbox' objects in contact editor"""
    section_names = {
        'Ims': 'Instant Messaging',
        'Phones': 'Telephone',
        'Emails': 'Email'}
    section = section_names[section.capitalize()]
    lbl = contact_editor.child(roleName='toggle button', name=section)
    panel = lbl.findAncestor(GenericPredicate(roleName='filler'))
    textboxes = panel.findChildren(GenericPredicate(roleName='text'))

    # Scroll to the bottom of the page if needed
    pagetab = panel.findAncestor(GenericPredicate(roleName='page tab'))
    for scroll in pagetab.findChildren(lambda x: x.roleName == 'scroll bar'):
        if scroll_to_bottom:
            scroll.value = scroll.maxValue
        else:
            scroll.value = 0

    # Expand section if button exists
    button = panel.findChild(
        GenericPredicate(roleName='push button', name=section),
        retry=False, requireResult=False)
    # Expand button if any of textboxes is not visible
    if button and (False in [x.showing for x in textboxes]):
        button.click()

    comboboxes = panel.findChildren(GenericPredicate(roleName='combo box'))

    # Rearrange comboboxes and textboxes according to their position
    result = []
    for combo in comboboxes:
        combo_row = combo.position[1]
        matching_textboxes = [
            x for x in textboxes
            if ((x.position[1] - combo_row) == 0) and (x.position[0] > combo.position[0])]
        if (matching_textboxes != []):
            correct_textbox = min(matching_textboxes, key=lambda x: x.position[0])
            result.append((combo, correct_textbox))

    comboboxes = [x[0] for x in result][::-1]
    textboxes = [x[1] for x in result][::-1]

    return (textboxes, comboboxes, button)


@step(u'Set {section} in contact editor to')
def set_contact_emails_to_value(context, section):
    (textboxes, comboboxes, collapse_button) = get_combobox_textbox_object(
        context.app.contact_editor, section)

    # clear existing data
    for textbox in textboxes:
        textbox.text = ""

    for index, row in enumerate(context.table.rows):
        # Check that we have sufficient amount of textboxes
        # If not - click plus buttons until we have enough
        if index == len(textboxes):
            textboxes[0].parent.child(roleName="push button").click()
            (textboxes, comboboxes, collapse_button) = get_combobox_textbox_object(
                context.app.contact_editor, section)
        textboxes[index].text = row['Value']
        if comboboxes[index].combovalue != row['Field']:
            comboboxes[index].combovalue = row['Field']


@then(u'{section} are set to')
def emails_are_set_to(context, section):
    (textboxes, comboboxes, collapse_button) = get_combobox_textbox_object(
        context.app.contact_editor, section, section == 'IMs')

    actual = []
    for index, textbox in enumerate(textboxes):
        combo_value = textbox.text
        if combo_value.strip() != '':
            type_value = comboboxes[index].combovalue
            actual.append({'Field': unicode(type_value), 'Value': unicode(combo_value)})
    actual = sorted(actual)

    expected = []
    for row in context.table:
        expected.append({'Field': row['Field'], 'Value': row['Value']})
    expected = sorted(expected)

    assert actual == expected, "Incorrect %s value:\nexpected:%s\n but was:%s" % (
        row['Field'], expected, actual)


@step(u'Tick "Wants to receive HTML mail" checkbox')
def tick_checkbox(context):
    context.app.contact_editor.childNamed("Wants to receive HTML mail").click()


@step(u'"Wants to receive HTML mail" checkbox is ticked')
def checkbox_is_ticked(context):
    check_state = context.app.childNamed("Wants to receive HTML mail").checked
    assert check_state, "Incorrect checkbox state"


@step(u'Switch to "{name}" tab in contact editor')
def switch_to_tab(context, name):
    context.app.contact_editor.tab(name).click()


@step(u'Set the following properties in contact editor')
def set_properties(context):
    for row in context.table.rows:
        context.execute_steps(u"""
            * Set "%s" in contact editor to "%s"
        """ % (row['Field'], row['Value']))


@step(u'The following properties in contact editor are set')
def verify_properties(context):
    for row in context.table.rows:
        context.execute_steps(u"""
            Then "%s" property is set to "%s"
        """ % (row['Field'], row['Value']))


@step(u'Set the following properties in "{section}" section of contact editor')
def set_properties_in_section(context, section):
    for row in context.table.rows:
        context.execute_steps(u"""
            * Set "%s" in "%s" section of contact editor to "%s"
        """ % (row['Field'], section, row['Value']))


@step(u'The following properties in "{section}" section of contact editor are set')
def verify_properties_in_section(context, section):
    for row in context.table.rows:
        context.execute_steps(u"""
            Then "%s" property in "%s" section is set to "%s"
        """ % (row['Field'], section, row['Value']))


@step(u'Set the following note for the contact')
def set_note_for_contact(context):
    context.app.contact_editor.child(
        roleName='page tab', name='Notes').textentry('').text = context.text


@then(u'The following note is set for the contact')
def verify_note_set_for_contact(context):
    actual = context.app.contact_editor.child(
        roleName='page tab', name='Notes').textentry('').text
    expected = context.text
    assert actual == expected,\
        "Incorrect note value:\nexpected:%s\n but was:%s" % (expected, actual)


@step(u'Set "{field_name}" in "{section}" section of contact editor to "{field_value}"')
def set_field_in_section_to_value(context, field_name, section, field_value):
    element = get_element_by_name(
        context.app.contact_editor, field_name, section=section)
    if element.roleName == "text":
        element.text = field_value
    elif element.roleName == "combo box":
        element.combovalue = field_value
