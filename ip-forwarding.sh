sudo sysctl -w net.inet.ip.forwarding=1
# no longer needed on OS X 10.11
# WLAN=en1
# sudo natd -interface $WLAN
# sudo ipfw add divert natd ip from any to any via $WLAN
