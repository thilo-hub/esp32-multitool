# ESP Multitool

Since I have a couple of 10 year old beaglebone blacks that I want to put to live again, I decided to add a 3$ ESP32s3 and 1$ RF433 receiver/sender plus a usb-audio interface that I had in a drawer.

This gives me a cheap and good interface to enable apple-airplay (shairport-sync) speakers on an ancient sound system and to replace / centralize the cheap 433mhz power outlet switches that are floating around.

I use a "hand-wire-wrapped" bread board on the beaglebone (picture). Essentially except for the ESP32S3 and the cheap 433mhz receiver/transmitter there are no further parts involved. (you need a voltage divder (2K & 3K) though  to go from the 5V rf433 receiver to the 3.3v EPS GPIO's)

## Features

 - A wifi ip-tunnel interface to linux using the ESP32-wifi
   (for performance SPI & UART are used simultaniously giving ~3.5 mbit transferrate)
   to make the beaglebone **wireless**

 - A webServer that capturing and sending of RF433 wireless commands 
  to replace cheap 433mhz remote controls and enable to programaticaly switch the **power outlets**...

 - A simple UART-wifi interface that allows devices like the beaglebone boot-uart 
   to enable boot-debugging on **wireless beagleboneblack**"

 - A simple wifi interface that allows devices like the beaglebone to be reset (gpio-toggle)
     - Obviously have (some) authentication ( for the above uart/reset )

 - an IOS/macos app that shows the buttons available for RF433 


# Status

 - spi/uart tunnel working as a separate esp image

 - rf433/reset/uart working as a separate esp image
 - Capturing of rf433 signals using command line interface and small tools on a PC

# Content
        esptunnel       - ESP32 firmware that tunnels IP through SPI & UART to a beaglebone
        linux           - The beaglebone side of the tunnel
        rf433           - ESP32 webinterface/uart-interface
        Apple           - Xcode content for separate App


 - Currently configured HW/connections

      
 BBK Header| Pin| Connection| ESP32s3 Pin| ESP32s3 Function
--:|:-----:|:-----|:-----|:-----
P9 | 1/2|         GND
P9 | 3/4|         VCC 5V
P9 | 56/|         VDD 3.3V
P9 | 19| D17      uart1_rtsn|  23| IO15 I/O CTS
P9 | 20| D18      uart1_ctsn|  13| IO14 I/O RTS
P9 | 24| D15      UART1_TXD|   27| IO16 I/O U2RXD
P9 | 26| D16      UART1_RXD|   28| IO17 I/O U2TXD
P9 | 23| V14      GPIO1_17|    3| EN I Module-enable signal. Active high.
P9 | 25| A14      GPIO3_21*|   25| IO0 I/O GPIO0 - doubles as SPI-ACK from ESP
P9 | 22| A17      UART2_RXD|   35| TXD0 I/O GPIO1
P9 | 21| B17      UART2_TXD|   34| RXD0 I/O GPIO3
P9 | 28| C12     SPI1_CS0  |  xx | CS I/O
P9 | 29| B13     SPI1_D0   |  xx | DI
P9 | 30| D12     SPI1_D1   |  xx | DO
P9 | 31| A13     SPI1_SCLK |  xx | SCLK
P? |  ?|  RESET            |  xx | GPIO?
P? |  4|  UART0 - RX       |  xx | U1RXD
P? |  5|  UART0 - TX       |  xx | U1TXD



-- Usage tunnel interface
    under linux/esp32 are some tools
        esp32_reset.sh  -- toggle esp32 reset
        setup_pins.sh   -- Setup beaglebone io pins
    
All configuration is in system.cfg now
copy the template system.cfg.default to system.cfg and edit it
the Makefiles will copy the config into the esp filesystem

Once the firmware is loaded and the ESP is started, it will wait on UART2 for configuration.
The ```decode``` tool will configure everything and start.


## building 
### Tunnel

Check the short Makefile!!

idf.py  set-target esp32s3
idf.py menuconfig - check what needs changing (in particular, there seems to be abuild problem if the web interface is not enabled)
idf.py build
idf.py flash
--OR--
idf.py  set-target esp32s3
idf.py menuconfig - check what needs changing
make HOST=10.1.1.73  ( the Flasher script is not part of the repository )

Caveat:
  the makefile automatically copies the system.cfg -- so please do this manually

On the target:

```
make install
systemctl start esp32-wifi
```

When it is not working as "wanted", see the service file for up to date conf.

But essentially, debugging as below should work

The console will print its configuration. Please verify this.

```
$ sudo ./decode /dev/ttyS1 tun3
Open Uart /dev/ttyS1
GPIO: 0

/dev/spidev1.0: spi mode 0x0, 8 bits per word, 48000000 Hz max
Waiting..
 S: 2 <>
 S: 1 <>
 S: 31 <ready to receive configuration>
 S: 1 <>
Waiting..
 S: 14 <Starting Wifi>
Starting Wifi
```
Which indicates all is working..

Troubleshooting :
```
 sudo ./decode /dev/ttyS1 tun3
/sys/class/gpio/gpio117/direction
Fail 38
```
(not seen for a long time...)
There seems to be a problem on the beagle io-setup
Usually running the setup_pin.sh again fixes it..

### RF433 interface

Check the short Makefile!!

Prepare wifi configuration:
UPDATE: configuration is now all in system.cfg (see above)

idf.py menuconfig - check what needs chaning
idf.py build
idf.py flash
--OR--
make HOST=10.1.1.73

The software reads the config from the ESP
It can either (prefered) be installed before  compilation time or installed/updated during runtime using the cmd interface:
```
echo "Store system.cfg $(base64 < .../system.cfg)" | ssh 10.1.1.73 dd of=/dev/ttyS2
```

Once the configuration is installed, the wifi connects automatically and starts the web servers.


Button interface:  http://{esp32-hostname}/index.html

The uart and reset API's  are available if the basic authentication is enabled

The server only starts listening AFTER authentication like:

```curl -u Beaglebone:Beagle1  http://beaglebone1.nispuk.com/basic_auth```

or
```curl -u Beaglebone:Beagle1  http://beaglebone1.nispuk.com/basic_auth?reset```
The latter will also toggle the reset-GPIO.

User/Password currently hard coded in configuration... (TODO)

Success will be greeted with from curl:
```
{"authenticated": true,"user": "Beaglebone"}
```

The remove-uart interface is now also active:

```stty -icanon && nc beaglebone1.nispuk.com 9876 ```

Gives you an interactive console to the uart...

# TODO
  - a lot of html work - looks horrible currently
  - html capture rf433 signal interface
  - merge the tunnel & the web server into a single ESP32 image
  - ??


  




