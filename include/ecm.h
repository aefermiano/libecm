////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022      Antonio Fermiano
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

#pragma once

#include "common.h"

typedef enum _State { COMPLETED,
                      IN_PROGRESS,
                      FAILURE } State;

// Little trick to generate "FailureReason" enum with "failure_reason_names" array with its names
#define FAILURE_REASONS \
    F(SUCCESS)\
    F(SUCCESS_PARTIAL)\
    F(ERROR_OPENING_INPUT_FILE)\
    F(ERROR_OPENING_OUTPUT_FILE)\
    F(OUT_OF_MEMORY)\
    F(ERROR_READING_INPUT_FILE)\
    F(ERROR_WRITING_OUTPUT_FILE)\
    F(INVALID_ECM_FILE)\
    F(ERROR_IN_CHECKSUM)\
    F(STDIN_NOT_SUPPORTED)
#define F(x) x,
typedef enum _FailureReason { FAILURE_REASONS } FailureReason;
#undef F

#define F(x) #x,
extern const char * const failure_reason_names[];

#define STDIN_MARKER "_marker_stdin"
#define STDOUT_MARKER "_marker_stdout"

typedef struct _Progress {
    State state;
    FailureReason failure_reason;
    int analyze_percentage;
    int encoding_or_decoding_percentage;
    off_t literal_bytes;
    off_t mode_1_sectors;
    off_t mode_2_form_1_sectors;
    off_t mode_2_form_2_sectors;
    off_t bytes_before_processing;
    off_t bytes_after_processing;
} Progress;

FailureReason prepare_encoding(char *inputFileName, char *outputFileName, int maxStepInBytes, Progress *progress);
void encode(Progress *progress);

FailureReason prepare_decoding(char *inputFileName, char *outputFileName, int maxStepInBytes, Progress *progress);
void decode(Progress *progress);

const char *get_failure_reason_string(FailureReason failureReason);
