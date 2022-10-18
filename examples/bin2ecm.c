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
        // bin2ecm source
        //
        infilename  = argv[1];
        tempfilename = malloc(strlen(infilename) + 7);
        if(!tempfilename) {
            printf("Out of memory\n");
            exit_with_error();
        }

        strcpy(tempfilename, infilename);

        //
        // Append ".ecm" to the input filename
        //
        strcat(tempfilename, ".ecm");

        outfilename = tempfilename;
        break;

    case 3:
        //
        // bin2ecm source dest
        //
        infilename  = argv[1];
        outfilename = argv[2];
        break;

    default:
        banner();
        printf(
            "Usage:\n"
            "\n"
            "    bin2ecm cdimagefile\n"
            "    bin2ecm cdimagefile ecmfile\n"
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
    const FailureReason ret = prepare_encoding(infilename, outfilename, MAX_STEP_IN_BYTES, &progress);
    if(ret != SUCCESS){
        printf("ERROR: %s\n", get_failure_reason_string(ret));
        exit_with_error();
    }

    printf("Encoding %s to %s...\n", infilename, outfilename);

    int last_analyze_progress = -1;
    int last_encoding_progress = - 1;
    do{
        encode(&progress);

        if(progress.analyze_percentage != last_analyze_progress || progress.encoding_or_decoding_percentage != last_encoding_progress){
            fprintf(stderr,
                "Analyze(%02d%%) Encode(%02d%%)\r", progress.analyze_percentage, progress.encoding_or_decoding_percentage
            );

            last_analyze_progress = progress.analyze_percentage;
            last_encoding_progress = progress.encoding_or_decoding_percentage;
        }
    }while(progress.state == IN_PROGRESS);

    if(progress.state != SUCCESS){
        printf("ERROR: %s\n", get_failure_reason_string(progress.failure_reason));
        exit_with_error();
    }

    //
    // Show report
    //
    printf("Literal bytes........... "); fprintdec(stdout, progress.literal_bytes); printf("\n");
    printf("Mode 1 sectors.......... "); fprintdec(stdout, progress.mode_1_sectors); printf("\n");
    printf("Mode 2 form 1 sectors... "); fprintdec(stdout, progress.mode_2_form_1_sectors); printf("\n");
    printf("Mode 2 form 2 sectors... "); fprintdec(stdout, progress.mode_2_form_2_sectors); printf("\n");
    printf("Encoded ");
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
