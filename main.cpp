#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <iomanip>
#include <stdexcept>

/*
 *
 *   __  __ _____  _  _ ___      _
 *  |  \/  |  __ \| || |__ \    | |
 *  | \  / | |__) | || |_ ) |___| |_ _ __
 *  | |\/| |  ___/|__   _/ // __| __| '__|
 *  | |  | | |       | |/ /_\__ \ |_| |
 *  |_|  |_|_|       |_|____|___/\__|_|
 *
 *  C++ Utility program to extract timecode to .str files from mp4's
 *  that are compliant with the ISO Base Media File Format
 *  (ISO/IEC 14496-12 - MPEG-4 Part 12) and that have:
 *  - "moov" atom/box, "mvhd" atom/box
 *  It is also capable of extracting additional data in XML format from:
 *  - "meta" atom/box, "xml " atom/box
 *
 *  Syntax: mp42str <video_file_path> [options]
 *  Options:
 *  -xml: Extract additional data in XML format only
 *  -debug: Print debug information
 *
 *  This program is licensed under the MIT License.
 *  (c) José Rodrigues 2024
 *
 */

using namespace std;

bool DEBUG, XMLonly;

string convert_timestamp_to_date(uint32_t timestamp) {
    time_t raw_time = timestamp - 2082844800; // Adjust for Mac timestamp
    struct tm *time_info;
    char buffer[80];
    time_info = gmtime(&raw_time);
    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", time_info);
    return string(buffer);
}

uint64_t read_uint64_from_bytes(const vector<char> &data, size_t offset) {
    if (offset + 4 > data.size()) {
        throw std::out_of_range("Not enough data to unpack a 32-bit integer.");
    }

    return uint64_t(static_cast<unsigned char>(data[offset]) << 24) |
           (static_cast<unsigned char>(data[offset + 1]) << 16) |
           (static_cast<unsigned char>(data[offset + 2]) << 8) |
           static_cast<unsigned char>(data[offset + 3]);
}

string extract_string(const vector<char> &data, size_t start, size_t length) {
    if (start + length > data.size()) {
        throw out_of_range("Not enough data to extract string.");
    }
    return string(data.begin() + start, data.begin() + start + length);
}

void parse_meta(vector<char> &data) {
    /*
                size: meta_data[0:4]
                type: meta_data[4:8]
                version: meta_data[8:9]
                flags: meta_data[9:12]
                */

    uint64_t meta_pos = 4;

    while (meta_pos < data.size()) {
        // Extract box size (4 bytes)
        uint32_t box_size = read_uint64_from_bytes(data, meta_pos);
        if (box_size == 0) { //box size cannot be 0
            break;
        }

        // Extract box type (4 bytes)
        string box_type = extract_string(data, meta_pos + 4, 4);

        if (DEBUG)
            cout << "Box Type: " << box_type << ", Size: " << box_size << endl;

        if (box_type == "xml ") {
            if (!XMLonly) {
                cout << "This file contains additional data in XML." << endl;
            } else {
                vector<char> xml_data(data.begin() + meta_pos + 12, data.begin() + meta_pos + box_size - 1);
                string xmlSTR = string(xml_data.begin(), xml_data.end());
                xmlSTR.erase(std::remove(xmlSTR.begin(), xmlSTR.end(), '\x00'), xmlSTR.end());

                cout << string(xml_data.begin(), xml_data.end()) << endl;
            }
        }

        // Move to the next box
        meta_pos += box_size;
    }
}

uint64_t read_box(ifstream &file, uint64_t current_pos) {
    file.seekg(current_pos);
    char header[8];
    file.read(header, 8); // Read the 8-byte header (4 bytes size + 4 bytes type)

    if (file.gcount() < 8) {
        throw runtime_error("End of file reached while reading box header.");
    }

    // first 4 bytes are the size of the box
    uint64_t boxSize = read_uint64_from_bytes(vector<char>(header, header + 4), 0);
    if (boxSize == 0) {
        cout << "MP4 Atom box cannot be size 0." << endl;
        return -1;
    }

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

        current_pos += 8;
    }

    // next 4 bytes are the type of the box
    string type(header + 4, header + 8);

    if (DEBUG) {
        cout << "BOX: " << type << " size: " << boxSize << " @ pos: " << current_pos << "\n";
    }

    if (XMLonly) {
        if (type == "meta") {
            // Read the box data
            vector<char> data(boxSize);
            file.read(reinterpret_cast<char *> (data.data()), boxSize);
            parse_meta(data);
        }
    } else {
        if (type == "ftyp" || type == "moov" || type == "mvhd" || type == "meta") {
            // Read the box data
            vector<char> data(boxSize);
            file.read(reinterpret_cast<char *> (data.data()), boxSize);

            if (type == "ftyp") {
                string major_brand(data.begin(), data.begin() + 4);
                cout << "MP4 Major Brand: " << major_brand << "\n";
            } else if (type == "moov") {
                // Parse the moov box
                read_box(file, current_pos + 8);
            } else if (type == "mvhd") {
                // Extracting data fields
                uint64_t creation_time_raw = read_uint64_from_bytes(data, 4);
                cout << "First timestamp: " << convert_timestamp_to_date(creation_time_raw) << endl;

                uint64_t time_scale = read_uint64_from_bytes(data, 12);
                uint64_t duration = read_uint64_from_bytes(data, 16);
                float duration_seconds = static_cast<float>(duration) / static_cast<float>(time_scale);

                cout << "File duration: " << duration_seconds << " seconds" << endl;
            } else if (type == "meta") {
                parse_meta(data);
            }
        }
    }


    return boxSize;
}

void parse_mp4_atoms(ifstream &file) {
    uint64_t current_pos = 0;

    while (true) {
        try {
            uint64_t box_size = read_box(file, current_pos);
            if (box_size == -1) {
                break;
            }
            current_pos += box_size;
        } catch (const exception &ignored) {
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "mp42str <video_file_path> <options: -xml, -debug>" << endl;
        return 1;
    }

    string video_path = argv[1];
    if (video_path.substr(video_path.find_last_of('.') + 1) != "mp4" &&
        video_path.substr(video_path.find_last_of('.') + 1) != "MP4") {
        cerr << "Please provide a valid MP4 video file path." << endl;
        return 1;
    }

    if (argc == 3) {
        string option = argv[2];
        if (option == "-xml") {
            XMLonly = true;
        } else if (option == "-debug") {
            DEBUG = true;
        }
    }


    ifstream file(video_path, ios::binary);
    if (!file.is_open()) {
        cerr << "Failed to open file: " << video_path << endl;
        return 1;
    }

    if (!XMLonly)
        cout << "Reading video file: " << video_path << endl;
    parse_mp4_atoms(file);
    file.close();

    if (!XMLonly)
        cout << "Finished reading " << video_path << endl;
    return 0;
}