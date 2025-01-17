////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022      Antonio Fermiano
// Copyright (C) 2015-2017 Maxime Gauduin
// Copyright (C) 2002-2011 Neill Corlett
//
// This file is part of libecm.
//
// libecm is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libecm is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "ecm.h"

////////////////////////////////////////////////////////////////////////////////
//
// Sector types
//
// Mode 1
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 01
// 0010h [---DATA...
// ...
// 0800h                                     ...DATA---]
// 0810h [---EDC---] 00 00 00 00 00 00 00 00 [---ECC...
// ...
// 0920h                                      ...ECC---]
// -----------------------------------------------------
//
// Mode 2 (XA), form 1
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 02
// 0010h [--FLAGS--] [--FLAGS--] [---DATA...
// ...
// 0810h             ...DATA---] [---EDC---] [---ECC...
// ...
// 0920h                                      ...ECC---]
// -----------------------------------------------------
//
// Mode 2 (XA), form 2
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 02
// 0010h [--FLAGS--] [--FLAGS--] [---DATA...
// ...
// 0920h                         ...DATA---] [---EDC---]
// -----------------------------------------------------
//
// ADDR:  Sector address, encoded as minutes:seconds:frames in BCD
// FLAGS: Used in Mode 2 (XA) sectors describing the type of sector; repeated
//        twice for redundancy
// DATA:  Area of the sector which contains the actual data itself
// EDC:   Error Detection Code
// ECC:   Error Correction Code
//

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// LUTs used for computing ECC/EDC
//
static uint8_t  ecc_f_lut[256];
static uint8_t  ecc_b_lut[256];
static uint32_t edc_lut  [256];

static void eccedc_init(void) {
    size_t i;
    for(i = 0; i < 256; i++) {
        uint32_t edc = i;
        size_t j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);
        ecc_f_lut[i] = j;
        ecc_b_lut[i ^ j] = i;
        for(j = 0; j < 8; j++) {
            edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001 : 0);
        }
        edc_lut[i] = edc;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Compute EDC for a block
//
static uint32_t edc_compute(
    uint32_t edc,
    const uint8_t* src,
    size_t size
) {
    for(; size; size--) {
        edc = (edc >> 8) ^ edc_lut[(edc ^ (*src++)) & 0xFF];
    }
    return edc;
}

////////////////////////////////////////////////////////////////////////////////
//
// Check ECC block (either P or Q)
// Returns true if the ECC data is an exact match
//
static int8_t ecc_checkpq(
    const uint8_t* address,
    const uint8_t* data,
    size_t major_count,
    size_t minor_count,
    size_t major_mult,
    size_t minor_inc,
    const uint8_t* ecc
) {
    size_t size = major_count * minor_count;
    size_t major;
    for(major = 0; major < major_count; major++) {
        size_t index = (major >> 1) * major_mult + (major & 1);
        uint8_t ecc_a = 0;
        uint8_t ecc_b = 0;
        size_t minor;
        for(minor = 0; minor < minor_count; minor++) {
            uint8_t temp;
            if(index < 4) {
                temp = address[index];
            } else {
                temp = data[index - 4];
            }
            index += minor_inc;
            if(index >= size) { index -= size; }
            ecc_a ^= temp;
            ecc_b ^= temp;
            ecc_a = ecc_f_lut[ecc_a];
        }
        ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
        if(
            ecc[major              ] != (ecc_a        ) ||
            ecc[major + major_count] != (ecc_a ^ ecc_b)
        ) {
            return 0;
        }
    }
    return 1;
}

