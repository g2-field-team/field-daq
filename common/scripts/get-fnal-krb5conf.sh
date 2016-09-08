#!/bin/bash

if [ "$(grep 'FNAL.GOV' /etc/krb5.conf)" ]; then
    echo -e "\e[31mAlready installed FNAL kerberos config\e[0m"
else 
    echo -e "\e[31mInstalling FNAL kerberos config\e[0m"
    wget http://computing.fnal.gov/authentication/krb5conf/Linux/krb5.conf -O /etc/krb5.conf
fi

echo -e "\e[31mGet a kerberos ticket like: kinit <fnal-user>@FNAL.GOV\e[0m"




