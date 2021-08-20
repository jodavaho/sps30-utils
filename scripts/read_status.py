#!/usr/bin/python3
import time
import serial
import binascii

def write_read(out):
    print("(0x)>{}".format(out))
    ser.write(binascii.unhexlify(out))
    time.sleep(0.1)
    returned = bytearray()
    while ser.inWaiting()>0:
        returned.append(ord(ser.read(1)))
    print("  <{}".format(binascii.hexlify(returned)))


# configure the serial connections (the parameters differs on the device you are connecting to)
ser = serial.Serial(
        port='/dev/ttyAMA0',
        baudrate=115200,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS
        )

bDI = '7e00d001002e7e'
bStart= '7e0000020103f97e'
bStop = '7e000100fe7e'
bRead = '7e000300fc7e'
bWakeup = 'ff7e001100ee7e7e001100ee7e'
write_read(bWakeup)
time.sleep(1)
write_read(bDI)
time.sleep(1)
write_read(bStart)
time.sleep(1)
write_read(bRead)
time.sleep(1)
write_read(bStop)
time.sleep(1)
