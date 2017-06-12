EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:ESP8266
LIBS:RfidShelf-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L NodeMCU_1.0_(ESP-12E) U1
U 1 1 58F64AA0
P 2150 2000
F 0 "U1" H 2150 2850 60  0000 C CNN
F 1 "NodeMCU_1.0_(ESP-12E)" H 2150 1150 60  0000 C CNN
F 2 "ESP8266:NodeMCU1.0(12-E)" H 1550 1150 60  0000 C CNN
F 3 "" H 1550 1150 60  0000 C CNN
	1    2150 2000
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR01
U 1 1 58F64FB6
P 3400 3100
F 0 "#PWR01" H 3400 2850 50  0001 C CNN
F 1 "GND" H 3400 2950 50  0000 C CNN
F 2 "" H 3400 3100 50  0001 C CNN
F 3 "" H 3400 3100 50  0001 C CNN
	1    3400 3100
	1    0    0    -1  
$EndComp
$Comp
L CONN_01X08 J1
U 1 1 58F65373
P 5050 1650
F 0 "J1" H 5050 2050 50  0000 C CNN
F 1 "RC522 Header" V 5150 1650 50  0000 C CNN
F 2 "Socket_Strips:Socket_Strip_Angled_1x08_Pitch2.54mm" H 5050 1650 50  0001 C CNN
F 3 "" H 5050 1650 50  0001 C CNN
	1    5050 1650
	1    0    0    -1  
$EndComp
Text Label 3300 2000 2    60   ~ 0
SCLK
Text Label 3300 2100 2    60   ~ 0
MISO
Text Label 3300 2200 2    60   ~ 0
MOSI
Text Label 4600 1400 0    60   ~ 0
SCLK
Text Label 4600 1500 0    60   ~ 0
MOSI
Text Label 4600 1600 0    60   ~ 0
MISO
$Comp
L CONN_01X10 J2
U 1 1 58F66484
P 5100 3250
F 0 "J2" H 5100 3800 50  0000 C CNN
F 1 "GEEETECH_MP3" V 5200 3250 50  0000 C CNN
F 2 "Socket_Strips:Socket_Strip_Angled_1x10_Pitch2.54mm" H 5100 3250 50  0001 C CNN
F 3 "" H 5100 3250 50  0001 C CNN
	1    5100 3250
	1    0    0    -1  
$EndComp
Text Label 4600 3200 0    60   ~ 0
SCLK
Text Label 4600 3300 0    60   ~ 0
MOSI
Text Label 4600 3400 0    60   ~ 0
MISO
Entry Wire Line
	3300 2200 3400 2300
Entry Wire Line
	3300 2100 3400 2200
Entry Wire Line
	3300 2000 3400 2100
Entry Wire Line
	4500 3500 4600 3400
Entry Wire Line
	4500 3400 4600 3300
Entry Wire Line
	4500 3300 4600 3200
Entry Wire Line
	4500 1700 4600 1600
Entry Wire Line
	4500 1600 4600 1500
Entry Wire Line
	4500 1500 4600 1400
Text Label 3300 1600 2    60   ~ 0
XCS
Text Label 4600 1300 0    60   ~ 0
RF_CS
Text Label 3300 2300 2    60   ~ 0
RF_CS
Text Label 3300 1700 2    60   ~ 0
AMP_SLP
Text Label 3300 1500 2    60   ~ 0
SD_CS
Text Label 3300 1400 2    60   ~ 0
DREQ
Text Label 3300 1300 2    60   ~ 0
XDCS
Text Label 4600 2800 0    60   ~ 0
DREQ
Text Label 4600 2900 0    60   ~ 0
XDCS
Text Label 4600 3000 0    60   ~ 0
RST
Text Label 4600 3100 0    60   ~ 0
XCS
Text Label 4600 3500 0    60   ~ 0
SD_CS
$Comp
L +3.3V #PWR02
U 1 1 58F66FB9
P 2950 3250
F 0 "#PWR02" H 2950 3100 50  0001 C CNN
F 1 "+3.3V" H 2950 3390 50  0000 C CNN
F 2 "" H 2950 3250 50  0001 C CNN
F 3 "" H 2950 3250 50  0001 C CNN
	1    2950 3250
	-1   0    0    1   