//
// Write ECC block (either P or Q)
//
static void ecc_writepq(
    const uint8_t* address,
    const uint8_t* data,
    size_t major_count,
    size_t minor_count,
    size_t major_mult,
    size_t minor_inc,
    uint8_t* ecc
) {
    size_t size = major_count * minor_count;
    size_t major;
    for(major = 0; major < major_count; major++) {
        size_t index = (major >> 1) * major_mult + (major & 1);
        uint8_t ecc_a = 0;
        uint8_t ecc_b = 0;
        size_t minor;
        for(minor = 0; minor < minor_count; minor++) {
            uint8_t temp;
            if(index < 4) {
                temp = address[index];
            } else {
                temp = data[index - 4];
            }
            index += minor_inc;
            if(index >= size) { index -= size; }
            ecc_a ^= temp;
            ecc_b ^= temp;
            ecc_a = ecc_f_lut[ecc_a];
        }
        ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
        ecc[major              ] = (ecc_a        );
        ecc[major + major_count] = (ecc_a ^ ecc_b);
    }
}

//
// Check ECC P and Q codes for a sector
// Returns true if the ECC data is an exact match
//
static int8_t ecc_checksector(
    const uint8_t *address,
    const uint8_t *data,
    const uint8_t *ecc
) {
    return
        ecc_checkpq(address, data, 86, 24,  2, 86, ecc) &&      // P
        ecc_checkpq(address, data, 52, 43, 86, 88, ecc + 0xAC); // Q
}

//
// Write ECC P and Q codes for a sector
//
static void ecc_writesector(
    const uint8_t *address,
    const uint8_t *data,
    uint8_t *ecc
) {
    ecc_writepq(address, data, 86, 24,  2, 86, ecc);        // P
    ecc_writepq(address, data, 52, 43, 86, 88, ecc + 0xAC); // Q
}

////////////////////////////////////////////////////////////////////////////////

static const uint8_t zeroaddress[4] = {0, 0, 0, 0};

