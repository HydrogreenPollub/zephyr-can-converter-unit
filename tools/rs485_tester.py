"""
This module provides utility functions and an interactive prompt to encode and send MasterFrame messages over a serial port.

Functions:
    cobs_encode(data: bytes) -> bytes: Encodes data using the Consistent Overhead Byte Stuffing (COBS) algorithm.
    create_master_measurements_frame() -> bytes: Constructs a serialized MasterFrame containing MasterMeasurements.
    create_master_status_frame() -> bytes: Constructs a serialized MasterFrame containing MasterStatus.
    create_protium_operating_state_frame() -> bytes: Constructs a serialized MasterFrame containing a ProtiumOperatingStateMsg.
    create_protium_values_frame() -> bytes: Constructs a serialized MasterFrame containing ProtiumValues.
    send_frame(port: serial.Serial, encoded_data: bytes) -> None: Transmits the COBS-encoded frame over the provided serial port.
    run_automated_mode(port: serial.Serial) -> None: Executes the automated transmission mode, sending frames at predefined intervals.
    main() -> None: Main execution loop to monitor keyboard input and trigger serial transmission.
"""

import serial
import time
import master_pb2

def cobs_encode(data: bytes) -> bytes:
    """
    Encodes data using the Consistent Overhead Byte Stuffing (COBS) algorithm.

    Args:
        data (bytes): The input byte sequence to encode.

    Returns:
        bytes: The COBS-encoded byte sequence without the trailing zero delimiter.
    """
    encoded = bytearray()
    block_start = 0
    code = 1

    for i in range(len(data)):
        if data[i] == 0:
            encoded.append(code)
            encoded.extend(data[block_start:i])
            block_start = i + 1
            code = 1
        else:
            code += 1
            if code == 0xFF:
                encoded.append(code)
                encoded.extend(data[block_start:i + 1])
                block_start = i + 1
                code = 1

    encoded.append(code)
    encoded.extend(data[block_start:])

    return bytes(encoded)

def create_master_measurements_frame() -> bytes:
    """
    Constructs a serialized MasterFrame containing MasterMeasurements.

    Returns:
        bytes: The serialized Protocol Buffers payload.
    """
    frame = master_pb2.MasterFrame()
    frame.ms_clock_tick_count = 1000
    frame.cycle_clock_tick_count = 10

    meas = master_pb2.MasterMeasurements()
    meas.speed = 45.5
    meas.rpm = 3000.0
    meas.supercapacitor_voltage = 48.2

    frame.master_measurements.CopyFrom(meas)
    return frame.SerializeToString()

def create_master_status_frame() -> bytes:
    """
    Constructs a serialized MasterFrame containing MasterStatus.

    Returns:
        bytes: The serialized Protocol Buffers payload.
    """
    frame = master_pb2.MasterFrame()
    frame.ms_clock_tick_count = 2000
    frame.cycle_clock_tick_count = 20

    status = master_pb2.MasterStatus()
    status.serial_number = 123456
    status.state = master_pb2.MasterStatus.RUNNING

    frame.master_status.CopyFrom(status)
    return frame.SerializeToString()

def create_protium_operating_state_frame() -> bytes:
    """
    Constructs a serialized MasterFrame containing a ProtiumOperatingStateMsg.

    Returns:
        bytes: The serialized Protocol Buffers payload.
    """
    frame = master_pb2.MasterFrame()
    frame.ms_clock_tick_count = 12345
    frame.cycle_clock_tick_count = 678

    protium_msg = master_pb2.ProtiumOperatingStateMsg()
    protium_msg.current_state = master_pb2.RUNNING

    entry = protium_msg.log_entries.add()
    entry.ms_clock_tick_count = 12000
    entry.state = master_pb2.ENTERING_TO_RUNNING_PHASE

    frame.protium_operating_state.CopyFrom(protium_msg)
    return frame.SerializeToString()

def create_protium_values_frame() -> bytes:
    """
    Constructs a serialized MasterFrame containing ProtiumValues.

    Returns:
        bytes: The serialized Protocol Buffers payload.
    """
    frame = master_pb2.MasterFrame()
    frame.ms_clock_tick_count = 4000
    frame.cycle_clock_tick_count = 40

    vals = master_pb2.ProtiumValues()
    vals.fc_v = 50.0
    vals.fc_a = 10.5
    vals.energy = 500.0

    frame.protium_values.CopyFrom(vals)
    return frame.SerializeToString()

def send_frame(port: serial.Serial, encoded_data: bytes) -> None:
    """
    Transmits the COBS-encoded frame over the provided serial port.

    Args:
        port (serial.Serial): The serial port instance to use for transmission.
        encoded_data (bytes): The COBS-encoded payload to transmit.

    Returns:
        None
    """
    port.write(encoded_data + b'\x00')

def run_automated_mode(port: serial.Serial) -> None:
    """
    Executes the automated transmission mode, sending frames at predefined intervals.

    Args:
        port (serial.Serial): The serial port instance to use for transmission.

    Returns:
        None
    """
    print("Running automated mode. Press Ctrl+C to stop and return to the menu.")

    interval_1 = 0.025
    interval_2 = 0.010
    interval_3 = 0.100
    interval_4 = 0.250

    current_time = time.perf_counter()
    next_1 = current_time + interval_1
    next_2 = current_time + interval_2
    next_3 = current_time + interval_3
    next_4 = current_time + interval_4

    try:
        while True:
            current_time = time.perf_counter()

            if current_time >= next_1:
                send_frame(port, cobs_encode(create_master_measurements_frame()))
                next_1 += interval_1

            if current_time >= next_2:
                send_frame(port, cobs_encode(create_master_status_frame()))
                next_2 += interval_2

            if current_time >= next_3:
                send_frame(port, cobs_encode(create_protium_operating_state_frame()))
                next_3 += interval_3

            if current_time >= next_4:
                send_frame(port, cobs_encode(create_protium_values_frame()))
                next_4 += interval_4

            time.sleep(0.001)
    except KeyboardInterrupt:
        print("\nExited automated mode.")

def main() -> None:
    """
    Main execution loop to monitor keyboard input and trigger serial transmission.

    Returns:
        None
    """
    port_name = input("Enter the serial port address [default COM4]: ")
    if not port_name:
        port_name = 'COM4'

    try:
        port = serial.Serial(port_name, 115200)
    except serial.SerialException as e:
        print(f"Failed to open port {port_name}: {e}")
        return

    while True:
        print("\nSelect an option:")
        print("1: Send MasterMeasurements once")
        print("2: Send MasterStatus once")
        print("3: Send ProtiumOperatingState once")
        print("4: Send ProtiumValues once")
        print("a: Run automated mode (continuous)")
        print("q: Quit")

        choice = input("Choice: ")

        if choice == '1':
            send_frame(port, cobs_encode(create_master_measurements_frame()))
            print("Frame 1 sent.")
        elif choice == '2':
            send_frame(port, cobs_encode(create_master_status_frame()))
            print("Frame 2 sent.")
        elif choice == '3':
            send_frame(port, cobs_encode(create_protium_operating_state_frame()))
            print("Frame 3 sent.")
        elif choice == '4':
            send_frame(port, cobs_encode(create_protium_values_frame()))
            print("Frame 4 sent.")
        elif choice.lower() == 'a':
            run_automated_mode(port)
        elif choice.lower() == 'q':
            break
        else:
            print("Invalid choice, please try again.")

if __name__ == "__main__":
    main()