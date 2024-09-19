from enum import Enum

ack = bytearray([0x55, 0x4D, 0x00, 0x83, 0x23])
nack = bytearray([0x55, 0x4D, 0x00, 0x84, 0x23])

DATA_HEADER = b"\x55"  # Data Header byte
END_MARK_BYTE = b"\x23"  # End Mark byte
MAX_PACKAGE_DATA_SIZE = 1024  # Maximum size of package data field
LIST_FILES_COMMAND = b"\x4c"
GET_NEXT_FILE_COMMAND = b"\x4d"
GET_SPEC_FILE_COMMAND = b"\x45"
SEND_SPEC_FILE_COMMAND = b"\x46"
BAUDRATE_COMMAND = b"\x49"
OSD_COMMAND = b"\x4f"
RTC_COMMAND = b"\x54"
POSITION_TOP = b"\x54"
POSITION_BOTTOM = b"\x42"
MAX_PACKAGE_SIZE = 1024


class PackageSize(Enum):
	SIZE_512 = 0
	SIZE_1024 = 1
	SIZE_2048 = 2


def calculate_checksum(header, cmd, id, package, length, data):
	result = header + cmd + id + package + length
	for i in data:
		result += i
	# return last 2 bytes
	return result & 0xFFFF


# Function to construct a frame with the new data structure
def construct_data_frame(command_specifier, camera_id, package_number, package_data):
	if len(package_data) > MAX_PACKAGE_DATA_SIZE:
		raise ValueError(
			f"Package data size exceeds maximum allowed size of {MAX_PACKAGE_DATA_SIZE} bytes"
		)

	package_size = len(package_data).to_bytes(
		2, byteorder="big"
	)  # Package size is 2 bytes
	package_number = package_number.to_bytes(
		2, byteorder="big"
	)  # Package number is 2 bytes

	# Start with header, command specifier, and camera ID
	frame = DATA_HEADER + bytes([command_specifier]) + bytes([camera_id])

	# Append the package number, package size, and the actual package data
	frame += package_number + package_size + package_data

	# Calculate the checksum (excluding checksum itself)
	checksum = calculate_checksum(frame)

	# Append checksum to frame
	frame += checksum

	return frame


# Function to parse a frame and validate checksum
def parse_data_frame(frame):
	if (
		len(frame) < 8
	):  # Minimum size: header (1) + command (1) + ID (1) + pkg no (2) + pkg size (2) + checksum (2)
		raise ValueError("Frame is too short")

	# Extract the checksum from the frame
	received_checksum = frame[-2:]  # Last 2 bytes are the checksum
	frame_without_checksum = frame[:-2]  # Exclude checksum for calculation

	# Calculate checksum on the frame (excluding the received checksum)
	calculated_checksum = calculate_checksum(frame_without_checksum)

	# Validate checksum
	if received_checksum != calculated_checksum:
		raise ValueError("Checksum does not match, frame might be corrupted")

	# Parse the components
	data_header = frame[0]  # Data Header (1 byte)
	command_specifier = frame[1]  # Command Specifier (1 byte)
	camera_id = frame[2]  # Camera ID (1 byte)

	# Extract the package number (2 bytes) and package size (2 bytes)
	package_number = int.from_bytes(frame[3:5], byteorder="big")
	package_size = int.from_bytes(frame[5:7], byteorder="big")

	# Extract the actual package data
	package_data = frame[7:-2]  # Package data (variable size, excluding checksum)

	if len(package_data) != package_size:
		raise ValueError("Package size does not match the actual data size")

	return (
		data_header,
		command_specifier,
		camera_id,
		package_number,
		package_size,
		package_data,
	)


# Function to construct a frame for "List Files" command
def construct_list_files_frame(camera_id, year, month, day, hour, package_size):
	if package_size > MAX_PACKAGE_SIZE:
		raise ValueError(
			f"Package size exceeds maximum allowed size of {MAX_PACKAGE_SIZE} bytes"
		)

	# Convert date to bytes (year in 2 bytes, month and day in 1 byte each)
	year_bytes = year.to_bytes(2, byteorder="big")
	month_byte = bytes([month])
	day_byte = bytes([day])

	# Hour (1 byte)
	hour_byte = bytes([hour])

	# Package size (2 bytes)
	package_size_bytes = package_size.to_bytes(2, byteorder="big")

	# Start constructing the frame
	frame = DATA_HEADER + LIST_FILES_COMMAND + bytes([camera_id])

	# Append the date, hour, and package size to the command content
	frame += year_bytes + month_byte + day_byte + hour_byte + package_size_bytes

	# Append the end mark
	frame += END_MARK_BYTE

	return frame


# Function to parse the ACK frame
def parse_list_file_ack_frame(frame):
	if len(frame) != 5:  # Fixed size of 5 bytes for ACK frame
		raise ValueError("ACK Frame size is incorrect")

	data_header = frame[0]
	command_specifier = frame[1]
	camera_id = frame[2]
	files_count = frame[3]
	end_mark = frame[4]

	if data_header != DATA_HEADER[0] or end_mark != END_MARK_BYTE[0]:
		raise ValueError("Invalid frame format")

	return data_header, command_specifier, camera_id, files_count