////////////////////////////////////////////////////////////////////////////////
//
// Check if this is a sector we can compress
//
// Sector types:
//   0: Literal bytes (not a sector)
//   1: 2352 mode 1         predict sync, mode, reserved, edc, ecc
//   2: 2336 mode 2 form 1  predict redundant flags, edc, ecc
//   3: 2336 mode 2 form 2  predict redundant flags, edc
//
static int8_t detect_sector(const uint8_t* sector, size_t size_available) {
    if(
        size_available >= 2352 &&
        sector[0x000] == 0x00 && // sync (12 bytes)
        sector[0x001] == 0xFF &&
        sector[0x002] == 0xFF &&
        sector[0x003] == 0xFF &&
        sector[0x004] == 0xFF &&
        sector[0x005] == 0xFF &&
        sector[0x006] == 0xFF &&
        sector[0x007] == 0xFF &&
        sector[0x008] == 0xFF &&
        sector[0x009] == 0xFF &&
        sector[0x00A] == 0xFF &&
        sector[0x00B] == 0x00 &&
        sector[0x00F] == 0x01 && // mode (1 byte)
        sector[0x814] == 0x00 && // reserved (8 bytes)
        sector[0x815] == 0x00 &&
        sector[0x816] == 0x00 &&
        sector[0x817] == 0x00 &&
        sector[0x818] == 0x00 &&
        sector[0x819] == 0x00 &&
        sector[0x81A] == 0x00 &&
        sector[0x81B] == 0x00
    ) {
        //
        // Might be Mode 1
        //
        if(
            ecc_checksector(
                sector + 0xC,
                sector + 0x10,
                sector + 0x81C
            ) &&
            edc_compute(0, sector, 0x810) == get32lsb(sector + 0x810)
        ) {
            return 1; // Mode 1
        }

    } else if(
        size_available >= 2336 &&
        sector[0] == sector[4] && // flags (4 bytes)
        sector[1] == sector[5] && //   versus redundant copy
        sector[2] == sector[6] &&
        sector[3] == sector[7]
    ) {
        //
        // Might be Mode 2, Form 1 or 2
        //
        if(
            ecc_checksector(
                zeroaddress,
                sector,
                sector + 0x80C
            ) &&
            edc_compute(0, sector, 0x808) == get32lsb(sector + 0x808)
        ) {
            return 2; // Mode 2, Form 1
        }
        //
        // Might be Mode 2, Form 2
        //
        if(
            edc_compute(0, sector, 0x91C) == get32lsb(sector + 0x91C)
        ) {
            return 3; // Mode 2, Form 2
        }
    }

    //
    // Nothing
    //
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Reconstruct a sector based on type
//
static void reconstruct_sector(
    uint8_t* sector, // must point to a full 2352-byte sector
    int8_t type
) {
    //
    // Sync
    //
    sector[0x000] = 0x00;
    sector[0x001] = 0xFF;
    sector[0x002] = 0xFF;
    sector[0x003] = 0xFF;
    sector[0x004] = 0xFF;
    sector[0x005] = 0xFF;
    sector[0x006] = 0xFF;
    sector[0x007] = 0xFF;
    sector[0x008] = 0xFF;
    sector[0x009] = 0xFF;
    sector[0x00A] = 0xFF;
    sector[0x00B] = 0x00;

    switch(type) {
    case 1:
        //
        // Mode
        //
        sector[0x00F] = 0x01;
        //
        // Reserved
        //
        sector[0x814] = 0x00;
        sector[0x815] = 0x00;
        sector[0x816] = 0x00;
        sector[0x817] = 0x00;
        sector[0x818] = 0x00;
        sector[0x819] = 0x00;
        sector[0x81A] = 0x00;
        sector[0x81B] = 0x00;
        break;
    case 2:
    case 3:
        //
        // Mode
        //
        sector[0x00F] = 0x02;
        //
        // Flags
        //
        sector[0x010] = sector[0x014];
        sector[0x011] = sector[0x015];
        sector[0x012] = sector[0x016];
        sector[0x013] = sector[0x017];
        break;
    }

    //
    // Compute EDC
    //
    switch(type) {
    case 1: put32lsb(sector+0x810, edc_compute(0, sector     , 0x810)); break;
    case 2: put32lsb(sector+0x818, edc_compute(0, sector+0x10, 0x808)); break;
    case 3: put32lsb(sector+0x92C, edc_compute(0, sector+0x10, 0x91C)); break;
    }

    //
    // Compute ECC
    //
    switch(type) {
    case 1: ecc_writesector(sector+0xC , sector+0x10, sector+0x81C); break;
    case 2: ecc_writesector(zeroaddress, sector+0x10, sector+0x81C); break;
    }

    //
    // Done
    //
}

////////////////////////////////////////////////////////////////////////////////
//
// Encode a type/count combo
//
static FailureReason write_type_count(
    FILE *out,
    int8_t type,
    uint32_t count
) {
    count--;
    if(fputc(((count >= 32) << 7) | ((count & 31) << 2) | type, out) == EOF) {
        return ERROR_WRITING_OUTPUT_FILE;
    }
    count >>= 5;
    while(count) {
        if(fputc(((count >= 128) << 7) | (count & 127), out) == EOF) {
            return ERROR_WRITING_OUTPUT_FILE;
        }
        count >>= 7;
    }

    return SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////

static uint8_t sector_buffer[2352];

static off_t mycounter_analyze = (off_t)-1;
static off_t mycounter_encode  = (off_t)-1;
static off_t mycounter_decode  = (off_t)-1;
static off_t mycounter_total   = 0;

static void resetcounter(off_t total) {
    mycounter_analyze = (off_t)-1;
    mycounter_encode  = (off_t)-1;
    mycounter_decode  = (off_t)-1;
    mycounter_total   = total;
}

static void refresh_progress_decode(Progress *progress) {
    // Case stdin total size is unknown
    if(mycounter_total < 0){
        return;
    }

    off_t d = (mycounter_decode  + 64) / 128;
    off_t t = (mycounter_total   + 64) / 128;
    if(!t) { t = 1; }

    progress->encoding_or_decoding_percentage = (((off_t)100) * d) / t;
}

static void setcounter_analyze(off_t n) {
    mycounter_analyze = n;
}

static void setcounter_encode(off_t n) {
    mycounter_encode = n;
}

static void setcounter_decode(off_t n) {
    mycounter_decode = n;
}

////////////////////////////////////////////////////////////////////////////////
//
// Encode a run of sectors/literals of the same type
//
// Returns nonzero on error
//

int write_sectors_step;
int write_sectors_count;

static FailureReason write_sectors(
    int8_t type,
    uint32_t count,
    FILE* in,
    FILE* out,
    int max_step_in_bytes
) {
    int written_bytes = 0;

    if(write_sectors_step == 1){
        const FailureReason ret = write_type_count(out, type, count);
        if( ret != SUCCESS) {
            return ret;
        }

        write_sectors_step = 2;
        write_sectors_count = count;
    }

    if(write_sectors_step == 2){
        if(type == 0) {
            while(write_sectors_count) {
                uint32_t b = write_sectors_count;
                if(b > sizeof(sector_buffer)) { b = sizeof(sector_buffer); }
                if(fread(sector_buffer, 1, b, in) != b) { return ERROR_READING_INPUT_FILE; }
                if(fwrite(sector_buffer, 1, b, out) != b) { return ERROR_WRITING_OUTPUT_FILE; }
                write_sectors_count -= b;
                written_bytes += b;
                setcounter_encode(ftello(in));

                if(write_sectors_count && written_bytes >= max_step_in_bytes){
                    return SUCCESS_PARTIAL;
                }
            }
            return SUCCESS;
        }
        write_sectors_step = 3;
    }

    if(write_sectors_step == 3){
        for(; write_sectors_count; write_sectors_count--) {
            switch(type) {
            case 1:
                if(fread(sector_buffer, 1, 2352, in) != 2352) { return ERROR_READING_INPUT_FILE; }
                if(fwrite(sector_buffer + 0x00C, 1, 0x003, out) != 0x003) { return ERROR_WRITING_OUTPUT_FILE; }
                if(fwrite(sector_buffer + 0x010, 1, 0x800, out) != 0x800) { return ERROR_WRITING_OUTPUT_FILE; }
                written_bytes += 0x003 + 0x800;
                break;
            case 2:
                if(fread(sector_buffer, 1, 2336, in) != 2336) { return ERROR_READING_INPUT_FILE; }
                if(fwrite(sector_buffer + 0x004, 1, 0x804, out) != 0x804) { return ERROR_WRITING_OUTPUT_FILE; }
                written_bytes += 2336 + 0x804;
                break;
            case 3:
                if(fread(sector_buffer, 1, 2336, in) != 2336) { return ERROR_READING_INPUT_FILE; }
                if(fwrite(sector_buffer + 0x004, 1, 0x918, out) != 0x918) { return ERROR_WRITING_OUTPUT_FILE; }
                written_bytes += 0x918;
                break;
            }
            setcounter_encode(ftello(in));

            if(written_bytes >= max_step_in_bytes){
                write_sectors_count--;
                return SUCCESS_PARTIAL;
            }
        }
    }
    //
    // Success
    //
    return SUCCESS;
}

static FILE *in;
static FILE *out;

static uint8_t* queue;
static size_t queue_start_ofs;
static size_t queue_bytes_available;

static uint32_t input_edc;

static int8_t   curtype;
static uint32_t curtype_count;
static off_t    curtype_in_start;

static uint32_t literal_skip;

static off_t input_file_length;
static off_t input_bytes_checked;
static off_t input_bytes_queued;

static off_t typetally[4];

static const size_t sectorsize[4] = {
    1,
    2352,
    2336,
    2336
};

static uint32_t output_edc;
static int8_t type;
static uint32_t num;

static size_t queue_size;
static int8_t detecttype;
static int max_step_in_bytes;

int writing_sectors;
int decoding_state;

const char * const failure_reason_names[] = { FAILURE_REASONS };

////////////////////////////////////////////////////////////////////////////////

static void reset_progress(Progress *progress){
    memset(progress, 0, sizeof(Progress));
    progress->state = IN_PROGRESS;

    return;
}

static void fill_report_encoding(Progress *progress){
    progress->literal_bytes = typetally[0];
    progress->mode_1_sectors = typetally[1];
    progress->mode_2_form_1_sectors = typetally[2];
    progress->mode_2_form_2_sectors = typetally[3];
    progress->bytes_before_processing = input_file_length;
    progress->bytes_after_processing = ftello(out);
}

static void fill_report_decoding(Progress *progress){
    progress->bytes_before_processing = ftello(in);
    progress->bytes_after_processing = ftello(out);
}

void refresh_progress_encode(Progress *progress){
    off_t a = (mycounter_analyze + 64) / 128;
    off_t e = (mycounter_encode  + 64) / 128;
    off_t t = (mycounter_total   + 64) / 128;
    if(!t) { t = 1; }

    progress->analyze_percentage = (unsigned)((((off_t)100) * a) / t);
    progress->encoding_or_decoding_percentage = (unsigned)((((off_t)100) * e) / t);
}

////////////////////////////////////////////////////////////////////////////////

FailureReason prepare_decoding(char *input_file_name, char *output_file_name, int max_step_in_bytes_, Progress *progress){
    reset_progress(progress);

    max_step_in_bytes = max_step_in_bytes_;
    decoding_state = 1;

    eccedc_init();

    output_edc = 0;

    //
    // Open both files
    //
    if(strcmp(STDIN_MARKER, input_file_name) == 0){
        in = stdin;

        // Unknown, statistics won't be updated
        input_file_length = -1;
    }else{
        in = fopen(input_file_name, "rb");
        if(!in){
            return ERROR_OPENING_INPUT_FILE;
        }

        //
        // Get the length of the input file
        //
        if(fseeko(in, 0, SEEK_END) != 0) {
            return ERROR_READING_INPUT_FILE;
        }
        input_file_length = ftello(in);
        if(input_file_length < 0) {
            return ERROR_READING_INPUT_FILE;
        }

        if(fseeko(in, 0, SEEK_SET) != 0) {
            return ERROR_READING_INPUT_FILE;
        }
    }

    resetcounter(input_file_length);

    //
    // Magic header
    //
    if(
        (fgetc(in) != 'E') ||
        (fgetc(in) != 'C') ||
        (fgetc(in) != 'M') ||
        (fgetc(in) != 0x00)
    ) {
        return INVALID_ECM_FILE;
    }

    //
    // Open output file
    //
    if(strcmp(STDOUT_MARKER, output_file_name) == 0){
        out = stdout;
    }else{
        out = fopen(output_file_name, "wb");
        if(!out) {
            return ERROR_OPENING_OUTPUT_FILE;
        }
    }

    return SUCCESS;
}

FailureReason prepare_encoding(char *input_file_name, char *output_file_name, int max_step_in_bytes_, Progress *progress){
    reset_progress(progress);

    max_step_in_bytes = max_step_in_bytes_;
    writing_sectors = 0;

    eccedc_init();

    queue = NULL;
    queue_start_ofs = 0;
    queue_bytes_available = 0;

    input_edc = 0;

    //
    // Current sector type (run)
    //
    curtype = -1; // not a valid type
    curtype_count = 0;
    curtype_in_start = 0;

    literal_skip = 0;

    input_bytes_checked = 0;
    input_bytes_queued  = 0;

    memset(typetally, 0, sizeof(typetally));

    queue_size = ((size_t)(-1)) - 4095;
    if((unsigned long)queue_size > 0x40000lu) {
        queue_size = (size_t)0x40000lu;
    }

    //
    // Allocate space for queue
    //
    queue = malloc(queue_size);
    if(!queue) {
        return OUT_OF_MEMORY;
    }

    //
    // Open both files
    //
    if(strcmp(STDIN_MARKER, input_file_name) == 0){
        return STDIN_NOT_SUPPORTED;
    }
    in = fopen(input_file_name, "rb");
    if(!in) {
        return ERROR_OPENING_INPUT_FILE;
    }

    if(strcmp(STDOUT_MARKER, output_file_name) == 0){
        out = stdout;
    }
    else{
        out = fopen(output_file_name, "wb");
        if(!out) {
            return ERROR_OPENING_OUTPUT_FILE;
        }
    }

    //
    // Get the length of the input file
    //
    if(fseeko(in, 0, SEEK_END) != 0) {
        return ERROR_READING_INPUT_FILE;
    }
    input_file_length = ftello(in);
    if(input_file_length < 0) {
        return ERROR_READING_INPUT_FILE;
    }

    resetcounter(input_file_length);

    //
    // Magic identifier
    //
    if(fputc('E' , out) == EOF) { return ERROR_WRITING_OUTPUT_FILE; }
    if(fputc('C' , out) == EOF) { return ERROR_WRITING_OUTPUT_FILE; }
    if(fputc('M' , out) == EOF) { return ERROR_WRITING_OUTPUT_FILE; }
    if(fputc(0x00, out) == EOF) { return ERROR_WRITING_OUTPUT_FILE; }

    return SUCCESS;
}

void encode(Progress *progress){
    //
    // Refill queue if necessary
    //
    if(!writing_sectors){
        if(
            (queue_bytes_available < 2352) &&
            (((off_t)queue_bytes_available) < (input_file_length - input_bytes_queued))
        ) {
            //
            // We need to read more data
            //
            off_t willread = input_file_length - input_bytes_queued;
            off_t maxread = queue_size - queue_bytes_available;
            if(willread > maxread) {
                willread = maxread;
            }
            if(willread > max_step_in_bytes){
                willread = max_step_in_bytes;
            }

            if(queue_start_ofs > 0) {
                memmove(queue, queue + queue_start_ofs, queue_bytes_available);
                queue_start_ofs = 0;
            }
            if(willread) {
                setcounter_analyze(input_bytes_queued);

                if(fseeko(in, input_bytes_queued, SEEK_SET) != 0) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_READING_INPUT_FILE;
                    return;
                }
                if(fread(queue + queue_bytes_available, 1, willread, in) != (size_t)willread) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_READING_INPUT_FILE;
                    return;
                }

                input_edc = edc_compute(
                    input_edc,
                    queue + queue_bytes_available,
                    willread
                );

                input_bytes_queued    += willread;
                queue_bytes_available += willread;
            }
        }

        if(queue_bytes_available == 0) {
            //
            // No data left to read -> quit
            //
            detecttype = -1;

        } else if(literal_skip > 0) {
            //
            // Skipping through literal bytes
            //
            literal_skip--;
            detecttype = 0;

        } else {
            //
            // Heuristic to skip past CD sync after a mode 2 sector
            //
            if(
                curtype >= 2 &&
                queue_bytes_available >= 0x10 &&
                queue[queue_start_ofs + 0x0] == 0x00 &&
                queue[queue_start_ofs + 0x1] == 0xFF &&
                queue[queue_start_ofs + 0x2] == 0xFF &&
                queue[queue_start_ofs + 0x3] == 0xFF &&
                queue[queue_start_ofs + 0x4] == 0xFF &&
                queue[queue_start_ofs + 0x5] == 0xFF &&
                queue[queue_start_ofs + 0x6] == 0xFF &&
                queue[queue_start_ofs + 0x7] == 0xFF &&
                queue[queue_start_ofs + 0x8] == 0xFF &&
                queue[queue_start_ofs + 0x9] == 0xFF &&
                queue[queue_start_ofs + 0xA] == 0xFF &&
                queue[queue_start_ofs + 0xB] == 0x00 &&
                queue[queue_start_ofs + 0xF] == 0x02
            ) {
                // Treat this byte as a literal...
                detecttype = 0;
                // ...and skip the next 15
                literal_skip = 15;
            } else {
                //
                // Detect the sector type at the current offset
                //
                detecttype = detect_sector(queue + queue_start_ofs, queue_bytes_available);
            }
        }
    }

    if( (!writing_sectors) &&
        (detecttype == curtype) &&
        (curtype_count <= 0x7FFFFFFF) // avoid overflow
    ) {
        //
        // Same type as last sector
        //
        curtype_count++;

    } else {
        //
        // Changing types: Flush the input
        //
        if(curtype_count > 0 || writing_sectors) {
            if(!writing_sectors){
                if(fseeko(in, curtype_in_start, SEEK_SET) != 0) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_READING_INPUT_FILE;
                    return;
                }
                typetally[curtype] += curtype_count;

                write_sectors_step = 1;
                writing_sectors = 1;
            }

            if(writing_sectors){
                FailureReason writeSectorsRet = write_sectors(
                                curtype,
                                curtype_count,
                                in,
                                out,
                                max_step_in_bytes);

                if(writeSectorsRet == SUCCESS_PARTIAL){
                    refresh_progress_encode(progress);
                    return;
                }
                else if(writeSectorsRet == FAILURE) {
                    progress->state = FAILURE;
                    progress->failure_reason = writeSectorsRet;
                    return;
                }

                writing_sectors = 0;
            }
        }
        curtype = detecttype;
        curtype_in_start = input_bytes_checked;
        curtype_count = 1;
    }

    if(curtype >= 0) {
        input_bytes_checked   += sectorsize[curtype];
        queue_start_ofs       += sectorsize[curtype];
        queue_bytes_available -= sectorsize[curtype];
        refresh_progress_encode(progress);

        //
        // Advance to the next sector
        //
        return;
    }

    //
    // Current type is negative ==> quit
    //

    //
    // Store the end-of-records indicator
    //
    const FailureReason writeTypeCountRet = write_type_count(out, 0, 0);
    if(writeTypeCountRet != SUCCESS) {
        progress->state = FAILURE;
        progress->failure_reason = writeTypeCountRet;
        return;
    }

    //
    // Store the EDC of the input file
    //
    put32lsb(sector_buffer, input_edc);
    if(fwrite(sector_buffer, 1, 4, out) != 4) {
        progress->state = FAILURE;
        progress->failure_reason = ERROR_WRITING_OUTPUT_FILE;
        return;
    }

    //
    // Success
    //
    progress->state = COMPLETED;
    progress->analyze_percentage = 100;
    progress->encoding_or_decoding_percentage = 100;
    fill_report_encoding(progress);

    if(queue != NULL) { free(queue); }
    if(in    != NULL) { fclose(in ); }
    if(out != NULL && out != stdout ) { fclose(out); }
}

