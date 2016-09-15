#!/bin/bash
# The script starts general midas utilites for midas.

source $(dirname $(readlink -f $0))/../../common/.expt-env

odbedit -e $EXPT -c clean
addresslist="-a $EXPT_IP -a localhost"

for addr in $EXT_IP; do
    addresslist="$addresslist -a $addr"
done

# Restart the daq components for the basic_vme setup.
for mu in "${MIDAS_UTIL[@]}"; do

    case $mu in
        'mserver')
            cmd="mserver -p $MSERVER_PORT -e $EXPT $(printf \\r)"
            screen -dmS "${EXPT}.mserver"
	    sleep 0.05
            screen -S "${EXPT}.mserver" -p 0 -rX stuff "$cmd";;
        
        'mhttpd')
            cmd="mhttpd  -e $EXPT $addresslist --nomg --oldserver $MHTTPD_PORT$(printf \\r)"
            screen -dmS "${EXPT}.mhttpd"
	    sleep 0.05
            screen -S "${EXPT}.mhttpd" -p 0 -rX stuff "$cmd";;
        
        'mlogger')
            cmd="mlogger -e $EXPT$(printf \\r)"
            screen -dmS "${EXPT}.mlogger"
	    sleep 0.05
            screen -S "${EXPT}.mlogger" -p 0 -rX stuff "$cmd";;
        
        'mevb')
            cmd="mevb -e $EXPT -b BUF$(printf \\r)"
            screen -dmS "${EXPT}.mevb"
	    sleep 0.05
            screen -S "${EXPT}.mevb" -p 0 -rX stuff "$cmd";;
    esac
done

unset cmd
unset addresslist
# end script
