# -*- coding: UTF-8 -*-
from behave import step

from common_steps import check_for_errors
from dogtail.tree import root
from os import system
from pyatspi import STATE_SENSITIVE
from time import sleep


@step(u'Open Evolution and setup fake account')
def open_evolution_and_setup_fake_account(context):
    system("evolution --force-shutdown 2&> /dev/null")
    context.execute_steps(u'* Start a new Evolution instance')
    window = context.app.child(roleName='frame')
    if window.name == 'Welcome':
        context.execute_steps(u"""
            * Complete Welcome dialog in Evolution Account Assistant
            * Complete Restore from Backup dialog in Evolution Account Assistant
            * Complete Identity dialog setting name to "GNOME QE User" and email address to "test@test"
            * Wait for account is being looked up dialog in Evolution Account Assistant
            * Complete Receiving Email dialog of Evolution Account Assistant setting
              | Field        | Value |
              | Server Type: | None  |
            * Complete Sending Email dialog of Evolution Account Assistant setting
              | Field        | Value    |
              | Server Type: | Sendmail |
            * Complete Account Summary in Evolution Account Assistant
            * Complete Done dialog in Evolution Account Assistant
            """)


@step(u'Complete Receiving Options in Evolution Account Assistant')
@step(u'Complete Account Summary in Evolution Account Assistant')
@step(u'Complete Restore from Backup dialog in Evolution Account Assistant')
@step(u'Complete Welcome dialog in Evolution Account Assistant')
def evo_account_assistant_dummy_dialogs(context):
    # nothing to do here, skip it
    window = context.app.child(roleName='frame')
    click_next(window)


@step(u'Complete Identity dialog setting name to "{name}" and email address to "{email}"')
def evo_account_assistant_identity_dialog(context, name, email):
    # nothing to do here, skip it
    window = context.app.child(roleName='frame')
    window.childLabelled("Full Name:").text = name
    window.childLabelled("Email Address:").text = email
    click_next(window)


@step(u"Wait for account is being looked up dialog in Evolution Account Assistant")
def wait_for_account_to_be_looked_up(context):
    window = context.app.child(roleName='frame')
    skip_lookup = window.findChildren(lambda x: x.name == 'Skip Lookup')
    visible_skip_lookup = [x for x in skip_lookup if x.showing]
    if len(visible_skip_lookup) > 0:
        visible_skip_lookup = visible_skip_lookup[0]
        # bug https://bugzilla.gnome.org/show_bug.cgi?id=726539: Skip Lookup is not being removed
        #assert wait_until(lambda x: not x.showing, visible_skip_lookup),\
        #    "Skip Lookup button didn't dissappear"


def click_next(window):
    # As initial wizard dialog creates a bunch of 'Next' buttons
    # We have to click to the visible and enabled one
    buttons = window.findChildren(lambda x: x.name == 'Next' and x.showing and
                                  STATE_SENSITIVE in x.getState().getStates())
    if buttons == []:
        raise Exception("Enabled Next button was not found")
    else:
        buttons[0].click()


@step(u'Complete {sending_or_receiving} Email dialog of Evolution Account Assistant setting')
def evo_account_assistant_receiving_email_dialog_from_table(context, sending_or_receiving):
    window = context.app.child(roleName='frame')
    for row in context.table:
        label = str(row['Field'])
        value = str(row['Value'])
        filler = window.child(roleName='filler', name='%s Email' % sending_or_receiving)
        widgets = filler.findChildren(lambda x: x.showing)
        visible_widgets = [x for x in widgets if x.labeller and x.labeller.name == label]
        if len(visible_widgets) == 0:
            raise RuntimeError("Cannot find visible widget labelled '%s'" % label)
        widget = visible_widgets[0]
        if widget.roleName == 'combo box':
            if label != 'Port:':
                widget.click()
                widget.menuItem(value).click()
            else:
                # Port is a combobox, but you can type your port there
                widget.textentry('').text = value
                widget.textentry('').grab_focus()
                widget.textentry('').keyCombo("<Enter>")
        if widget.roleName == 'text':
            widget.text = value

    # Check for password here and accept self-generated certificate (if appears)
    btns = window.findChildren(lambda x: x.name == 'Check for Supported Types')
    visible_btns = [w for w in btns if w.showing]
    if visible_btns == []:
        click_next(window)
        return
    visible_btns[0].click()

    # Confirm all certificates by clicking 'Accept Permanently' until dialog is visible
    apps = [x.name for x in root.applications()]
    if 'evolution-user-prompter' in apps:
        prompter = root.application('evolution-user-prompter')
        dialog = prompter.child(roleName='dialog')
        while dialog.showing:
            if prompter.findChild(lambda x: x.name == 'Accept Permanently', retry=False, requireResult=False):
                prompter.button('Accept Permanently').click()
            else:
                sleep(0.1)

    # Wait until Cancel button disappears
    cancel = filler.findChildren(lambda x: x.name == 'Cancel')[0]
    while cancel.showing:
        sleep(0.1)
    check_for_errors(context)
    click_next(window)


@step(u'Complete Done dialog in Evolution Account Assistant')
def evo_account_assistant_done_dialog(context):
    # nothing to do here, skip it
    window = context.app.child(roleName='frame')
    window.button('Apply').click()