void decode(Progress *progress){
    int bytesRead = 0;

    if(decoding_state == 1){
        int c = fgetc(in);
        int bits = 5;
        if(c == EOF) {
            progress->state = FAILURE;
            progress->failure_reason = ERROR_READING_INPUT_FILE;
            return;
        }
        type = c & 3;
        num = (c >> 2) & 0x1F;
        while(c & 0x80) {
            c = fgetc(in);
            if(c == EOF) {
                progress->state = FAILURE;
                progress->failure_reason = ERROR_READING_INPUT_FILE;
                return;
            }
            if(
                (bits > 31) ||
                ((uint32_t)(c & 0x7F)) >= (((uint32_t)0x80000000LU) >> (bits-1))
            ) {
                progress->state = FAILURE;
                progress->failure_reason = INVALID_ECM_FILE;
                return;
            }
            num |= ((uint32_t)(c & 0x7F)) << bits;
            bits += 7;
        }
        if(num == 0xFFFFFFFF) {
            // End indicator
            decoding_state = 4;
        }
        else{
            num++;
            decoding_state = 2;
        }
    }

    if(decoding_state == 2){
        if(type == 0) {
            while(num) {
                uint32_t b = num;
                if(b > sizeof(sector_buffer)) { b = sizeof(sector_buffer); }
                if(fread(sector_buffer, 1, b, in) != b) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_READING_INPUT_FILE;
                    return;
                }

                bytesRead += b;

                output_edc = edc_compute(output_edc, sector_buffer, b);
                if(fwrite(sector_buffer, 1, b, out) != b) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_WRITING_OUTPUT_FILE;
                    return;
                }
                num -= b;
                setcounter_decode(ftello(in));

                if(bytesRead >= max_step_in_bytes){
                    refresh_progress_decode(progress);
                    return;
                }
            }
            decoding_state = 1;
        }
        else{
            decoding_state = 3;
        }
    }

    if(decoding_state == 3){
        for(; num; num--) {
            switch(type) {
            case 1:
                if(fread(sector_buffer + 0x00C, 1, 0x003, in) != 0x003) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_READING_INPUT_FILE;
                    return;
                }
                if(fread(sector_buffer + 0x010, 1, 0x800, in) != 0x800) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_READING_INPUT_FILE;
                    return;
                }
                bytesRead += 0x003 + 0x800;

                reconstruct_sector(sector_buffer, 1);
                output_edc = edc_compute(output_edc, sector_buffer, 2352);
                if(fwrite(sector_buffer, 1, 2352, out) != 2352) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_WRITING_OUTPUT_FILE;
                    return;
                }
                break;
            case 2:
                if(fread(sector_buffer + 0x014, 1, 0x804, in) != 0x804) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_READING_INPUT_FILE;
                    return;
                }
                bytesRead += 0x804;

                reconstruct_sector(sector_buffer, 2);
                output_edc = edc_compute(output_edc, sector_buffer + 0x10, 2336);
                if(fwrite(sector_buffer + 0x10, 1, 2336, out) != 2336) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_WRITING_OUTPUT_FILE;
                    return;
                }
                break;
            case 3:
                if(fread(sector_buffer + 0x014, 1, 0x918, in) != 0x918) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_READING_INPUT_FILE;
                    return;
                }
                bytesRead += 0x918;

                reconstruct_sector(sector_buffer, 3);
                output_edc = edc_compute(output_edc, sector_buffer + 0x10, 2336);
                if(fwrite(sector_buffer + 0x10, 1, 2336, out) != 2336) {
                    progress->state = FAILURE;
                    progress->failure_reason = ERROR_WRITING_OUTPUT_FILE;
                    return;
                }
                break;
            }
            if(bytesRead >= max_step_in_bytes){
                num--;
                refresh_progress_decode(progress);
                return;
            }
            setcounter_decode(ftello(in));
        }
        decoding_state = 1;
    }

    if(decoding_state != 4){
        refresh_progress_decode(progress);
        return;
    }

    //
    // Verify the EDC of the entire output file
    //
    if(fread(sector_buffer, 1, 4, in) != 4) {
        progress->state = FAILURE;
        progress->failure_reason = ERROR_READING_INPUT_FILE;
        return;
    }

    fill_report_decoding(progress);

    if(get32lsb(sector_buffer) != output_edc) {
        progress->state = FAILURE;
        progress->failure_reason = ERROR_IN_CHECKSUM;
        return;
    }

    //
    // Success
    //

    if(in != NULL && in != stdin ) { fclose(in ); }
    if(out != NULL && out != stdout ) { fclose(out); }

    progress->state = COMPLETED;
    progress->failure_reason = SUCCESS;
    return;
}

const char *get_failure_reason_string(FailureReason failureReason){
    return failure_reason_names[failureReason];
}

