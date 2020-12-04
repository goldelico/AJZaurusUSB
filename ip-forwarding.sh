#! /bin/bash

# based on
# http://apple.stackexchange.com/questions/228936/using-server-5-0-15-to-share-internet-without-internet-sharing
# https://superuser.com/questions/931827/sharing-openvpn-on-mac-os-x-yosemite

DEVICE=192.168.0.202	# Letux device

if ifconfig "utun0" >/dev/null
then # VPN hides highest priority active network
	CURRENT="tunnelblick"
	INTERFACE="utun0"

else

# locate highest priority active service
# found at https://apple.stackexchange.com/questions/191879/how-to-find-the-currently-connected-network-service-from-the-command-line
# and improved...

	while read LINE
	do
		NAME=$(echo $LINE | awk -F  "(, )|(: )|[)]" '{print $2}')
		DEV=$(echo $LINE | awk -F  "(, )|(: )|[)]" '{print $4}')
		# echo "Current service: $NAME, $DEV, $CURRENT, $INTERFACE"
		if [ "$DEV" ]
		then
			if ifconfig "$DEV" 2>/dev/null | grep -q 'status: active'
			then
				CURRENT="$NAME"
				INTERFACE="$DEV"
				break 2	# take first we find
			fi
		fi
	done < <(networksetup -listnetworkserviceorder | grep 'Hardware Port')

	if [ -n "$CURRENT" ]
	then
		echo Sharing over: $CURRENT $INTERFACE
	else
		>&2 echo "Could not find current service"
		exit 1
	fi

fi

sudo pfctl -F nat

(
echo "nat log on $INTERFACE from $DEVICE to any -> ($INTERFACE)"
INTERFACE=en0
echo "nat log on $INTERFACE from $DEVICE to any -> ($INTERFACE)"
INTERFACE=en3
echo "nat log on $INTERFACE from $DEVICE to any -> ($INTERFACE)"
) | sudo pfctl -N -f - -e
sudo pfctl -a '*' -s nat

sudo sysctl -w net.inet.ip.forwarding=1
sudo sysctl -w net.inet.ip.fw.enable=1
# sudo sysctl -w net.inet6.ip6.forwarding=1

## logging
# ifconfig pflog1 create
# sudo tcpdump -n -e -ttt -i pflog1
# sudo tcpdump -n -e -ttt -i $INTERFACE