$EndComp
$Comp
L GND #PWR03
U 1 1 58F6719A
P 4650 2150
F 0 "#PWR03" H 4650 1900 50  0001 C CNN
F 1 "GND" H 4650 2000 50  0000 C CNN
F 2 "" H 4650 2150 50  0001 C CNN
F 3 "" H 4650 2150 50  0001 C CNN
	1    4650 2150
	1    0    0    -1  
$EndComp
$Comp
L +3.3V #PWR04
U 1 1 58F6722B
P 4850 2400
F 0 "#PWR04" H 4850 2250 50  0001 C CNN
F 1 "+3.3V" H 4850 2540 50  0000 C CNN
F 2 "" H 4850 2400 50  0001 C CNN
F 3 "" H 4850 2400 50  0001 C CNN
	1    4850 2400
	-1   0    0    1   
$EndComp
$Comp
L +5V #PWR05
U 1 1 58F672F6
P 650 1300
F 0 "#PWR05" H 650 1150 50  0001 C CNN
F 1 "+5V" H 650 1440 50  0000 C CNN
F 2 "" H 650 1300 50  0001 C CNN
F 3 "" H 650 1300 50  0001 C CNN
	1    650  1300
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR06
U 1 1 58F67344
P 900 1300
F 0 "#PWR06" H 900 1050 50  0001 C CNN
F 1 "GND" H 900 1150 50  0000 C CNN
F 2 "" H 900 1300 50  0001 C CNN
F 3 "" H 900 1300 50  0001 C CNN
	1    900  1300
	-1   0    0    1   
$EndComp
$Comp
L CONN_01X05 J3
U 1 1 58F678E4
P 6450 1400
F 0 "J3" H 6450 1700 50  0000 C CNN
F 1 "Amplifier" V 6550 1400 50  0000 C CNN
F 2 "Socket_Strips:Socket_Strip_Angled_1x05_Pitch2.54mm" H 6450 1400 50  0001 C CNN
F 3 "" H 6450 1400 50  0001 C CNN
	1    6450 1400
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR07
U 1 1 58F67A21
P 5500 1300
F 0 "#PWR07" H 5500 1050 50  0001 C CNN
F 1 "GND" H 5500 1150 50  0000 C CNN
F 2 "" H 5500 1300 50  0001 C CNN
F 3 "" H 5500 1300 50  0001 C CNN
	1    5500 1300
	1    0    0    -1  
$EndComp
$Comp
L +5V #PWR08
U 1 1 58F67A6D
P 5650 1550
F 0 "#PWR08" H 5650 1400 50  0001 C CNN
F 1 "+5V" H 5650 1690 50  0000 C CNN
F 2 "" H 5650 1550 50  0001 C CNN
F 3 "" H 5650 1550 50  0001 C CNN
	1    5650 1550
	-1   0    0    1   
$EndComp
$Comp
L GND #PWR09
U 1 1 58F67ADB
P 5850 1750
F 0 "#PWR09" H 5850 1500 50  0001 C CNN
F 1 "GND" H 5850 1600 50  0000 C CNN
F 2 "" H 5850 1750 50  0001 C CNN
F 3 "" H 5850 1750 50  0001 C CNN
	1    5850 1750
	1    0    0    -1  
$EndComp
Wire Wire Line
	3400 3100 3400 2600
Wire Wire Line
	3400 2600 2950 2600
Wire Wire Line
	2950 2000 3300 2000
Wire Wire Line
	2950 2100 3300 2100
Wire Wire Line
	2950 2200 3300 2200
Wire Wire Line
	4850 1400 4600 1400
Wire Wire Line
	4850 1500 4600 1500
Wire Wire Line
	4850 1600 4600 1600
Wire Wire Line
	4600 3200 4900 3200
Wire Wire Line
	4900 3300 4600 3300
Wire Wire Line
	4900 3400 4600 3400
Wire Bus Line
	3400 1100 3400 2300
Wire Wire Line
	2950 1600 3300 1600
Wire Wire Line
	4850 1300 4600 1300
Wire Wire Line
	2950 2300 3300 2300
Wire Wire Line
	2950 1700 3300 1700
Wire Wire Line
	2950 1500 3300 1500
Wire Wire Line
	2950 1400 3300 1400
