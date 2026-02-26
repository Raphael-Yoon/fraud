#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <stdlib.h>

/**
 * 조작개발팀 System Engineer [김정음]
 * 리눅스/유닉스 환경 특화: 옵션에 따라 파일 수정 시간(mtime)을 연 단위로 조작합니다.
 * 사용법: ./backdate_linux [-y years]
 * 예시: ./backdate_linux -y -1  (1년 전으로)
 *       ./backdate_linux -y 1   (1년 후로/원복)
 */

void print_usage(const char *progname) {
    printf("사용법: %s [-y years]\n", progname);
    printf("옵션:\n");
    printf("  -y years : 가감할 연도 (기본값: -1)\n");
    printf("           예: -1 (1년 전으로), 1 (1년 후로/원복)\n");
}

int main(int argc, char *argv[]) {
    int year_diff = -1; // 기본값: 1년 전
    int opt;

    // 인자 처리
    while ((opt = getopt(argc, argv, "y:h")) != -1) {
        switch (opt) {
            case 'y':
                year_diff = atoi(optarg);
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }

    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    struct timespec new_times[2];
    
    char *exec_path = strdup(argv[0]);
    char *exec_name = basename(exec_path);

    dir = opendir(".");
    if (dir == NULL) {
        perror("현재 디렉토리를 열 수 없습니다");
        free(exec_path);
        return 1;
    }

    printf("--- 조작 시작 (연도 변이: %s%d년) ---\n", year_diff > 0 ? "+" : "", year_diff);

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (entry->d_type != DT_REG) {
            continue;
        }

        if (strcmp(entry->d_name, exec_name) == 0 || strcmp(entry->d_name, "backdate_linux.c") == 0) {
            continue;
        }

        if (stat(entry->d_name, &file_stat) == 0) {
            time_t current_mtime = file_stat.st_mtime;
            struct tm *tm_info = localtime(&current_mtime);
            
            char old_date[32];
            strftime(old_date, sizeof(old_date), "%Y-%m-%d %H:%M:%S", tm_info);

            // 지정된 연도만큼 가감
            tm_info->tm_year += year_diff;
            
            time_t new_mtime = mktime(tm_info);
            if (new_mtime == (time_t)-1) {
                fprintf(stderr, "오류: %s의 시간을 계산할 수 없습니다.\n", entry->d_name);
                continue;
            }

            new_times[0].tv_sec = file_stat.st_atime;
            new_times[0].tv_nsec = 0;
            new_times[1].tv_sec = new_mtime;
            new_times[1].tv_nsec = 0;

            if (utimensat(AT_FDCWD, entry->d_name, new_times, 0) == 0) {
                struct tm *new_tm_info = localtime(&new_mtime);
                char new_date[32];
                strftime(new_date, sizeof(new_date), "%Y-%m-%d %H:%M:%S", new_tm_info);
                printf("성공: %-15s [%s] -> [%s]\n", entry->d_name, old_date, new_date);
            } else {
                perror("타임스탬프 업데이트 오류");
            }
        }
    }

    printf("--- 조작 완료 ---\n");
    closedir(dir);
    free(exec_path);
    return 0;
}
