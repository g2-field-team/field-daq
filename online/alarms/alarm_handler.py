#!/usr/bin/python
import sys
from email.mime.text import MIMEText
from smtplib import SMTP_SSL


def get_alarm_sender(alarm_type):
    """Retrieve sender emails for this alarm."""
    return 'g2.field.alarms@gmail.com'


def get_alarm_recipients(alarm_type):
    """Retrieve expert contacts on this alarm."""
    rec = [get_alarm_sender(alarm_type)]

    if (alarm_type == 'error' or alarm_type == 'failure'):
        rec.append('mwsmith2112@gmail.com')

    if (alarm_type == 'failure'):
        rec.append('6186918870@txt.att.net')
    
    return rec


def send_emails(subject, body, sender, recipients):
    """Send the email to all recipients in the list."""
    msg = MIMEText(body)
    msg['Subject'] = subject

    s = SMTP_SSL('smtp.gmail.com', 465, timeout=10)
    try:
        s.login(sender, 'g2precess')
        s.sendmail(sender, recipients, msg.as_string())

    finally:
        s.quit()


def handle_alarm(alarm_type, alarm_msg):
    """Handle the cases of different alarms."""
    sender = get_alarm_sender(alarm_type)
    recipients = get_alarm_recipients(alarm_type)
    send_emails('field-daq: ' + alarm_type, alarm_msg, sender, recipients)


def main():

    alarm_type = sys.argv[1]
    alarm_msg = sys.argv[2]
    handle_alarm(alarm_type, alarm_msg)

    return 0


if __name__ == '__main__':
    sys.exit(main())

    
