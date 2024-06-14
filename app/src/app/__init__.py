import time
import serial


def send_data_to_serial(port, baudrate, data):
    # Open serial connection
    with serial.Serial(port, baudrate, timeout=1) as ser:
        # Write data to serial port
        ser.write(data)
        ser.write(b"\n")
        print(f"Data {data} sent to {port} at {baudrate} baud.")
        # read response
        response = ser.readline()
        # print bytes received
        print(f"Response: {response}")
        for i in response:
            print(f"Byte: {i}")


if __name__ == "__main__":
    # UART configuration
    uart_port = "/dev/ttyUSB1"  # Change this to your UART port
    baudrate = 115200

    # Data to send
    data_osd = bytearray(
        [
            0x55,
            0x4F,
            0x00,
            0x54,
            0x06,
            0x65,
            0x65,
            0x65,
            0x65,
            0x65,
            0x65,
            0x23,
        ]
    )
    data_get_file = bytearray(
        [
            0x55,
            0x4D,
            0x00,
            0x18,
            0x05,
            0x19,
            0x0B,
            0x29,
            0x01,
            0x23,
        ]
    )
    send_data_to_serial(uart_port, baudrate, data_osd)

    # send data every 1 second
    # while True:
    #     send_data_to_serial(uart_port, baudrate, data)
    #     time.sleep(1)
