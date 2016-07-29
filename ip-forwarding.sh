#! /bin/sh
# http://apple.stackexchange.com/questions/178653/creating-a-bridged-network-connection-in-terminal
# ######################################
#  coded by Nat!
#  2013 Mulle kybernetiK
#  GPL

if false
then

command=${1:-start}
shift
proxyarp=${1:-no}
shift

start()
{
        sysctl -w net.inet.ip.forwarding=1
        sysctl -w net.inet.ip.fw.enable=1
        if [ "$proxyarp" != "no" ]
        then
                sysctl -w net.link.ether.inet.proxyall=1
        fi

        ifconfig bridge0 create
        ifconfig bridge0 addm en1
        ifconfig bridge0 addm en3
        ifconfig bridge0 up
        if [ $? -eq 0 ]
        then
                syslog -s "Ethernet Bridge is up"
        else
                syslog -s "Ethernet Bridge failure"
        fi
}


stop()
{
        ifconfig bridge0 destroy

        sysctl -w net.inet.ip.forwarding=0
        sysctl -w net.inet.ip.fw.enable=0
        sysctl -w net.link.ether.inet.proxyall=0

        syslog -s "Ethernet Bridge is down"
}


exit

fi

if true
then

# based on
# http://apple.stackexchange.com/questions/228936/using-server-5-0-15-to-share-internet-without-internet-sharing

function findinterface {
# scan ifconfig for IP address???
	case "$1" in
		192.168.2.107 )
			echo "en1"
			;;
		192.168.0.200 )
			echo "en3"
			;;
	esac
}

RULES=/private/etc/nat-rules
RULES=.nat-rules

internet=$(findinterface 192.168.2.107)
localnet=$(findinterface 192.168.0.200)
device=192.168.0.202

sudo pfctl -F nat

#echo "nat on $ext_if from $localnet to any -> ($ext_if)" | tee $RULES
echo "nat log on $internet from $device to any -> ($internet)" | sudo pfctl -N -f -
sudo pfctl -a '*' -s nat

sudo sysctl -w net.inet.ip.forwarding=1
sudo sysctl -w net.inet.ip.fw.enable=1
# sudo sysctl -w net.inet6.ip6.forwarding=1

#disables pfctl
#sudo pfctl -d

sleep 1

#flushes all pfctl rules
#sudo pfctl -F all

sleep 1

#starts pfctl and loads the rules from the nat-rules file
# sudo pfctl -N -f $RULES

#don't disable...
# sudo pfctl -E

## logging
# ifconfig pflog1 create
# sudo tcpdump -n -e -ttt -i pflog1

fi

if false
then

# http://apple.stackexchange.com/questions/243426/set-static-network-addresses-for-clients-in-os-x-shared-network
# http://chariotsolutions.com/blog/post/configuring-network-used-by-mac-os-x/

sudo tee /etc/bootptab <<END
#
# bootptab example
#
%%
# machine entries have the following format:
#
# hostname      hwtype  hwaddr              ipaddr          bootfile
rndis         1       e6:4f:4a:e6:a4:6a   192.168.0.200       rndis
END
echo restart Internet Sharing and DHCP leases


# no longer available on OS X 10.11
# WLAN=en1
# sudo natd -interface $WLAN
# sudo ipfw add divert natd ip from any to any via $WLAN

fi