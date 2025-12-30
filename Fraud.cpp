#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <locale>

#ifdef _WIN32
    #include <windows.h>
    #include <fileapi.h>
    #include <io.h>
    #include <direct.h>
#else
    #include <sys/stat.h>
    #include <utime.h>
    #include <dirent.h>
    #include <unistd.h>
#endif

class FileDateModifier {
private:
    std::string currentExecutablePath;
    std::string logFileName;
    std::ofstream logFile;
    std::vector<std::string> targetExtensions;
    bool processAllFiles;

    // 현재 실행 파일의 경로를 가져오는 함수
    std::string getCurrentExecutablePath() {
        #ifdef _WIN32
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            return std::string(buffer);
        #else
            char buffer[1024];
            ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer)-1);
            if (len != -1) {
                buffer[len] = '\0';
                return std::string(buffer);
            }
            return "";
        #endif
    }

    // 파일 확장자 확인 함수
    bool shouldProcessFile(const std::string& filePath) {
        if (processAllFiles) {
            return true;
        }

        // 파일 확장자 추출
        size_t dotPos = filePath.find_last_of('.');
        if (dotPos == std::string::npos) {
            return false; // 확장자가 없는 파일
        }

        std::string extension = filePath.substr(dotPos);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        // 대상 확장자 목록에서 확인
        for (size_t i = 0; i < targetExtensions.size(); ++i) {
            if (extension == targetExtensions[i]) {
                return true;
            }
        }

        return false;
    }

    // 명령행 인수에서 확장자 파싱
    void parseExtensions(const std::vector<std::string>& args) {
        processAllFiles = true;
        targetExtensions.clear();

        for (size_t i = 0; i < args.size(); ++i) {
            const std::string& arg = args[i];
            // C++20의 starts_with 대신 수동 구현
            if (arg.length() >= 2 && arg.substr(0, 2) == "*.") {
                std::string ext = arg.substr(1); // '*' 제거
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                targetExtensions.push_back(ext);
                processAllFiles = false;
            }
        }
    }

    // 현재 시간을 문자열로 변환 (로그용)
    std::string getCurrentTimeString() {
        time_t now = time(0);
        char buffer[100];
        struct tm* timeinfo = localtime(&now);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(buffer);
    }

    // 로그 파일 이름 생성
    std::string generateLogFileName() {
        time_t now = time(0);
        char buffer[100];
        struct tm* timeinfo = localtime(&now);
        strftime(buffer, sizeof(buffer), "file_date_change_log_%Y%m%d_%H%M%S.txt", timeinfo);
        return std::string(buffer);
    }

    // 로그 기록 함수
    void writeLog(const std::string& message) {
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.flush(); // 즉시 파일에 쓰기
        }
    }

    void writeFileChangeLog(const std::string& fileName,
                           const std::string& oldTime,
                           const std::string& newTime,
                           bool success) {
        std::string message;
        if (success) {
            message = "[성공] " + fileName + " | " + oldTime + " → " + newTime;
        } else {
            message = "[실패] " + fileName + " | " + oldTime + " (변경 실패)";
        }
        writeLog(message);
    }

    // 1년을 빼는 함수 (윤년 고려)
    time_t subtractOneYear(time_t originalTime) {
        struct tm* tm = localtime(&originalTime);

        // 1년 빼기
        tm->tm_year -= 1;

        // 2월 29일이고 대상 연도가 윤년이 아닌 경우 2월 28일로 조정
        if (tm->tm_mon == 1 && tm->tm_mday == 29) {  // 2월 29일
            int targetYear = tm->tm_year + 1900;
            bool isLeapYear = (targetYear % 4 == 0 && targetYear % 100 != 0) || (targetYear % 400 == 0);
            if (!isLeapYear) {
                tm->tm_mday = 28;
            }
        }

        return mktime(tm);
    }

    // 시간을 문자열로 변환
    std::string timeToString(time_t timeValue) {
        char buffer[100];
        struct tm* timeinfo = localtime(&timeValue);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(buffer);
    }

    #ifdef _WIN32
    // Windows에서 파일 시간 변경
    bool modifyFileTime(const std::string& filePath, time_t newTime) {
        HANDLE hFile = CreateFileA(filePath.c_str(),
                                  FILE_WRITE_ATTRIBUTES,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);

        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }

        // time_t를 FILETIME으로 변환
        LONGLONG ll = Int32x32To64(newTime, 10000000) + 116444736000000000LL;
        FILETIME ft;
        ft.dwLowDateTime = (DWORD)ll;
        ft.dwHighDateTime = (DWORD)(ll >> 32);

        BOOL result = SetFileTime(hFile, &ft, &ft, &ft); // 생성, 접근, 수정 시간 모두 설정
        CloseHandle(hFile);

        return result != 0;
    }

    // Windows에서 파일 시간 가져오기
    time_t getFileTime(const std::string& filePath) {
        WIN32_FILE_ATTRIBUTE_DATA fileData;
        if (GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &fileData)) {
            ULARGE_INTEGER ull;
            ull.LowPart = fileData.ftLastWriteTime.dwLowDateTime;
            ull.HighPart = fileData.ftLastWriteTime.dwHighDateTime;

            return (time_t)((ull.QuadPart - 116444736000000000LL) / 10000000);
        }
        return 0;
    }
    #else
    // Linux/Unix에서 파일 시간 변경
    bool modifyFileTime(const std::string& filePath, time_t newTime) {
        struct utimbuf times;
        times.actime = newTime;   // 접근 시간
        times.modtime = newTime;  // 수정 시간

        return utime(filePath.c_str(), &times) == 0;
    }

    // Linux/Unix에서 파일 시간 가져오기
    time_t getFileTime(const std::string& filePath) {
        struct stat fileStat;
        if (stat(filePath.c_str(), &fileStat) == 0) {
            return fileStat.st_mtime;
        }
        return 0;
    }
    #endif

    // 디렉토리 내 파일 목록 가져오기
    std::vector<std::string> getFilesInDirectory(const std::string& directory) {
        std::vector<std::string> files;

        #ifdef _WIN32
            WIN32_FIND_DATAA findFileData;
            HANDLE hFind = FindFirstFileA((directory + "\\*").c_str(), &findFileData);

            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                        files.push_back(std::string(findFileData.cFileName));
                    }
                } while (FindNextFileA(hFind, &findFileData) != 0);
                FindClose(hFind);
            }
        #else
            DIR* dir = opendir(directory.c_str());
            if (dir != NULL) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != NULL) {
                    struct stat fileStat;
                    std::string fullPath = directory + "/" + entry->d_name;
                    if (stat(fullPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                        files.push_back(std::string(entry->d_name));
                    }
                }
                closedir(dir);
            }
        #endif

        return files;
    }

