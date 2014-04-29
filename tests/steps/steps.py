# -*- coding: UTF-8 -*-
from behave import step, then
from common_steps import wait_until
from dogtail.tree import root
from dogtail.rawinput import keyCombo
from time import sleep, time
from os import system
from gi.repository import Gio, GLib


@step(u'Help section "{name}" is displayed')
def help_is_displayed(context, name):
    try:
        context.yelp = root.application('yelp')
        frame = context.yelp.child(roleName='frame')
        wait_until(lambda x: x.showing, frame)
        sleep(1)
        context.assertion.assertEquals(name, frame.name)
    finally:
        system("killall yelp")


@step(u'Evolution has {num:d} window opened')
@step(u'Evolution has {num:d} windows opened')
def evolution_has_num_windows_opened(context, num):
    windows = context.app.findChildren(lambda x: x.roleName == 'frame')
    context.assertion.assertEqual(len(windows), num)


@step(u'Preferences dialog is opened')
def preferences_dialog_opened(context):
    context.app.window('Evolution Preferences')


@step(u'"{name}" view is opened')
def view_is_opened(context, name):
    if name != 'Mail':
        window_name = context.app.children[0].name
        context.assertion.assertEquals(window_name, "%s - Evolution" % name)
    else:
        # A special case for Mail
        context.assertion.assertTrue(context.app.menu('Message').showing)


def get_visible_searchbar(context):
    """Wait for searchbar to become visible"""
    def get_searchbars():
        return context.app.findChildren(lambda x: x.labeller.name == 'Search:' and x.showing)
    assert wait_until(lambda x: len(x()) > 0, get_searchbars), "No visible searchbars found"
    return get_searchbars()[0]


@step(u'Open "{section_name}" section')
def open_section_by_name(context, section_name):
    wait_until(lambda x: x.showing, context.app.menu('View'))
    sleep(0.2)
    context.app.menu('View').click()
    context.app.menu('View').menu('Window').point()
    context.app.menu('View').menu('Window').menuItem(section_name).click()

    # Find a search bar
    context.app.search_bar = get_visible_searchbar(context)

    # Check that service required for this sections is running
    required_services = {
        'Mail': 'org.gnome.evolution.dataserver.Sources',
        'Calendar': 'org.gnome.evolution.dataserver.Calendar',
        'Tasks': 'org.gnome.evolution.dataserver.Calendar',
        'Memos': 'org.gnome.evolution.dataserver.Calendar',
        'Contacts': 'org.gnome.evolution.dataserver.AddressBook',
    }
    required_service = required_services[section_name]
    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    dbus_proxy = Gio.DBusProxy.new_sync(bus, Gio.DBusProxyFlags.NONE, None,
                                        'org.freedesktop.DBus',
                                        '/org/freedesktop/DBus',
                                        'org.freedesktop.DBus', None)
    for attempt in xrange(0, 10):
        result = dbus_proxy.call_sync(
            'ListNames', None, Gio.DBusCallFlags.NO_AUTO_START, 500, None)
        sleep(1)
        if True in [required_service in x for x in result[0]]:
            return
    raise RuntimeError("%s service was not found" % required_service)


@step(u'"{name}" menu is opened')
def menu_is_opened(context, name):
    sleep(0.5)
    menu = context.app.menu(name)
    children_displayed = [x.showing for x in menu.children]
    context.assertion.assertTrue(True in children_displayed, "Menu '%s' is not opened" % name)


@step(u'Press "{sequence}"')
def press_button_sequence(context, sequence):
    keyCombo(sequence)
    sleep(0.5)


@then(u'Evolution is closed')
def evolution_is_closed(context):
    assert wait_until(lambda x: x.dead, context.app),\
        "Evolution window is opened"
    context.assertion.assertFalse(context.app_class.isRunning(), "Evolution is in the process list")


@step(u'Message composer with title "{name}" is opened')
def message_composer_is_opened(context, name):
    context.app.composer = context.app.window(name)


@then(u'Contact editor window with title "{title}" is opened')
def contact_editor_with_label_is_opened(context, title):
    context.app.contact_editor = context.app.dialog(title)
    context.assertion.assertIsNotNone(
        context.app.contact_editor, "Contact Editor was not found")
    context.assertion.assertTrue(
        context.app.contact_editor.showing, "Contact Editor didn't appear")


@then(u'Contact editor window is opened')
def contact_editor_is_opened(context):
    context.execute_steps(u'Then Contact editor window with title "Contact Editor" is opened')


@then(u'Contact List editor window is opened')
def contact_list_editor_is_opened(context):
    context.execute_steps(
        u'Then Contact List editor window with title "Contact List Editor" is opened')


@then(u'Contact List editor window with title "{name}" is opened')
def contact_list_editor__with_name_is_opened(context, name):
    context.app.contact_list_editor = context.app.dialog(name)


@step(u'Memo editor with title "{name}" is opened')
def memo_editor_is_opened(context, name):
    context.execute_steps(u'* Task editor with title "%s" is opened' % name)


@step(u'Shared Memo editor with title "{name}" is opened')
def shared_memo_editor_is_opened(context, name):
    context.execute_steps(u'* Task editor with title "%s" is opened' % name)


@step(u'Task editor with title "{title}" is opened')
def task_editor_with_title_is_opened(context, title):
    context.app.task_editor = context.app.window(title)
    # Spoof event_editor for assigned tasks
    if 'Assigned' in title:
        context.app.event_editor = context.app.task_editor


@step(u'Event editor with title "{name}" is displayed')
def event_editor_with_name_displayed(context, name):
    context.app.event_editor = context.app.window(name)


@step(u'Wait for email to synchronize')
def wait_for_mail_folder_to_synchronize(context):
    # Wait until Google calendar is loaded
    for attempt in range(0, 10):
        start_time = time()
        try:
            spinners = context.app.findChildren(lambda x: x.name == 'Spinner')
            for spinner in spinners:
                try:
                    while spinner.showing:
                        sleep(0.1)
                        if (time() - start_time) > 180:
                            raise RuntimeError("Mail takes too long to synchronize")
                except GLib.GError:
                    continue
        except (GLib.GError, TypeError):
            continue
