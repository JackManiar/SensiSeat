import asyncio
import struct
from bleak import BleakScanner, BleakClient

import streamlit as st
import numpy as np
import matplotlib.pyplot as plt
from math import sin, cos, pi
from matplotlib.patches import Ellipse

POSTURE_SERVICE_UUID = '0000FEED-0000-1000-8000-00805f9b34fb'
POSTURE_CHARACTERISTIC_UUID = '0000F00D-0000-1000-8000-00805f9b34fb'
POSTURE_PACKET_T_FORMAT = '<ffff'

figure_frame = st.empty()

def unpack_posture_packet(data):
    return struct.unpack(POSTURE_PACKET_T_FORMAT,data)

async def main():
    stop_event = asyncio.Event()
    st.set_page_config(
        page_title="SensiSeat App",
        page_icon="🔷"
    )

    def found_callback(device, advertising_data):
        print(f"Device: {device}, Advertising Data: {advertising_data}")

    def received_data_callback(sender, data):
        packet = unpack_posture_packet(data)
        print(f"Received: {packet}")
        fig = create_pressure_plot(packet)
        figure_frame.pyplot(fig)
        plt.close(fig)

    async with BleakScanner(found_callback) as scanner:
        device = await scanner.find_device_by_name("SensiSeat")
        print(f"Found device: {device}\n")
    
    async with BleakClient(device) as client:
        await client.connect()
        print("Connected to device\n")
        print(f"Services: {client.services}")
        try:
            packet_characteristic = client.services.get_service(POSTURE_SERVICE_UUID).get_characteristic(POSTURE_CHARACTERISTIC_UUID)
        except:
            print("Failed to get necessary data. Please relaunch application.")
        else:
            await client.start_notify(
                packet_characteristic,
                received_data_callback
            )
        await stop_event.wait()

def polar(center, r, theta):
    return (center[0] + r*cos(theta), center[1] + r*sin(theta))

def create_pressure_plot(weights):
    total_weight = sum(weights)
    xNormalized = ((weights[1] + weights[3]) - (weights[0] + weights[2])) / total_weight;
    yNormalized = ((weights[0] + weights[1]) - (weights[2] + weights[3])) / total_weight;
    center_of_mass = (xNormalized, yNormalized)
    x_min, x_max = -1.0, 1.0
    y_min, y_max = -1.0, 1.0
    radius=.75
    sensor_radius = .3
    center = (0,0)
    interval = 0.5
    fig, ax = plt.subplots(figsize=(6, 6))
    ax.set_xlim(x_min, x_max)
    ax.set_ylim(y_min, y_max)
    ax.set_xticks(np.arange(x_min, x_max + interval, interval))
    ax.set_yticks(np.arange(y_min, y_max + interval, interval))
    ax.set_aspect("equal")
    ax.grid(True, linestyle="--", color="#d3d3d3", alpha=0.8)

    seat = Ellipse(xy=center, width=radius*2, height=radius*2 * .8, angle=0,
                   facecolor="blue", alpha=0.3, edgecolor="black")
    ax.add_patch(seat)

    tl_sensor = Ellipse(xy=polar(center,radius,3*pi/4), width=sensor_radius,height=sensor_radius,
                        angle=0,facecolor='red',edgecolor='red',alpha=weights[0]/total_weight)
    
    tr_sensor = Ellipse(xy=polar(center,radius,pi/4), width=sensor_radius,height=sensor_radius,
                        angle=0,facecolor='red',edgecolor='red',alpha=weights[1]/total_weight)
    
    bl_sensor = Ellipse(xy=polar(center,radius,5*pi/4), width=sensor_radius,height=sensor_radius,
                        angle=0,facecolor='red',edgecolor='red',alpha=weights[2]/total_weight)
    
    br_sensor = Ellipse(xy=polar(center,radius,7*pi/4), width=sensor_radius,height=sensor_radius,
                        angle=0,facecolor='red',edgecolor='red',alpha=weights[3]/total_weight)
    
    ax.add_patch(tl_sensor); ax.add_patch(tr_sensor); ax.add_patch(bl_sensor); ax.add_patch(br_sensor)


    # temporary posture quality check, can be refined to a more complex condition
    bad_posture = (center_of_mass[0]**2 + center_of_mass[1]**2)**(.5) > .75*radius

    # Draw the center of mass point
    ax.scatter([center_of_mass[0]], [center_of_mass[1]], color="red" if bad_posture else"black", s=100, zorder=5)
    ax.text(center_of_mass[0] + 0.05, center_of_mass[1] + 0.05, "Center of Mass" + ("(UNHEALTHY)" if bad_posture else ""), fontsize=10, color="red" if bad_posture else"black")

    ax.set_xlabel("X position")
    ax.set_ylabel("Y position")
    return fig

if __name__ == "__main__":
    asyncio.run(main())