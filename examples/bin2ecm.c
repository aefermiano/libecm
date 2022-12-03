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

#define STDOUT "--stdout"

static char* tempfilename = NULL;

static void exit_with_error(){
    if(tempfilename) { free(tempfilename); }
    exit(1);
}

static void show_usage(){
    banner();
    fprintf(stderr,
        "Usage:\n"
        "\n"
        "    bin2ecm <cdimagefile>\n"
        "    bin2ecm <cdimagefile> <ecmfile>\n"
        "    bin2ecm " STDOUT " <cdimagefile> \n"
    );
}

int main(int argc, char* argv[]) {

    char* infilename  = NULL;
    char* outfilename = NULL;
    int silent = 0;

    normalize_argv0(argv[0]);

    for(int i = 1; i < argc; i++){
        char *current_argv = argv[i];

        if(strcmp(STDOUT, current_argv) == 0 && outfilename == NULL){
            outfilename = STDOUT_MARKER;
            silent = 1;
        }
        else if(infilename == NULL){
            infilename = current_argv;
        }
        else if(outfilename == NULL){
            outfilename = current_argv;
        }
        else{
            show_usage();
            exit_with_error();
        }
    }

    if(infilename == NULL){
        show_usage();
        exit_with_error();
    }

    if(outfilename == NULL){
        //
        // Append ".ecm" to the input filename
        //
        tempfilename = malloc(strlen(infilename) + 7);
        if(!tempfilename) {
            fprintf(stderr, "Out of memory\n");
            exit_with_error();
        }

        strcpy(tempfilename, infilename);

        strcat(tempfilename, ".ecm");

        outfilename = tempfilename;
    }

    if(strcmp(STDOUT_MARKER, outfilename) != 0){
        FILE *file = fopen(outfilename, "rb");
        if(file != NULL){
            fclose(file);
            fprintf(stderr, "Error: %s exists; refusing to overwrite\n", outfilename);
            exit_with_error();
        }
    }

    Progress progress;
    const FailureReason ret = prepare_encoding(infilename, outfilename, MAX_STEP_IN_BYTES, &progress);
    if(ret != SUCCESS){
        fprintf(stderr, "ERROR: %s\n", get_failure_reason_string(ret));
        exit_with_error();
    }

    if(!silent) fprintf(stderr, "Encoding %s to %s...\n", infilename, outfilename);

    int last_analyze_progress = -1;
    int last_encoding_progress = - 1;
    do{
        encode(&progress);

        if(progress.analyze_percentage != last_analyze_progress || progress.encoding_or_decoding_percentage != last_encoding_progress){
            if(!silent){
                fprintf(stderr,
                    "Analyze(%02d%%) Encode(%02d%%)\r", progress.analyze_percentage, progress.encoding_or_decoding_percentage
                );
            }

            last_analyze_progress = progress.analyze_percentage;
            last_encoding_progress = progress.encoding_or_decoding_percentage;
        }
    }while(progress.state == IN_PROGRESS);

    if(progress.state != SUCCESS){
        fprintf(stderr, "ERROR: %s\n", get_failure_reason_string(progress.failure_reason));
        exit_with_error();
    }

    //
    // Show report
    //
    if(!silent){
        fprintf(stderr, "Literal bytes........... "); fprintdec(stderr, progress.literal_bytes); printf("\n");
        fprintf(stderr, "Mode 1 sectors.......... "); fprintdec(stderr, progress.mode_1_sectors); printf("\n");
        fprintf(stderr, "Mode 2 form 1 sectors... "); fprintdec(stderr, progress.mode_2_form_1_sectors); printf("\n");
        fprintf(stderr, "Mode 2 form 2 sectors... "); fprintdec(stderr, progress.mode_2_form_2_sectors); printf("\n");
        fprintf(stderr, "Encoded ");
        fprintdec(stderr, progress.bytes_before_processing);
        fprintf(stderr, " bytes -> ");
        fprintdec(stderr, progress.bytes_after_processing);
        fprintf(stderr, " bytes\n");

        //
        // Success
        //
        fprintf(stderr, "Done\n");
    }

    if(tempfilename) { free(tempfilename); }

    return 0;
}
