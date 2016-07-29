#! /bin/sh

# based on
# http://apple.stackexchange.com/questions/228936/using-server-5-0-15-to-share-internet-without-internet-sharing

### besser: aus networksetup -listallnetworkservices heraussuchen

internet=en1
[ "$1" ] && internet=$1
device=192.168.0.202	# Letux device

sudo pfctl -F nat

echo "nat log on $internet from $device to any -> ($internet)" | sudo pfctl -N -f - -e
sudo pfctl -a '*' -s nat

sudo sysctl -w net.inet.ip.forwarding=1
sudo sysctl -w net.inet.ip.fw.enable=1
# sudo sysctl -w net.inet6.ip6.forwarding=1

## logging
# ifconfig pflog1 create
# sudo tcpdump -n -e -ttt -i pflog1
# sudo tcpdump -n -e -ttt -i $internet
