WLAN=en1
sudo sysctl -w net.inet.ip.forwarding=1
sudo natd -interface $WLAN
sudo ipfw add divert natd ip from any to any via $WLAN