# Function to construct the "Get Next File by File" command frame
def construct_get_next_file_command(
	camera_id, year, month, day, hour, minute, package_size_data
):
	if not isinstance(package_size_data, PackageSize):
		raise ValueError("Invalid package size data. Must be 00, 01, or 02.")

	# Date is packed into 3 bytes (Year, Month, Day)
	date = bytes([year - 2000]) + bytes([month]) + bytes([day])  # Year offset for 2000+

	# Time is packed into 2 bytes (Hour and Minute)
	time = bytes([hour]) + bytes([minute])

	# Package size data is 1 byte
	package_size_data_byte = bytes([package_size_data.value])

	# Start with Data Header, Command Specifier, Camera ID, then Command Content
	frame = DATA_HEADER + GET_NEXT_FILE_COMMAND + bytes([camera_id])
	frame += date + time + package_size_data_byte
	frame += END_MARK_BYTE  # End mark

	return frame


# Function to parse the ACK frame
def parse_nextfile_ack_frame(frame):
	if len(frame) != 5:  # Fixed size of 5 bytes for ACK frame
		raise ValueError("ACK Frame size is incorrect")

	data_header = frame[0]
	command_specifier = frame[1]
	camera_id = frame[2]
	total_package = frame[3]
	end_mark = frame[4]

	if data_header != DATA_HEADER[0] or end_mark != END_MARK_BYTE[0]:
		raise ValueError("Invalid frame format")

	return camera_id, total_package


# Function to construct the OSD command frame
def construct_osd_command(camera_id, position, text_value):
	if position not in [POSITION_TOP, POSITION_BOTTOM]:
		raise ValueError("Invalid position. Must be 0x54 or 0x42.")

	length = len(text_value)  # Length of the text value
	if length > 255:
		raise ValueError("Text value too long. Maximum length is 255 bytes.")

	# Construct frame: Data Header, Command Specifier, Camera ID, Position, Length, Text Value, End Mark
	frame = DATA_HEADER + OSD_COMMAND + bytes([camera_id])
	frame += position + bytes([length]) + text_value.encode()  # Encoding text value
	frame += END_MARK_BYTE  # End mark

	return frame


# Function to parse the ACK frame
def parse_ack_osd_frame(frame):
	if len(frame) != 4:  # ACK frame size is fixed to 4 bytes
		raise ValueError("ACK Frame size is incorrect")

	data_header = frame[0]
	command_specifier = frame[1]
	camera_id = frame[2]
	end_mark = frame[3]

	if data_header != DATA_HEADER[0] or end_mark != END_MARK_BYTE[0]:
		raise ValueError("Invalid frame format")

	return command_specifier, camera_id


# Function to parse the ACK frame
def parse_ack_spec_frame(frame):
	if len(frame) != 4:  # Fixed size of 4 bytes for ACK frame
		raise ValueError("ACK Frame size is incorrect")

	data_header = frame[0]
	command_specifier = frame[1]
	camera_id = frame[2]
	end_mark = frame[3]

	if data_header != DATA_HEADER[0] or end_mark != END_MARK_BYTE[0]:
		raise ValueError("Invalid frame format")

	return data_header, command_specifier, camera_id


# Function to construct the "Get Specified Package" command frame
def construct_get_specified_package_command(camera_id, hour, minute, package_number):
	if not (0 <= hour <= 23):
		raise ValueError("Hour must be between 0 and 23")
	if not (0 <= minute <= 59):
		raise ValueError("Minute must be between 0 and 59")
	if not (0 <= package_number <= 255):
		raise ValueError("Package number must be between 0 and 255")

	# Command content: HH (hour) + MM (minute) + PK (package number)
	command_content = bytes([hour, minute, package_number])

	# Construct frame: Data Header, Command Specifier, Camera ID, Command Content, End Mark
	frame = DATA_HEADER + GET_SPEC_FILE_COMMAND + bytes([camera_id])
	frame += command_content + END_MARK_BYTE  # End mark

	return frame


# Function to parse the "Send Specified Data Package" frame
def parse_get_send_data_package(frame):
	# if (
	# 	len(frame) < 7
	# ):  # Minimum size: 1 byte header + 1 byte specifier + 1 byte camera ID + 3 bytes command content + 2 bytes checksum
	# 	raise ValueError("Frame size is incorrect")

	data_header = frame[0]
	command_specifier = frame[1]
	camera_id = frame[2]

	# Extract Command Content
	command_content = frame[3:-2]  # Excluding checksum
	checksum_received = frame[-2:]

	# Extract Package Number (3 bytes: HH, MM, PK) + Package Size (1 byte) + Package Data (up to 1024 bytes)
	hour = command_content[0]
	minute = command_content[1]
	package_number = command_content[2]
	package_size = command_content[3]
	package_data = command_content[4:-1]

	# Recalculate checksum to verify
	# checksum_calculated = calculate_checksum(
	# 	data_header, command_specifier, camera_id, command_content[:-2]
	# )
	# if checksum_received != checksum_calculated:
	# 	raise ValueError("Checksum does not match")

	return (
		camera_id,
		hour,
		minute,
		package_number,
		package_size,
		package_data,
	)


# Function to construct the OSD command frame
def construct_rtc_command(camera_id, byte1, byte2, byte3, byte4):
	frame = DATA_HEADER + RTC_COMMAND + bytes([camera_id])
	frame += bytes([byte1, byte2, byte3, byte4])
	frame += END_MARK_BYTE  # End mark

	return frame


# Function to parse the ACK frame
def parse_ack_rtc_frame(frame):
	if len(frame) != 4:  # ACK frame size is fixed to 4 bytes
		raise ValueError("ACK Frame size is incorrect")

	data_header = frame[0]
	command_specifier = frame[1]
	camera_id = frame[2]
	end_mark = frame[3]

	if data_header != DATA_HEADER[0] or end_mark != END_MARK_BYTE[0]:
		raise ValueError("Invalid frame format")

	return command_specifier, camera_id
