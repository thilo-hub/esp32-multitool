#!/bin/sh

#set -vx

IP="10.1.1.9"

#ip tuntap add tun3  mode tun 
systemctl stop esp32-wifi
pkill -9 python3
pkill -9 decode

./decode /dev/ttyS1 tun3 2>&1 &
PID=$!
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

echo END


