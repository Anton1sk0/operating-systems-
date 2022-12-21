
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "limits.h"
#include "FileReader.h"

static int segment_loaded = -1;

char * load_segment(FILE * fp, int segment, int segment_lines) {
    char buffer[LINE_LENGTH] = { 0 };
    int skip_lines = segment*segment_lines;
    
    rewind(fp);
    
    while (fgets(buffer, sizeof (buffer), fp) && skip_lines > 0) {
        skip_lines--;
    }
    
    
    char ** buffers = (char **) malloc(sizeof(char*)*segment_lines);
    
    for (int i=0;i<segment_lines;i++) {
        buffers[i] = malloc(sizeof(char)*LINE_LENGTH);
    }
    
    for (int i=0;i<segment_lines;i++) {
        fgets(buffers[i], sizeof (buffer), fp);
    }
    
    int M = sizeof(char)*segment_lines*(LINE_LENGTH +1);
    char * merged = malloc(M);
    
    strcpy(merged, "");
    
    for (int i=0;i<segment_lines;i++) {
        strcat(merged, buffers[i]);
    }
    
    for (int i=0;i<segment_lines;i++) {
        free(buffers[i]);
    }
    
    free(buffers);
    
    segment_loaded = segment;
    
    printf("################ Segment loaded: %d \n", segment_loaded);
    
    return merged;
}

bool segment_already_loaded(int segment) {
    if (segment_loaded == segment) {
        return true;
    } else {
        return false;
    }
}