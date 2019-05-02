#include "logging.h"

void logfile_init(int role){
    FILE *fp;
    if(!role)
        fp = fopen("h_log.txt", "w");
    else
        fp = fopen("c_log.txt", "w");
    fclose(fp);
}

void w_log(char* msg, int role){
    FILE *fp;
    if(!role)
        fp = fopen("h_log.txt", "a");
    else
        fp = fopen("c_log.txt", "a");
    fprintf(fp, "%s", msg);
    fclose(fp);
}

