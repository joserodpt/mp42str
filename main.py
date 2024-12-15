import struct
import sys
from datetime import datetime, timedelta
import xml.etree.ElementTree as ET


def format_seconds(seconds):
    """
    Format a floating point number (seconds) into the SRT time format: HH:MM:SS,SSS.

    Parameters:
    - seconds (float): The total number of seconds, including the fractional part (milliseconds).

    Returns:
    - str: The formatted time in 'HH:MM:SS,SSS' format.
    """
    even = int(seconds)
    decimal = seconds - even

    milliseconds = int(decimal * 1000)

    hours = even // 3600
    minutes = (even % 3600) // 60
    seconds = even % 60

    return f"{hours:02d}:{minutes:02d}:{seconds:02d},{milliseconds:03d}"


def write_dates_to_srt(file_path, dates_list):
    """Write the list of dates as tuples (day, month, year, hour, min, sec) to an SRT file with the given offset."""

    srt_file_path = file_path.replace(".MP4", ".srt").replace(".mp4", ".srt")

    print(f'Writing timecodes to SRT file... {srt_file_path}')

    with open(srt_file_path, 'w') as f:
        previous_time = 0

        for i, date_tuple in enumerate(dates_list):
            # Extract the date and time values from the tuple
            day, month, year, hour, minute, second = date_tuple

            # Write the subtitle index, time range, and text
            f.write(f"{i + 1}\n")
            f.write(f"{format_seconds(previous_time)} --> {format_seconds(previous_time + 1)}\n")
            f.write(f"{day:02}/{month:02}/{year}\n")
            f.write(f"{hour:02}:{minute:02}:{second:02}\n")
            f.write("\n")

            # Update previous time
            previous_time += 1  # Increment by 1 second


def read_box(f):
    """Reads an MP4 box, which contains the size and type of the box."""
    header = f.read(8)  # Each box header is 8 bytes (4 bytes size + 4 bytes type)

    if len(header) < 8:
        return None

    try:
        # Unpack size and box type (big-endian 32-bit integer and 4 ASCII characters)
        size, box_type = struct.unpack('>I4s', header)

        # Handle extended size (if size == 1, the real size is in the next 8 bytes)
        if size == 1:
            size = struct.unpack('>Q', f.read(8))[0]  # Read 64-bit extended size

        box_type = box_type.decode('utf-8', errors='ignore').strip()

        return {
            'size': size,
            'type': box_type,
            'start_pos': f.tell() - 8
        }
    except Exception as e:
        print(f"Error parsing box: {e}")
        return None


def decode_ftyp(ftyp_data):
    """Decodes the ftyp box and prints the major brand, minor version, and compatible brands."""
    major_brand = ftyp_data[:4].decode('utf-8')
    minor_version = struct.unpack('>I', ftyp_data[4:8])[0]
    compatible_brands = []

    # Each compatible brand is 4 bytes long
    for i in range(8, len(ftyp_data), 4):
        compatible_brands.append(ftyp_data[i:i + 4].decode('utf-8'))

    print(f"MP4 Major Brand: {major_brand}")


def decode_mvhd(mvhd_data):
    """
    type: mvhd_data[4:8]
    version: mvhd_data[8:9]
    flags: mvhd_data[9:12]
    creation_time: mvhd_data[12:16]
    modification_time: mvhd_data[16:20]
    time_scale: mvhd_data[20:24]
    duration: mvhd_data[24:28]
    preferred_rate: mvhd_data[28:32]
    preferred_volume: mvhd_data[32:34]
    reserved: mvhd_data[34:44]
    matrix: mvhd_data[44:80]
    predefines: mvhd_data[80:104]
    next_track_id: mvhd_data[104:108]

    Type—A 32-bit unsigned integer that identifies the type, represented as a four-character code; this
    field must be set to 'mvhd'.
    - Version—A 1-byte specification of the version of this Movie Header atom; set here to '0x00'.
    - Flags—Three bytes of space for future movie header flags; set here to '0x000000'
    .
    - Creation Time—A 32-bit integer that specifies the calendar date and time (in seconds since
    midnight, January 1, 1904) when the movie atom was created in coordinated universal time
    (UTC); set here to '0xCCF85C09'.
    - Modification Time—A 32-bit integer that specifies the calendar date and time (in seconds since
    midnight, January 1, 1904) when the movie atom was created in coordinated universal time
    (UTC); set here to '0xCCF85C09'.
    - Time Scale—A time value that indicates the time scale for this movie—that is, the number of time
    units that pass per second in its time coordinate system; set here to '0x000003E8'.
    - Duration—A time value that indicates the duration of the movie in time scale units, derived from
    the movie’s tracks, corresponding to the duration of the longest track in the movie; set here to
    '0x00057BC0'.
    - Preferred Rate— A 32-bit fixed-point number that specifies the rate at which to play this movie (a
    value of 1.0 indicates normal rate); set here to '0x00010000'.
    - Preferred Volume—A 16-bit fixed-point number that specifies how loud to play this movie’s sound
    (a value of 1.0 indicates full volume); set here to '0x0100'.
    - Reserved—Ten reserved bytes set to zero.
    - Matrix—A transformation matrix that defines how to map points from one coordinate space into
    another coordinate space (please reference to the QuickTime File Format Specification for details).
    - Predefines—Media Header predefines; set to zero (please refer to the QuickTime File Format
    Specification for details).
    - Next Track ID—The number of the Next Track ID; set here to '3'

    Sample output:
    --- MVHD ---
    Size: 108
    Type: mvhd
    Version: 0
    Flags: (0, 0, 0)
    Creation Time: 2024-12-13 23:11:12
    Modification Time: 2024-12-13 23:11:12
    Time Scale: 50000
    Duration: 576000, 11.52 seconds
    Preferred Rate: 65536
    Preferred Volume: 256
    Reserved: (0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    Matrix: (0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0)
    Predefines: (0, 0, 0, 0, 0, 0)
    Next Track ID: 4
    """

    # Check if mvhd_data is the correct length
    if len(mvhd_data) < 108:
        print(f"Error: mvhd data is too short. Expected at least 108 bytes, got {len(mvhd_data)} bytes.")
        return

        # creation time
    creation_time_long = struct.unpack('>I', mvhd_data[12:16])[0]

    # convert to human readable format
    creation_time = datetime.utcfromtimestamp(creation_time_long - 2082844800)
    print(f"First Timecode: {creation_time.strftime('%d-%m-%Y %H:%M:%S')}")

    # time scale
    time_scale = struct.unpack('>I', mvhd_data[20:24])[0]
    # duration
    duration = struct.unpack('>I', mvhd_data[24:28])[0]
    seconds = int(round(duration / time_scale, 0))
    print(f"File duration: {seconds} seconds")

    dates = []
    for i in range(0, seconds):
        # add seconds to creation time
        creation_time = creation_time + timedelta(seconds=1)

        dates.append((creation_time.day, creation_time.month, creation_time.year, creation_time.hour,
                      creation_time.minute, creation_time.second))

    return dates


