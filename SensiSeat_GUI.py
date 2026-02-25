import serial
import matplotlib.pyplot as plt

# CHANGE THIS to your Arduino COM port
ser = serial.Serial('COM3', 9600)

plt.ion()
fig, ax = plt.subplots()

x_vals = []
y_vals = []

while True:
    try:
        line = ser.readline().decode().strip()
        x, y = map(float, line.split(','))

        x_vals.append(x)
        y_vals.append(y)

        ax.clear()
        ax.set_xlim(-1, 1)
        ax.set_ylim(-1, 1)
        ax.set_xlabel("Left  <---->  Right")
        ax.set_ylabel("Back  <---->  Front")
        ax.scatter(x, y)

        plt.pause(0.01)

    except:
        pass