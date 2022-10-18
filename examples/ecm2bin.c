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

#include "ecm.h"

#include "cmdlinecommon.h"

static char* tempfilename = NULL;

static void exit_with_error(){
    if(tempfilename) { free(tempfilename); }
    exit(1);
}

int main(int argc, char** argv) {
    char* infilename  = NULL;
    char* outfilename = NULL;

    normalize_argv0(argv[0]);

    //
    // Check command line
    //
    switch(argc) {
    case 2:
        //
        // ecm2bin source
        //
        infilename  = argv[1];

        tempfilename = malloc(strlen(infilename) + 7);
        if(!tempfilename) {
            printf("Out of memory\n");
            exit_with_error();
        }

        strcpy(tempfilename, infilename);

        //
        // Remove ".ecm" from the input filename
        //
        size_t l = strlen(tempfilename);
        if(
            (l > 4) &&
            tempfilename[l - 4] == '.' &&
            tolower(tempfilename[l - 3]) == 'e' &&
            tolower(tempfilename[l - 2]) == 'c' &&
            tolower(tempfilename[l - 1]) == 'm'
        ) {
            tempfilename[l - 4] = 0;
        } else {
            //
            // If that fails, append ".unecm" to the input filename
            //
            strcat(tempfilename, ".unecm");
        }

        outfilename = tempfilename;
        break;

    case 3:
        //
        // ecm2bin source dest
        //
        infilename  = argv[1];
        outfilename = argv[2];
        break;

    default:
        banner();
        printf(
            "Usage:\n"
            "\n"
            "    ecm2bin ecmfile\n"
            "    ecm2bin ecmfile cdimagefile\n"
        );

        exit_with_error();
    }

    FILE *file = fopen(outfilename, "rb");
    if(file != NULL){
        fclose(file);
        printf("Error: %s exists; refusing to overwrite\n", outfilename);
        exit_with_error();
    }

    Progress progress;
    const FailureReason ret = prepare_decoding(infilename, outfilename, MAX_STEP_IN_BYTES, &progress);
    if(ret != SUCCESS){
        printf("ERROR: %s\n", get_failure_reason_string(ret));
        exit_with_error();
    }

    printf("Decoding %s to %s...\n", infilename, outfilename);

    int last_decoding_progress = - 1;
    do{
        decode(&progress);

        if(progress.encoding_or_decoding_percentage != last_decoding_progress){
            fprintf(stderr,
                "Decode(%02d%%)\r", progress.encoding_or_decoding_percentage
            );

            last_decoding_progress = progress.encoding_or_decoding_percentage;
        }
    }while(progress.state == IN_PROGRESS);

    if(progress.state != SUCCESS){
        printf("ERROR: %s\n", get_failure_reason_string(progress.failure_reason));
        exit_with_error();
    }

    //
    // Show report
    //
    printf("Decoded ");
    fprintdec(stdout, progress.bytes_before_processing);
    printf(" bytes -> ");
    fprintdec(stdout, progress.bytes_after_processing);
    printf(" bytes\n");

    //
    // Success
    //
    printf("Done\n");

    if(tempfilename) { free(tempfilename); }

    return 0;
}

