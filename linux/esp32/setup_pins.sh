#!/bin/sh
config-pin  p9.28 spi_cs
config-pin  p9.29 spi
config-pin  p9.30 spi
config-pin  p9.31 spi_sclk
config-pin p9.24 uart
config-pin p9.26 uart
echo -n 117 >/sys/class/gpio/export 
echo -n 49 >/sys/class/gpio/export 
