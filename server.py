import ctypes
import pyautogui
import pytesseract
import re
import serial
import sys
import time
import threading

SendInput = ctypes.windll.user32.SendInput
pytesseract.pytesseract.tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract'

keys = {
    'W' : 0x11,
    'A' : 0x1E,
    'S' : 0x1F,
    'D' : 0x20,
    'left' : 0x4B,
    'right' : 0x4D,
    'up' : 0x48,
    'down' : 0x50,
    'space'	: 0x39,
    'enter' : 0x1C
}

# C struct redefinitions 
PUL = ctypes.POINTER(ctypes.c_ulong)
class KeyBdInput(ctypes.Structure):
    _fields_ = [("wVk", ctypes.c_ushort),
                ("wScan", ctypes.c_ushort),
                ("dwFlags", ctypes.c_ulong),
                ("time", ctypes.c_ulong),
                ("dwExtraInfo", PUL)]

class HardwareInput(ctypes.Structure):
    _fields_ = [("uMsg", ctypes.c_ulong),
                ("wParamL", ctypes.c_short),
                ("wParamH", ctypes.c_ushort)]

class MouseInput(ctypes.Structure):
    _fields_ = [("dx", ctypes.c_long),
                ("dy", ctypes.c_long),
                ("mouseData", ctypes.c_ulong),
                ("dwFlags", ctypes.c_ulong),
                ("time",ctypes.c_ulong),
                ("dwExtraInfo", PUL)]

class Input_I(ctypes.Union):
    _fields_ = [("ki", KeyBdInput),
                 ("mi", MouseInput),
                 ("hi", HardwareInput)]

class Input(ctypes.Structure):
    _fields_ = [("type", ctypes.c_ulong),
                ("ii", Input_I)]

def PressKey(hexKeyCode):
    extra = ctypes.c_ulong(0)
    ii_ = Input_I()
    ii_.ki = KeyBdInput( 0, hexKeyCode, 0x0008, 0, ctypes.pointer(extra) )
    x = Input( ctypes.c_ulong(1), ii_ )
    ctypes.windll.user32.SendInput(1, ctypes.pointer(x), ctypes.sizeof(x))

def ReleaseKey(hexKeyCode):
    extra = ctypes.c_ulong(0)
    ii_ = Input_I()
    ii_.ki = KeyBdInput( 0, hexKeyCode, 0x0008 | 0x0002, 0, ctypes.pointer(extra) )
    x = Input( ctypes.c_ulong(1), ii_ )
    ctypes.windll.user32.SendInput(1, ctypes.pointer(x), ctypes.sizeof(x))

def SpeedometerThread():
    size = pyautogui.size()
    print("Started speedometer thread, screen size is " + str(size.width) + "x" + str(size.height) + " px")

    while True:
        if (speedometerActive):
            speed = re.sub(r"\D", "", pytesseract.image_to_string(pyautogui.screenshot(region=(size.width - 0.15 * size.width, 0.02 * size.height, 0.13 * size.width, 0.1 * size.height)), config='--psm 8 --oem 3 -c tessedit_char_whitelist=0123456789'))
            if (len(speed) > 0):
                ser.write(int(speed).to_bytes(2, byteorder='big'))

# Start Main Code
if len(sys.argv) != 2:
    sys.exit("Usage: " + str(sys.argv[0]) + " <COM port (e.g. \"COM1\")>")
elif not re.search(r"COM\d+", sys.argv[1]):
    sys.exit("Usage: " + str(sys.argv[0]) + " <COM port (e.g. \"COM1\")>")

ser = serial.Serial(
    port=sys.argv[1],\
    baudrate=9600,\
    parity=serial.PARITY_NONE,\
    stopbits=serial.STOPBITS_ONE,\
    bytesize=serial.EIGHTBITS,\
    timeout=0)

print("connected to: " + ser.portstr)

direction = "left"
rate = 0
counter = 0
pressed = False
speedometerActive = False

speedometer = threading.Thread(target=SpeedometerThread)
speedometer.start()

while True:
    buffer = ser.read()

    if (len(buffer) > 0):
        command = int.from_bytes(buffer, "big")
        print("Received command: " + str(command))

        if (command == 16):
            PressKey(keys['up'])
        elif (command == 17):
            ReleaseKey(keys['up'])
        elif (command == 18):
            PressKey(keys['left'])
        elif (command == 19):
            ReleaseKey(keys['left'])
        elif (command == 20):
            PressKey(keys['right'])
        elif (command == 21):
            ReleaseKey(keys['right'])
        elif (command == 22):
            PressKey(keys['down'])
        elif (command == 23):
            ReleaseKey(keys['down'])
        elif (command == 24):
            PressKey(keys['space'])
        elif (command == 25):
            ReleaseKey(keys['space'])
        elif (command == 26):
            PressKey(keys['enter'])
        elif (command == 27):
            ReleaseKey(keys['enter'])
        elif (command == 28):
            # Release All
            for key in keys:
                ReleaseKey(keys[key])
        elif (command == 29):
            speedometerActive = True
        elif (command == 30):
            speedometerActive = False
        elif (command > 7):
            direction = "left"
            rate = command - 8
        elif (command < 8):
            direction = "right"
            rate = 7 - command
        else:
            print("Received invalid command: " + str(command))

    if (counter % 5 < rate):
        if (pressed == False):
            PressKey(keys[direction])
            pressed = True
    else:
        if (pressed == True):
            ReleaseKey(keys[direction])
            pressed = False

    time.sleep(.04)

    counter += 1

# Theoretically, this should never happen
ser.close()