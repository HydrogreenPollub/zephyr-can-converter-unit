"""
RS485 test utility for the CAN converter unit.

Encodes and sends MasterFrame messages (protobuf over COBS) to simulate
the TM4C master control unit. Supports all payload types defined in
master.proto.

Functions:
    cobs_encode(data: bytes) -> bytes: Encodes data using COBS.
    create_master_measurements_frame() -> bytes: MasterFrame with MasterMeasurements.
    create_master_status_frame() -> bytes: MasterFrame with MasterStatus.
    create_master_faults_frame() -> bytes: MasterFrame with MasterFaults.
    create_protium_operating_state_frame() -> bytes: MasterFrame with ProtiumOperatingStateMsg.
    create_protium_values_frame() -> bytes: MasterFrame with ProtiumValues.
    send_frame(port, encoded_data) -> None: Transmits COBS-encoded frame.
    run_automated_mode(port) -> None: Continuous transmission at set intervals.
    main() -> None: Interactive menu loop.
"""

import serial
import time
import random
import master_pb2


def cobs_encode(data: bytes) -> bytes:
    """Encodes data using the Consistent Overhead Byte Stuffing (COBS) algorithm."""
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
    """Constructs a MasterFrame with randomized MasterMeasurements."""
    frame = master_pb2.MasterFrame()
    frame.ms_clock_tick_count = int(time.perf_counter() * 1000) % 0xFFFFFFFF
    frame.cycle_clock_tick_count = random.randint(0, 1000)

    meas = master_pb2.MasterMeasurements()
    meas.ai1 = random.uniform(0.0, 3.3)
    meas.supercapacitor_current = random.uniform(0.0, 6.0)
    meas.ai3 = random.uniform(0.0, 3.3)
    meas.ai4 = random.uniform(0.0, 3.3)
    meas.ai5 = random.uniform(0.0, 3.3)
    meas.accel_pedal_voltage = random.uniform(0.0, 5.0)
    meas.brake_pedal_voltage = random.uniform(0.0, 5.0)
    meas.accessory_battery_current = random.uniform(0.0, 5.0)
    meas.hydrogen_high_pressure = random.uniform(0.0, 350.0)
    meas.hydrogen_leakage_sensor_voltage = random.uniform(0.0, 3.3)
    meas.fuel_cell_output_current = random.uniform(0.0, 6.0)
    meas.motor_controller_supply_current = random.uniform(0.0, 6.0)
    meas.accessory_battery_voltage = random.uniform(10.0, 14.0)
    meas.fuel_cell_output_voltage = random.uniform(18.0, 30.0)
    meas.supercapacitor_voltage = random.uniform(35.0, 50.0)
    meas.motor_controller_supply_voltage = random.uniform(35.0, 50.0)
    meas.rpm = random.uniform(0.0, 8000.0)
    meas.speed = random.uniform(0.0, 45.0)

    # Digital inputs
    din = master_pb2.DigitalInputs()
    din.gas_pedal = random.choice([True, False])
    din.start_button = random.choice([True, False])
    din.dead_mans_switch = random.choice([True, False])
    din.emergency_switch = False
    din.reset_button = random.choice([True, False])
    din.calibration_button = False
    din.leakage_detected = False
    din.shell_relay = False
    meas.din.CopyFrom(din)

    frame.master_measurements.CopyFrom(meas)
    return frame.SerializeToString()


