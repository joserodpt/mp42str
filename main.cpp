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
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <iomanip>
#include <stdexcept>

using namespace std;

#define VERSION 0.1
bool DEBUG, XMLonly;
string video_path;

#define INFO_ICON "(i) "
#define WARNING_ICON "(!) "
#define ACTION_ICON "> "
#define OK_ICON " (OK)"

string convert_timestamp_to_date(uint32_t timestamp, bool break_line = false) {
    time_t raw_time = timestamp; // Adjust for Mac timestamp
    struct tm *time_info;
    char buffer[80];
    time_info = gmtime(&raw_time);

    strftime(buffer, sizeof(buffer), break_line ? "%d-%m-%Y\n%H:%M:%S" : "%d-%m-%Y %H:%M:%S", time_info);
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
                cout << WARNING_ICON << "This file contains additional data in XML." << endl;
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

/**
 * @brief Format seconds as an SRT-compatible timestamp.
 * @param total_seconds The total number of seconds.
 * @return A formatted string "HH:MM:SS,000" (SRT requires milliseconds, so we add ,000)
 */
string format_seconds(int total_seconds) {
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d,000", hours, minutes, seconds);
    return string(buffer);
}

/**
 * @brief Write the list of dates as subtitles to an SRT file.
 * @param file_path The original video file path.
 * @param dates_list A vector of date strings, each in the format "dd/mm/yyyy hh:mm:ss".
 */
void write_dates_to_srt( const vector<string>& dates_list) {
    // Replace .mp4 or .MP4 with .srt
    string srt_file_path = video_path;
    size_t pos = srt_file_path.rfind(".mp4");
    if (pos == string::npos) {
        pos = srt_file_path.rfind(".MP4");
    }
    if (pos != string::npos) {
        srt_file_path.replace(pos, 4, ".srt");
    }

    cout << ACTION_ICON << "Writing timecodes to SRT file: " << srt_file_path;

    ofstream srt_file(srt_file_path);
    if (!srt_file.is_open()) {
        cerr << "Failed to open SRT file for writing: " << srt_file_path << endl;
        return;
    }

    int previous_time = 0;
    for (size_t i = 0; i < dates_list.size(); ++i) {
        const string& date = dates_list[i];

        // Write the subtitle index
        srt_file << (i + 1) << "\n";

        // Write the time range
        srt_file << format_seconds(previous_time) << " --> "
                 << format_seconds(previous_time + 1) << "\n";

        // Write the date and time
        srt_file << date << "\n\n";

        // Update the previous time
        previous_time += 1;  // Increment by 1 second
    }

    srt_file.close();
    cout << OK_ICON << endl;
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
                cout << INFO_ICON << "MP4 Major Brand: " << major_brand << "\n";
            } else if (type == "moov") {
                // Parse the moov box
                read_box(file, current_pos + 8);
            } else if (type == "mvhd") {
                // Extracting data fields
                uint64_t creation_time_raw = read_uint64_from_bytes(data, 4) - 2082844800;
                cout << INFO_ICON << "First timestamp: " << convert_timestamp_to_date(creation_time_raw) << endl;

                uint64_t time_scale = read_uint64_from_bytes(data, 12);
                uint64_t duration = read_uint64_from_bytes(data, 16);
                int duration_seconds = round(static_cast<double>(duration) / static_cast<double>(time_scale));

                cout << INFO_ICON << "File duration: " << duration_seconds << " seconds" << endl;

                vector<string> timecode;
                for (int i = 0; i < duration_seconds; ++i) {
                    timecode.push_back(convert_timestamp_to_date(creation_time_raw + i, true));
                }

                write_dates_to_srt(timecode);
            } else if (type == "meta") {
                parse_meta(data);
            }
        }
    }


    return boxSize;
}

void parse_mp4_atoms(ifstream &file) {
    uint64_t current_pos = 0;

    cout << OK_ICON << endl;

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

    video_path = argv[1];
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

    if (!XMLonly) {
        cout << "  __  __ _____  _  _ ___      _    " << endl;
        cout << R"( |  \/  |  __ \| || |__ \    | |  v)" << VERSION << endl;
        cout << " | \\  / | |__) | || |_ ) |___| |_ _ __ " << endl;
        cout << " | |\\/| |  ___/|__   _/ // __| __| '__|" << endl;
        cout << " | |  | | |       | |/ /_\\__ \\ |_| |   " << endl;
        cout << " |_|  |_|_|       |_|____|___/\\__|_|   " << endl;
        cout << endl;
        cout << ACTION_ICON << "Reading video file: " << video_path;
    }
    parse_mp4_atoms(file);
    file.close();

    if (!XMLonly)
        cout << ACTION_ICON << "Finished reading " << video_path << endl;
    return 0;
}