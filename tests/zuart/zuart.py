#!/usr/bin/python3

import sys
import re
import time
import serial
import logging

SERIAL_PORT = 'ttyS1'
SERIAL_BAUDRATE = 115200

if len( sys.argv ) == 2:
    SERIAL_PORT = sys.argv[1]

ser = serial.Serial()

ser.port=SERIAL_PORT
ser.baudrate=SERIAL_BAUDRATE

ser.open();

while 1:
    line = ser.readline().decode( 'ascii' ).replace( '\n', '' );
    argv = re.split( r"\s+", line )

    print( "$" + argv[ 0 ].upper() + ' ' + ' '.join( argv[1:] ) )

    if argv[ 0 ] == "ping":
        ser.write( 1 )
    elif argv[ 0 ] == "flood":
        size = int( argv[ 1 ] )
        ser.write( bytearray( size ) )
    elif argv[ 0 ] == "close":
        ser.close()
        break