def create_master_status_frame() -> bytes:
    """Constructs a MasterFrame with randomized MasterStatus."""
    frame = master_pb2.MasterFrame()
    frame.ms_clock_tick_count = int(time.perf_counter() * 1000) % 0xFFFFFFFF
    frame.cycle_clock_tick_count = random.randint(0, 1000)

    status = master_pb2.MasterStatus()
    status.serial_number = 123456
    status.state = random.choice([
        master_pb2.MasterStatus.INIT,
        master_pb2.MasterStatus.IDLE,
        master_pb2.MasterStatus.STARTING,
        master_pb2.MasterStatus.RUNNING,
        master_pb2.MasterStatus.SHUTTING_DOWN,
        master_pb2.MasterStatus.FAILURE,
    ])
    status.vehicle_type = master_pb2.HYDRA
    status.vehicle_allowed_to_move = status.state == master_pb2.MasterStatus.RUNNING
    status.main_valve_enabled = status.state in (
        master_pb2.MasterStatus.RUNNING,
        master_pb2.MasterStatus.SHUTTING_DOWN,
    )
    status.motor_controller_enabled = status.state == master_pb2.MasterStatus.RUNNING
    status.accel_output_voltage = random.uniform(0.0, 5.0) if status.state == master_pb2.MasterStatus.RUNNING else 0.0
    status.brake_output_voltage = random.uniform(0.0, 5.0) if status.state == master_pb2.MasterStatus.RUNNING else 0.0

    frame.master_status.CopyFrom(status)
    return frame.SerializeToString()


def create_master_faults_frame() -> bytes:
    """Constructs a MasterFrame with randomized MasterFaults."""
    frame = master_pb2.MasterFrame()
    frame.ms_clock_tick_count = int(time.perf_counter() * 1000) % 0xFFFFFFFF
    frame.cycle_clock_tick_count = random.randint(0, 1000)

    faults = master_pb2.MasterFaults()
    # Emergency flags — mostly false in normal operation
    faults.emg_emergency_switch = random.random() < 0.05
    faults.emg_dead_mans_switch = random.random() < 0.05
    faults.emg_leakage_sensor_switch = random.random() < 0.02
    faults.emg_hydrogen_leakage_voltage = random.random() < 0.02
    # Sensor error flags — occasional faults
    faults.err_fuel_cell_output_voltage = random.random() < 0.03
    faults.err_fuel_cell_output_current = random.random() < 0.03
    faults.err_supercapacitor_voltage = random.random() < 0.03
    faults.err_supercapacitor_current = random.random() < 0.03
    faults.err_motor_controller_supply_voltage = random.random() < 0.03
    faults.err_motor_controller_supply_current = random.random() < 0.03
    faults.err_accessory_battery_voltage = random.random() < 0.03
    faults.err_accessory_battery_current = random.random() < 0.03
    faults.err_accel_pedal_voltage = random.random() < 0.03
    faults.err_brake_pedal_voltage = random.random() < 0.03
    faults.err_hydrogen_high_pressure = random.random() < 0.03
    faults.err_speed = random.random() < 0.03

    frame.master_faults.CopyFrom(faults)
    return frame.SerializeToString()


def create_protium_operating_state_frame() -> bytes:
    """Constructs a MasterFrame with a randomized ProtiumOperatingStateMsg."""
    frame = master_pb2.MasterFrame()
    frame.ms_clock_tick_count = int(time.perf_counter() * 1000) % 0xFFFFFFFF
    frame.cycle_clock_tick_count = random.randint(0, 1000)

    protium_msg = master_pb2.ProtiumOperatingStateMsg()
    protium_msg.current_state = random.randint(0, 14)

    entry = protium_msg.log_entries.add()
    entry.ms_clock_tick_count = frame.ms_clock_tick_count - random.randint(100, 500)
    entry.state = random.randint(0, 14)

    frame.protium_operating_state.CopyFrom(protium_msg)
    return frame.SerializeToString()


