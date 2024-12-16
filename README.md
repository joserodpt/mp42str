# mp42str ![Build Status](https://github.com/joserodpt/mp42str/actions/workflows/cmake-multi-platform.yml/badge.svg)
[Download Latest Artifacts](https://github.com/joserodpt/mp42str/actions)

## Project Overview

`mp42str` is a C++ utility program designed to extract timecodes and additional data from MP4 files that comply with the ISO Base Media File Format (ISO/IEC 14496-12 - MPEG-4 Part 12). The program can extract timecodes to `.str` files and additional data in XML format from specific atoms/boxes within the MP4 file.
More specifically, the program reads the following atoms/boxes:
- `ftyp`: File Type Box
- `moov`: Movie Box
- `mvhd`: Movie Header Box
- `meta`: Metadata Box

More info on what each atom/box contains can be found in the [mp4atoms.pdf](readings/mp4atoms.pdf) and [mep4metaAtom.pdf](readings/mp4metaAtom.pdf).

## Generated .srt file example
```
1
00:00:00,00 --> 00:00:01,00
13-12-2024
23:11:12

2
00:00:01,00 --> 00:00:02,00
13-12-2024
23:11:13

3
00:00:02,00 --> 00:00:03,00
13-12-2024
23:11:14

...
```

## Features

- Extracts timecodes from MP4 files and writes them to `.str` files.
- Capable of extracting additional data in XML format from `meta` and `xml ` atoms/boxes.
- Provides options to extract only XML data, if present in the mpeg4 file.


## Usage

```sh
mp42str <video_file_path> [options]
Options:
-xml: Extract additional data in XML format only.
-debug: Print debug information.
```

## Code Overview
The main C++ file contains the following key components:  
- Timestamp Conversion: Converts timestamps to human-readable date formats.
- Box Reading: Reads and processes different MP4 atoms/boxes (ftyp, moov, mvhd, meta).
- FTYP Parsing: Determines the MPEG4 encoding type.
- MOOV/MVHD Parsing: These atoms provide the first timecode example of the file.
- Meta Parsing: Extracts and processes data from meta and xml atoms/boxes.
- SRT Writing: Writes extracted timecodes to an SRT file.
