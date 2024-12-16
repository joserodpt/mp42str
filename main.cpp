#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>

using namespace std;

void decode_ftyp(vector<char> &data) {
    string major_brand(data.begin(), data.begin() + 4);
    cout << "MP4 Major Brand: " << major_brand << "\n";
}

#define DEBUG true

string convert_timestamp_to_date(uint32_t timestamp) {
    time_t raw_time = timestamp - 2082844800; // Adjust for Mac timestamp
    struct tm *time_info;
    char buffer[80];
    time_info = gmtime(&raw_time);
    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", time_info);
    return string(buffer);
}

uint64_t read_uint64_from_bytes(const vector<char>& data, size_t offset) {
    return uint64_t(static_cast<unsigned char>(data[offset]) << 24) |
           (static_cast<unsigned char>(data[offset + 1]) << 16) |
           (static_cast<unsigned char>(data[offset + 2]) << 8) |
           static_cast<unsigned char>(data[offset + 3]);
}

uint32_t unpack_uint32_big_endian(const std::vector<char>& data, size_t start) {
    // Ensure there are enough bytes to unpack
    if (start + 4 > data.size()) {
        throw std::out_of_range("Not enough data to unpack a 32-bit integer.");
    }

    // Unpack the 4 bytes as a big-endian uint32_t
    uint32_t value = 0;
    value |= (static_cast<unsigned char>(data[start]) << 24);
    value |= (static_cast<unsigned char>(data[start + 1]) << 16);
    value |= (static_cast<unsigned char>(data[start + 2]) << 8);
    value |= (static_cast<unsigned char>(data[start + 3]));

    return value;
}

uint64_t read_box(ifstream &file) {
    uint64_t start_pos = file.tellg();

    char header[8];
    file.read(header, 8); // Read the 8-byte header (4 bytes size + 4 bytes type)

    if (file.gcount() < 8) {
        throw runtime_error("End of file reached while reading box header.");
    }

    // first 4 bytes are the size of the box
    uint64_t boxSize = read_uint64_from_bytes(vector<char>(header, header + 4), 0);

    // Handle large box sizes (when size == 1)
    if (boxSize == 1) {
        char extended_size_bytes[8];
        file.read(extended_size_bytes, 8); // Read the 64-bit extended size

        if (file.gcount() < 8) {
            throw runtime_error("End of file reached while reading extended size.");
        }

        // Convert the 8-byte extended size from big-endian to uint64_t
        boxSize = (static_cast<uint64_t>(static_cast<unsigned char>(extended_size_bytes[0])) << 56) |
                   (static_cast<uint64_t>(static_cast<unsigned char>(extended_size_bytes[1])) << 48) |
                   (static_cast<uint64_t>(static_cast<unsigned char>(extended_size_bytes[2])) << 40) |
                   (static_cast<uint64_t>(static_cast<unsigned char>(extended_size_bytes[3])) << 32) |
                   (static_cast<uint64_t>(static_cast<unsigned char>(extended_size_bytes[4])) << 24) |
                   (static_cast<uint64_t>(static_cast<unsigned char>(extended_size_bytes[5])) << 16) |
                   (static_cast<uint64_t>(static_cast<unsigned char>(extended_size_bytes[6])) << 8) |
                   (static_cast<uint64_t>(static_cast<unsigned char>(extended_size_bytes[7])));
    }

    // next 4 bytes are the type of the box
    string type(header + 4, header + 8);

    if (DEBUG) {
        cout << "BOX: " << type << " size: " << boxSize << " @ pos: " << start_pos << "\n";
    }

    if (type == "ftyp" || type == "moov" || type == "mvhd" || type == "meta") {
        // Read the box data
        vector<char> data(boxSize);
        file.read(reinterpret_cast<char *> (data.data()), boxSize);

        if (type == "ftyp") {
            decode_ftyp(data);
        } else if (type == "moov") {
            // Parse the moov box
            file.seekg(start_pos + 8);
            return read_box(file);
        } else if (type == "mvhd") {
            // Extracting data fields
            uint32_t creation_time_raw = unpack_uint32_big_endian(data, 12);
            uint32_t time_scale = unpack_uint32_big_endian(data, 20);
            uint32_t duration = unpack_uint32_big_endian(data, 24);

            // Decode
            cout << "--- MVHD ---" << endl;

            // Convert timestamps to UTC date
            cout << "Creation Time: " << convert_timestamp_to_date(creation_time_raw) << endl;
            cout << "Time Scale: " << time_scale << endl;
            cout << "Duration: " << duration << ", " << static_cast<float>(duration) / time_scale << " seconds" << endl;
        } else if (type == "meta") {

        }
    }

    return boxSize;
}

void parse_mp4_atoms(ifstream &file) {
    uint64_t current_pos = 0;

    while (true) {
        file.seekg(current_pos);
        try {
            current_pos += read_box(file);
        } catch (const exception &ignored) {
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Please provide a video file path as an argument." << endl;
        return 1;
    }

    string video_path = argv[1];
    if (video_path.substr(video_path.find_last_of('.') + 1) != "mp4" &&
        video_path.substr(video_path.find_last_of('.') + 1) != "MP4") {
        cerr << "Please provide a valid MP4 video file path." << endl;
        return 1;
    }

    ifstream file(video_path, ios::binary);
    if (!file.is_open()) {
        cerr << "Failed to open file: " << video_path << endl;
        return 1;
    }

    cout << "Reading video file: " << video_path << endl;
    parse_mp4_atoms(file);
    file.close();
    return 0;
}