def create_protium_values_frame() -> bytes:
    """Constructs a MasterFrame with randomized ProtiumValues."""
    frame = master_pb2.MasterFrame()
    frame.ms_clock_tick_count = int(time.perf_counter() * 1000) % 0xFFFFFFFF
    frame.cycle_clock_tick_count = random.randint(0, 1000)

    vals = master_pb2.ProtiumValues()
    vals.fc_v = random.uniform(45.0, 55.0)
    vals.fc_a = random.uniform(0.0, 20.0)
    vals.fc_w = random.uniform(0.0, 500.0)
    vals.energy = random.uniform(100.0, 1000.0)
    vals.fct1 = random.uniform(25.0, 65.0)
    vals.fct2 = random.uniform(25.0, 65.0)
    vals.fan = random.uniform(0.0, 100.0)
    vals.blw = random.uniform(0.0, 100.0)
    vals.h2p1 = random.uniform(0.0, 350.0)
    vals.h2p2 = random.uniform(0.0, 10.0)
    vals.tank_p = random.uniform(0.0, 350.0)
    vals.tank_t = random.uniform(15.0, 40.0)
    vals.v_set = random.uniform(45.0, 55.0)
    vals.i_set = random.uniform(0.0, 20.0)
    vals.ucb_v = random.uniform(0.0, 3.0)
    vals.stasis_selector = random.uniform(0, 3)
    vals.stasis_v1 = random.uniform(0.0, 55.0)
    vals.stasis_v2 = random.uniform(0.0, 55.0)
    vals.number_of_cell = 36.0
    vals.batt_v = random.uniform(11.0, 13.0)
    vals.ip = random.uniform(0.0, 20.0)
    vals.tp = random.uniform(20.0, 60.0)

    frame.protium_values.CopyFrom(vals)
    return frame.SerializeToString()


def send_frame(port: serial.Serial, encoded_data: bytes) -> None:
    """Transmits the COBS-encoded frame over the provided serial port."""
    port.write(encoded_data + b'\x00\x00')


def run_automated_mode(port: serial.Serial) -> None:
    """Executes automated transmission mode, sending frames at predefined intervals."""
    print("Running automated mode. Press Ctrl+C to stop and return to the menu.")

    interval_measurements = 0.025
    interval_status       = 0.100
    interval_faults       = 0.100
    interval_protium_op   = 0.100
    interval_protium_vals = 1.000

    current_time = time.perf_counter()
    next_measurements = current_time + interval_measurements
    next_status       = current_time + interval_status
    next_faults       = current_time + interval_faults
    next_protium_op   = current_time + interval_protium_op
    next_protium_vals = current_time + interval_protium_vals

    try:
        while True:
            current_time = time.perf_counter()

            if current_time >= next_measurements:
                send_frame(port, cobs_encode(create_master_measurements_frame()))
                next_measurements += interval_measurements

            if current_time >= next_status:
                send_frame(port, cobs_encode(create_master_status_frame()))
                next_status += interval_status

            if current_time >= next_faults:
                send_frame(port, cobs_encode(create_master_faults_frame()))
                next_faults += interval_faults

            if current_time >= next_protium_op:
                send_frame(port, cobs_encode(create_protium_operating_state_frame()))
                next_protium_op += interval_protium_op

            if current_time >= next_protium_vals:
                send_frame(port, cobs_encode(create_protium_values_frame()))
                next_protium_vals += interval_protium_vals

            time.sleep(0.001)
    except KeyboardInterrupt:
        print("\nExited automated mode.")


def main() -> None:
    """Main execution loop with interactive menu."""
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
        print("3: Send MasterFaults once")
        print("4: Send ProtiumOperatingState once")
        print("5: Send ProtiumValues once")
        print("a: Run automated mode (continuous)")
        print("q: Quit")

        choice = input("Choice: ")

        if choice == '1':
            send_frame(port, cobs_encode(create_master_measurements_frame()))
            print("MasterMeasurements sent.")
        elif choice == '2':
            send_frame(port, cobs_encode(create_master_status_frame()))
            print("MasterStatus sent.")
        elif choice == '3':
            send_frame(port, cobs_encode(create_master_faults_frame()))
            print("MasterFaults sent.")
        elif choice == '4':
            send_frame(port, cobs_encode(create_protium_operating_state_frame()))
            print("ProtiumOperatingState sent.")
        elif choice == '5':
            send_frame(port, cobs_encode(create_protium_values_frame()))
            print("ProtiumValues sent.")
        elif choice.lower() == 'a':
            run_automated_mode(port)
        elif choice.lower() == 'q':
            break
        else:
            print("Invalid choice, please try again.")


if __name__ == "__main__":
    main()
