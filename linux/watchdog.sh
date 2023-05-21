#!/bin/sh

# implement a watchdog service for the wifi connection
# Try 3 restarts , then check-firmware version and if it doesn't match the installed, reinstall firmware and retry 3 restarts

cd "$(dirname "$0")"
# SERVICEIP any you want... that should be pingable...
SERVICEIP="google.com"
SERVICEIP="1.1.1.1"
export LC_ALL=C

DECODE=decode-esp
UART=/dev/ttyS1
TUN=tun2

GOODFW="./good-version"

VERSION_OK="$(test -f $GOODFW/version.info && cat $GOODFW/version.info)"

while true; do
	ping -w 10 -c 1 $SERVICEIP >/dev/null 2>&1 && STATE="Good"
	case ${STATE:="Unknown"} in
		Unknown) STATE="Timeout"
			 sleep 5;
			;;
		Updated) echo "This is bad -- I cannot get the interface back working...";
			exit 1;
			;;
		TimeoutXXX) 
			# get esp-firmware version
			VERSION="$(systemctl status -a esp32-wifi | perl -ne 'next unless /Version Information=\[(.*)\]/; print $1')"
			echo "Version Now:$VERSION"
			echo "Version OK: $VERSION_OK"
			if [ "$VERSION" != "$VERSION_OK" ]; then
				echo "Update software now"
				systemctl stop esp32-wifi
				sh $GOODFW/update_esp.sh
				systemctl start  esp32-wifi
				STATE="Updated"
				sleep 5;
			else
				echo "Try reboot -- maybe something else is broken..."
				systemctl reboot
				exit 1;
			fi
			;;
		TimeoutXX)  systemctl restart esp32-wifi;
			   echo "Service restart";
			sleep 5;
			   STATE="${STATE}X";
			;;
		Timeout*) 
			pkill $DECODE
			echo "try tool kill"
			sleep 5;
			STATE="${STATE}X";
			;;
		Good)   # Be quiet if all is good..
			if [ ! -f $GOODFW/version.info ]; then
				# Capture version
				VERSION="$(systemctl status -a esp32-wifi | perl -ne 'next unless /Version Information=\[(.*)\]/; print $1')"
				echo "$VERSION" > $GOODFW/version.info
				echo "Good -- Version noted"
			fi
			exit 0;;
	esac
done



		

# 
# if 
# echo "Alive..."
# find /sys  -name brightness | while read l ; do echo 0 > $l ; done
# echo "$STATE $NOW" >$FLAG
# elif [ $(($LASTCHECK + 60)) -lt "$NOW" ] ; then
# # more than 1 minute ago
# find /sys  -name brightness | while read l ; do echo 1 > $l ; done
# 
# 
# 
# 
# 
