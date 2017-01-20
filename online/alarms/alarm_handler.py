#!/usr/bin/python
import sys
from email.mime.text import MIMEText
from smtplib import SMTP_SSL


def get_alarm_sender(alarm_type):
    """Retrieve sender emails for this alarm."""
    return 'g2.field.alarms@gmail.com'


def get_alarm_recipients(alarm_type, subsystem=None):
    """Retrieve expert contacts on this alarm."""
    rec = [get_alarm_sender(alarm_type)]

    if (alarm_type == 'error' or alarm_type == 'failure'):
        for r in get_expert_emails(subsystem):
            rec.append(r)

    if (alarm_type == 'failure'):
        for r in get_expert_phones(subsystem):
            rec.append(r)
    
    return rec


def get_expert_emails(subsystem='general'):
    """Load the experts text file and parse for the specified 
    subsystem (default to general)."""
    experts = []
    
    with open('expert_emails.txt', 'r') as f:
        for line in f:
            if line.startswith('#'):
                continue
            
            if subsystem in line:
                experts.append(line.split(',')[0])
                
    return experts


def get_expert_phones(subsystem='general'):
    """Load the experts text file and parse for the specified 
    subsystem (default to general)."""
    experts = []

    with open('expert_phones.txt', 'r') as f:
        for line in f:
            if line.startswith('#'):
                continue
            
            if subsystem in line:
                experts.append(line.split(',')[0])
                
    return experts


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
    field_subsystems = []
    field_subsystems.append('general')
    field_subsystems.append('trolley')
    field_subsystems.append('fixed-probes')
    field_subsystems.append('absolute-probe')
    field_subsystems.append('surface-coils')
    field_subsystems.append('flux-gates')

    sender = get_alarm_sender(alarm_type)

    if alarm_msg.split(':')[0] in field_subsystems:
        recipients = get_alarm_recipients(alarm_type, alarm_msg.split(':')[0])
    else:
        recipients = get_alarm_recipients(alarm_type)

    send_emails('field-daq: ' + alarm_type, alarm_msg, sender, recipients)


def main():

    alarm_type = sys.argv[1]
    alarm_msg = sys.argv[2]
    handle_alarm(alarm_type, alarm_msg)

    return 0


if __name__ == '__main__':
    sys.exit(main())

    
