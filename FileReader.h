
#ifndef FILEREADER_H
#define FILEREADER_H


char * load_segment(FILE * fp, int segment, int segment_lines);

bool  segment_already_loaded(int segment);
#endif /* FILEREADER_H */