def decode_meta(meta_data):
    """Decodes the meta box and prints relevant details.

    size: meta_data[0:4]
    type: meta_data[4:8]
    version: meta_data[8:9]
    flags: meta_data[9:12]
    childs....

    """
    # size
    # size = struct.unpack('>I', meta_data[0:4])
    # print(f"Size: {size}")

    # type
    # type = meta_data[4:8].decode('utf-8')
    # print(f"Type: {type}")

    # version
    # version = meta_data[8:9]
    # print(f"Version: {version}")

    # flags
    # flags = meta_data[9:12]
    # print(f"Flags: {flags}")

    read_data = 12
    while read_data < len(meta_data):
        # box size
        box_size = struct.unpack('>I', meta_data[read_data:read_data + 4])[0]
        # box type
        box_type = meta_data[read_data + 4:read_data + 8].decode('utf-8')
        if "xml" in box_type:
            #sony cameras have aditional data in xml format, good fields: creationdate, device
            print("This file contains aditional data in XML:")
            cleanString = meta_data[read_data + 8:read_data + box_size].decode('utf-8')

            # remove null characters
            cleanString = cleanString.replace('\x00', '')
            #print(cleanString)

            root = ET.fromstring(cleanString)
            for child in root:
                key = child.tag.split('}')[1].lower()
                if key == "creationdate":
                    print("Creation Date:", child.attrib['value'])
                if key == "device":
                    #make one string with keys and values of child.attrib
                    print("Device: ")
                    for a, b in child.attrib.items():
                        print(a, b)

        read_data += box_size


def parse_box(f, parent_box_size=None, level=0):
    """Parses an MP4 box and its child boxes."""
    while True:
        current_pos = f.tell()

        if parent_box_size and current_pos >= parent_box_size:
            break

        box = read_box(f)
        if not box:
            break

        box_type = box['type']
        box_size = box['size']
        box_start = box['start_pos']

        # indent = '  ' * level
        # print(f"> {indent}Box Type: {box_type}, Size: {box_size}, Start: {box_start}")

        # Check if the box size is valid
        if box_size < 8:
            # print(f"{indent}Invalid box size ({box_size}). Skipping the box.")
            break

        # If the box is of type 'ftyp', decode it
        if box_type == 'ftyp':
            ftyp_data = f.read(box_size - 8)  # Read the user data in the 'ftyp' box
            decode_ftyp(ftyp_data)

        # If the box contains sub-boxes (like moov, trak, etc.), recursively parse them
        if box_type in ['moov']:
            # Subtract 8 bytes for the header, go into the sub-boxes
            parse_box(f, current_pos + box_size, level + 1)
        else:
            # Seek to the end of the box (skip over data)
            f.seek(current_pos + box_size)

        # Parse mvhd box
        if box_type == 'mvhd':
            f.seek(box_start)
            dates = decode_mvhd(f.read(box_size))
            if dates:
                write_dates_to_srt(f.name, dates)
            return

        if box_type == 'meta':
            f.seek(box_start)
            decode_meta(f.read(box_size))
            return


def parse(file_path):
    """Start parsing the MP4 file from the top level."""
    with open(file_path, 'rb') as f:
        parse_box(f)


if __name__ == "__main__":
    video_path = sys.argv[1]
    if not video_path:
        print("Please provide a video file path as an argument.")
        sys.exit(1)

    if not video_path.endswith(".mp4") and not video_path.endswith(".MP4"):
        print("Please provide a valid MP4 video file path.")
        sys.exit(1)

    print(f"Reading video file: {video_path}")
    parse(video_path)