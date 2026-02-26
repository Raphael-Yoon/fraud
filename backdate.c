#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    struct utimbuf new_times;
    
    // Get the base name of the executable to skip it
    char *exec_path = strdup(argv[0]);
    char *exec_name = basename(exec_path);

    dir = opendir(".");
    if (dir == NULL) {
        perror("Unable to open current directory");
        free(exec_path);
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip current and parent directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Filter for regular files (DT_REG) only
        if (entry->d_type != DT_REG) {
            continue;
        }

        // Skip the executable itself
        if (strcmp(entry->d_name, exec_name) == 0) {
            continue;
        }

        // Get current file stats
        if (stat(entry->d_name, &file_stat) == 0) {
            time_t current_mtime = file_stat.st_mtime;
            struct tm *tm_info = localtime(&current_mtime);
            
            // Store old date string for logging
            char old_date[32];
            strftime(old_date, sizeof(old_date), "%Y-%m-%d %H:%M:%S", tm_info);

            // Subtract 1 year from the year field
            tm_info->tm_year -= 1;
            
            // Reconvert to time_t
            time_t new_mtime = mktime(tm_info);
            if (new_mtime == (time_t)-1) {
                fprintf(stderr, "Error: Could not recalculate time for %s\n", entry->d_name);
                continue;
            }

            // Set new timestamps
            new_times.actime = file_stat.st_atime; // Keep original access time
            new_times.modtime = new_mtime;        // Updated modification time

            // Apply the new timestamp
            if (utime(entry->d_name, &new_times) == 0) {
                struct tm *new_tm_info = localtime(&new_mtime);
                char new_date[32];
                strftime(new_date, sizeof(new_date), "%Y-%m-%d %H:%M:%S", new_tm_info);
                printf("Modified: %s [%s] -> [%s]\n", entry->d_name, old_date, new_date);
            } else {
                perror("Error updating timestamp");
            }
        } else {
            perror("Error getting file status");
        }
    }

    closedir(dir);
    free(exec_path);
    return 0;
}
