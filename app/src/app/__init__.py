import random
import string
import time
import serial
import frame as fr
from PIL import Image
import io


class Serial:
	def __init__(self, port, baudrate):
		self.ser = serial.Serial(
			port, baudrate, bytesize=8, timeout=1, stopbits=serial.STOPBITS_ONE
		)
		self.ser.flush()
		self.ser.reset_input_buffer()
		self.ser.reset_output_buffer()

	def send_frame(self, frame):
		self.ser.write(frame)
		self.ser.write(b"\r\n")

	def read_frame(self):
		data = self.ser.readline()
		for byte in data:
			print(hex(byte), end=" ")
		# OSD frame
		# command_specifier, camera_id = fr.parse_ack_osd_frame(data)
		# print(command_specifier, camera_id)
		# for byte in data:
		# 	print(hex(byte), end=" ")

		# Next file frame
		# camera_id, total_package = fr.parse_nextfile_ack_frame(data)

		# print(camera_id, total_package)

	def read_data(self):
		data = bytearray()
		t1 = time.time()
		while True:
			if self.ser.in_waiting > 0:
				data += self.ser.read_all()
				if (time.time() - t1) > 1:
					break
				# fix data length
				if len(data) == 1034:
					break
		camera_id, hour, minute, package_number, package_size, package_data = (
			fr.parse_get_send_data_package(data)
		)
		print(camera_id, hour, minute, package_number, package_size)
		print(len(package_data))
		# print("package data")
		# for byte in package_data:
		# 	print(hex(byte), end=" ")
		return package_data

	def flush(self):
		self.ser.flush()

	def close(self):
		self.ser.close()


def save_image(data):
	image = Image.open(io.BytesIO(data))
	image.save("image.jpg")


if __name__ == "__main__":
	# UART configuration
	uart_port = "/dev/ttyUSB1"
	baudrate = 115200

	# Data to send
	data_osd = bytearray(
		[0x55, 0x4F, 0x01, 0x54, 0x06, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x23]
	)
	# get next file 24/09/11/Video/14h20
	data_get_file = bytearray(
		[0x55, 0x4D, 0x00, 0x18, 0x09, 0x0B, 0x0E, 0x14, 0x01, 0x23]
	)

	data_get_specific_file1 = bytearray([0x55, 0x45, 0x00, 0x0E, 0x14, 0x01, 0x23])
	data_get_specific_file2 = bytearray([0x55, 0x45, 0x00, 0x0E, 0x14, 0x02, 0x23])
	data_get_specific_file3 = bytearray([0x55, 0x45, 0x00, 0x0E, 0x14, 0x03, 0x23])
	ser = Serial(uart_port, baudrate)

	command_next_file = fr.construct_get_next_file_command(
		1, 2024, 9, 17, 13, 54, fr.PackageSize.SIZE_1024
	)

	# while True:
	# 	text = "".join(random.choices(string.ascii_letters, k=random.randint(10, 30)))
	# 	command_osd = fr.construct_osd_command(1, fr.POSITION_TOP, text)
	# 	ser.send_frame(command_osd)
	# 	ser.read_frame()
	# 	ser.flush()
	# 	time.sleep(1)

	# rtc_command = fr.construct_rtc_command(1, 101, 145, 158, 16)
	# ser.send_frame(rtc_command)
	# ser.read_frame()
	# ser.flush()

	# ser.send_frame(command_osd)
	# ser.read_frame()
	# ser.flush()

	# bytes_image = bytearray()
	# for i in range(1, 65):
	# 	command_get_specific_file = fr.construct_get_specified_package_command(
	# 		1, 14, 20, i
	# 	)
	# 	ser.send_frame(command_get_specific_file)
	# 	bytes_image += ser.read_data()
	# 	ser.flush()
	# 	time.sleep(2)
	# save_image(bytes_image)


	ser.send_frame(command_next_file)
	ser.read_frame()
	ser.flush()

	command_get_specific_file1 = fr.construct_get_specified_package_command(
		1, 14, 20, 1
	)
	ser.send_frame(command_get_specific_file1)
	ser.read_frame()
	ser.flush()

	# time.sleep(2)
	# command_get_specific_file2 = fr.construct_get_specified_package_command(
	# 	1, 14, 20, 2
	# )
	# ser.send_frame(command_get_specific_file2)
	# ser.read_data()
	# ser.flush()

	# ser.close()
	# test = bytearray([0x55, 0xFF, 0x01, 0x23])

	# list_file = bytearray([0x55, 0x4C, 0x01, 0x18, 0x09, 0x0B, 0x0E, 0x01, 0x23])

	# ser = serial.Serial(
	# 	uart_port, baudrate, bytesize=8, timeout=1, stopbits=serial.STOPBITS_ONE
	# )

	# ser.write(data_get_file)
	# ser.write(b"\r\n")
	# data = ser.readline()
	# for byte in data:
	# 	print(hex(byte), end=" ")
