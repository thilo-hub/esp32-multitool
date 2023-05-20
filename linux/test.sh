#!/bin/sh

#set -vx

IP="192.168.0.9"

#ip tuntap add tun3  mode tun 
systemctl stop esp32-wifi
pkill -9 python3
#pkill -9 decode

./decode /dev/ttyS1 tun3 2>&1 &
PID=$!
sleep 2
ifconfig tun3 192.168.0.158 netmask 255.255.255.0 up
ifconfig tun3
route add default gw  192.168.0.9
# tcpdump -i tun3 -w tun3.pcap &
sleep 10
while  ping -w 10 -c 1 $IP >/dev/null 2>&1 ; do
	echo Still ok
	sleep 10
#ping -c 1 192.168.0.9
# 	ifconfig tun3
done
echo No Answer
netstat -rn
ifconfig tun3

kill -9 $PID
pkill -9 decode
systemctl start esp32-wifi
# /home/thilo/bin/esp32_reset.sh
# /home/thilo/tmp.tun0.ok2/clnt_tst_orig.sh

echo END