Wire Wire Line
	2950 1300 3300 1300
Wire Wire Line
	4900 2800 4600 2800
Wire Wire Line
	4900 2900 4600 2900
Wire Wire Line
	4900 3000 4600 3000
Wire Wire Line
	4900 3100 4600 3100
Wire Wire Line
	4900 3500 4600 3500
Wire Wire Line
	1350 1400 900  1400
Wire Wire Line
	900  1400 900  1300
Wire Bus Line
	3400 1100 4500 1100
Wire Bus Line
	4500 1100 4500 3500
Wire Wire Line
	5500 1300 5500 1200
Wire Wire Line
	5500 1200 6250 1200
Wire Wire Line
	6250 1300 5650 1300
Wire Wire Line
	5650 1300 5650 1550
Wire Wire Line
	5850 1750 5850 1500
Wire Wire Line
	5850 1500 6250 1500
$Comp
L +5V #PWR010
U 1 1 58F7A59A
P 4650 3700
F 0 "#PWR010" H 4650 3550 50  0001 C CNN
F 1 "+5V" H 4650 3840 50  0000 C CNN
F 2 "" H 4650 3700 50  0001 C CNN
F 3 "" H 4650 3700 50  0001 C CNN
	1    4650 3700
	0    -1   -1   0   
$EndComp
$Comp
L GND #PWR011
U 1 1 58F7A5B8
P 4450 3600
F 0 "#PWR011" H 4450 3350 50  0001 C CNN
F 1 "GND" H 4450 3450 50  0000 C CNN
F 2 "" H 4450 3600 50  0001 C CNN
F 3 "" H 4450 3600 50  0001 C CNN
	1    4450 3600
	0    1    1    0   
$EndComp
Wire Wire Line
	2950 2700 2950 3250
Wire Wire Line
	650  1500 1350 1500
Wire Wire Line
	650  1300 650  1500
Wire Wire Line
	4850 1800 4650 1800
Wire Wire Line
	4650 1800 4650 2150
Wire Wire Line
	4850 2000 4850 2400
$Comp
L GND #PWR012
U 1 1 58FA5592
P 3150 1850
F 0 "#PWR012" H 3150 1600 50  0001 C CNN
F 1 "GND" H 3150 1700 50  0000 C CNN
F 2 "" H 3150 1850 50  0001 C CNN
F 3 "" H 3150 1850 50  0001 C CNN
	1    3150 1850
	0    -1   -1   0   
$EndComp
Wire Wire Line
	3150 1850 2950 1850
Wire Wire Line
	2950 1850 2950 1900
$Comp
L GND #PWR013
U 1 1 58FA55E6
P 1150 2600
F 0 "#PWR013" H 1150 2350 50  0001 C CNN
F 1 "GND" H 1150 2450 50  0000 C CNN
F 2 "" H 1150 2600 50  0001 C CNN
F 3 "" H 1150 2600 50  0001 C CNN
	1    1150 2600
	0    1    1    0   
$EndComp
Wire Wire Line
	1350 2600 1150 2600
Wire Wire Line
	1350 2500 1100 2500
Text Label 1100 2500 0    60   ~ 0
RST
Wire Wire Line
	6250 1400 5750 1400
Wire Wire Line
	5750 1400 5750 2200
Text Label 5750 2200 1    60   ~ 0
AMP_SLP
Wire Wire Line
	4900 3600 4450 3600
Wire Wire Line
	4650 3700 4900 3700
Wire Wire Line
	6250 1600 6100 1600
Wire Wire Line
	6100 1600 6100 2200
Text Label 6100 2200 1    60   ~ 0
AUD+
$Comp
L CONN_01X01 J4
U 1 1 591893C6
P 6350 3050
F 0 "J4" H 6350 3150 50  0000 C CNN
F 1 "Audio In" V 6450 3050 50  0000 C CNN
F 2 "Socket_Strips:Socket_Strip_Angled_1x01_Pitch2.54mm" H 6350 3050 50  0001 C CNN
F 3 "" H 6350 3050 50  0001 C CNN
	1    6350 3050
	1    0    0    -1  
$EndComp
Wire Wire Line
	6150 3050 5750 3050
Text Label 5750 3050 0    60   ~ 0
AUD+
$EndSCHEMATC