public:
    FileDateModifier(const std::vector<std::string>& args) {
        parseExtensions(args);

        currentExecutablePath = getCurrentExecutablePath();
        logFileName = generateLogFileName();

        // 로그 파일 열기
        logFile.open(logFileName.c_str());
        if (!logFile.is_open()) {
            std::cerr << "Warning: Cannot create log file: " << logFileName << std::endl;
        } else {
            std::cout << "Log file created: " << logFileName << std::endl;
        }

        std::cout << "Current executable: " << currentExecutablePath << std::endl;
        std::cout << "This file will be excluded from processing." << std::endl;

        if (processAllFiles) {
            std::cout << "처리 대상: 모든 파일" << std::endl;
        } else {
            std::cout << "처리 대상 확장자: ";
            for (size_t i = 0; i < targetExtensions.size(); ++i) {
                std::cout << "*" << targetExtensions[i];
                if (i < targetExtensions.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;

        // 로그 파일에 헤더 정보 기록
        if (logFile.is_open()) {
            writeLog("=== 파일 날짜 변경 로그 ===");
            writeLog("프로그램 실행 시간: " + getCurrentTimeString());
            writeLog("실행 파일: " + currentExecutablePath);

            #ifdef _WIN32
                char currentDir[MAX_PATH];
                GetCurrentDirectoryA(MAX_PATH, currentDir);
                writeLog("작업 디렉토리: " + std::string(currentDir));
            #else
                char currentDir[1024];
                if (getcwd(currentDir, sizeof(currentDir)) != NULL) {
                    writeLog("작업 디렉토리: " + std::string(currentDir));
                }
            #endif

            if (processAllFiles) {
                writeLog("처리 대상: 모든 파일");
            } else {
                std::string targetInfo = "처리 대상 확장자: ";
                for (size_t i = 0; i < targetExtensions.size(); ++i) {
                    targetInfo += "*" + targetExtensions[i];
                    if (i < targetExtensions.size() - 1) {
                        targetInfo += ", ";
                    }
                }
                writeLog(targetInfo);
            }

            writeLog("");
            writeLog("파일명 | 기존 날짜 → 새로운 날짜");
            writeLog("----------------------------------------");
        }
    }

    ~FileDateModifier() {
        if (logFile.is_open()) {
            writeLog("");
            writeLog("=== 로그 종료 ===");
            writeLog("종료 시간: " + getCurrentTimeString());
            logFile.close();
        }
    }

    void processCurrentDirectory() {
        try {
            int processedCount = 0;
            int skippedCount = 0;
            int excludedByExtension = 0;

            std::cout << "현재 디렉토리의 파일들을 처리중입니다...\n" << std::endl;

            std::vector<std::string> files = getFilesInDirectory(".");

            for (size_t i = 0; i < files.size(); ++i) {
                const std::string& fileName = files[i];

                // 현재 실행 파일인지 확인
                std::string fullPath;
                #ifdef _WIN32
                    char currentDir[MAX_PATH];
                    GetCurrentDirectoryA(MAX_PATH, currentDir);
                    fullPath = std::string(currentDir) + "\\" + fileName;
                #else
                    char currentDir[1024];
                    if (getcwd(currentDir, sizeof(currentDir)) != NULL) {
                        fullPath = std::string(currentDir) + "/" + fileName;
                    }
                #endif

                if (fullPath == currentExecutablePath) {
                    std::cout << "[건너뜀] " << fileName << " (실행 파일)" << std::endl;
                    writeLog("[건너뜀] " + fileName + " (실행 파일)");
                    skippedCount++;
                    continue;
                }

                // 로그 파일 자체는 건너뛰기
                if (fileName.find(logFileName) != std::string::npos) {
                    std::cout << "[건너뜀] " << fileName << " (로그 파일)" << std::endl;
                    writeLog("[건너뜀] " + fileName + " (로그 파일)");
                    skippedCount++;
                    continue;
                }

                // 확장자 필터 적용
                if (!shouldProcessFile(fileName)) {
                    std::cout << "[제외] " << fileName << " (확장자 불일치)" << std::endl;
                    writeLog("[제외] " + fileName + " (확장자 불일치)");
                    excludedByExtension++;
                    continue;
                }

                try {
                    // 현재 파일의 수정 시간 가져오기
                    time_t currentTime = getFileTime(fileName);
                    if (currentTime == 0) {
                        std::cout << "[오류] " << fileName << ": 파일 시간을 읽을 수 없습니다." << std::endl;
                        writeLog("[오류] " + fileName + ": 파일 시간을 읽을 수 없습니다.");
                        continue;
                    }

                    // 1년 전 시간 계산
                    time_t newTime = subtractOneYear(currentTime);

                    std::string oldTimeStr = timeToString(currentTime);
                    std::string newTimeStr = timeToString(newTime);

                    std::cout << "[처리중] " << fileName << std::endl;
                    std::cout << "  기존 시간: " << oldTimeStr << std::endl;
                    std::cout << "  새로운 시간: " << newTimeStr << std::endl;

                    // 파일 시간 변경
                    if (modifyFileTime(fileName, newTime)) {
                        std::cout << "  ✓ 성공적으로 변경되었습니다." << std::endl;
                        writeFileChangeLog(fileName, oldTimeStr, newTimeStr, true);
                        processedCount++;
                    } else {
                        std::cout << "  ✗ 변경에 실패했습니다." << std::endl;
                        writeFileChangeLog(fileName, oldTimeStr, newTimeStr, false);
                    }

                } catch (const std::exception& e) {
                    std::cout << "[오류] " << fileName << ": " << e.what() << std::endl;
                    writeLog("[오류] " + fileName + ": " + e.what());
                }

                std::cout << std::endl;
            }

            std::cout << "=== 처리 완료 ===" << std::endl;
            std::cout << "처리된 파일: " << processedCount << "개" << std::endl;
            std::cout << "건너뛴 파일: " << skippedCount << "개" << std::endl;
            if (!processAllFiles) {
                std::cout << "확장자 불일치로 제외된 파일: " << excludedByExtension << "개" << std::endl;
            }
            std::cout << "로그 파일: " << logFileName << std::endl;

            // 로그에도 결과 기록
            writeLog("");
            writeLog("=== 처리 결과 요약 ===");
            writeLog("처리된 파일: " + intToString(processedCount) + "개");
            writeLog("건너뛴 파일: " + intToString(skippedCount) + "개");
            if (!processAllFiles) {
                writeLog("확장자 불일치로 제외된 파일: " + intToString(excludedByExtension) + "개");
            }

        } catch (const std::exception& e) {
            std::cerr << "디렉토리 처리 중 오류 발생: " << e.what() << std::endl;
            writeLog("치명적 오류: " + std::string(e.what()));
        }
    }

private:
    // int를 string으로 변환 (C++11 이전 호환)
    std::string intToString(int value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }
};

// 사용법 출력 함수
void printUsage(const std::string& programName) {
    std::cout << "파일 날짜 변경 프로그램" << std::endl;
    std::cout << "현재 폴더의 파일들의 생성/수정 날짜를 1년 전으로 변경합니다." << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "사용법:" << std::endl;
    std::cout << "  " << programName << "                    // 모든 파일 처리" << std::endl;
    std::cout << "  " << programName << " *.txt              // .txt 파일만 처리" << std::endl;
    std::cout << "  " << programName << " *.jpg *.png        // .jpg, .png 파일만 처리" << std::endl;
    std::cout << "  " << programName << " *.doc *.docx *.pdf // 여러 확장자 동시 처리" << std::endl;
    std::cout << std::endl;
    std::cout << "예시:" << std::endl;
    std::cout << "  " << programName << " *.txt              // 모든 텍스트 파일" << std::endl;
    std::cout << "  " << programName << " *.jpg              // 모든 JPEG 이미지" << std::endl;
    std::cout << "  " << programName << " *.mp4 *.avi        // 동영상 파일들" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // Windows에서 한글 출력을 위한 코드페이지 설정
    #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        // 또는 시스템 로케일 사용
        // setlocale(LC_ALL, "korean");
    #endif

    // 명령행 인수를 벡터로 변환
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(std::string(argv[i]));
    }

    // 도움말 요청 확인
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "/h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    printUsage(argv[0]);

    std::string input;
    std::cout << "Continue? (Y/n): ";
    std::getline(std::cin, input);

    // 빈 문자열이거나 y, Y인 경우 계속 진행
    if (!input.empty() && input != "y" && input != "Y") {
        std::cout << "Program terminated." << std::endl;
        return 0;
    }

    FileDateModifier modifier(args);
    modifier.processCurrentDirectory();

    std::cout << "\nProgram completed. Press Enter to exit...";
    std::cin.get();

    return 0;
}
