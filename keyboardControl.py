import msvcrt
import serial

arduino = serial.Serial("COM5", 115200)

print('a,s,d,f,g,h or q:')
input_char = msvcrt.getch()
while input_char.lower() != b'q':
    input_char = msvcrt.getch()
    if input_char.lower() == b'a': 
        arduino.write(b'a')
        print('A')
    elif input_char.lower() == b's': 
        arduino.write(b's')
        print('S')
    elif input_char.lower() == b'd': 
        arduino.write(b'd')
        print('D')
    elif input_char.lower() == b'f': 
        arduino.write(b'f')
        print('F')
    elif input_char.lower() == b'g': 
        arduino.write(b'g')
        print('G')
    elif input_char.lower() == b'h': 
        arduino.write(b'h')
        print('H')
