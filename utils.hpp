#pragma once

#include <cstdarg>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <regex>
#include<cstring>

#include<time.h>


#ifndef _UTILS_HPP_
#define _UTILS_HPP_

#ifndef WIN32
#define vfprintf_s vfprintf
#endif

namespace fits {
    struct UID {
        static int generate();
    private:
        static int uid;
    };

    ///ascii escap colors: http://pueblo.sourceforge.net/doc/manual/ansi_color_codes.html
    struct Utils {
        static std::string trim(std::string& str);
        static void log(int level, const char* format, ...) {
            if (level < 0) level = -1;
            else if (level >= COLORS.size()) level = 7;
            const std::string full = COLORS.at(level) + format;
            va_list pal;
            va_start(pal, format);
            vfprintf_s(level < 0 ? stderr : stdout, full.data(), pal);
            va_end(pal);
        }
        static void log(const char* format, ...) {
            va_list pal;
            va_start(pal, format);
            vfprintf_s(stdout, format, pal);
            va_end(pal);
        }
        static long standard_to_stamp(std::string inputStringTime) {
            time_t testTimestamp = time(NULL);
            struct tm timetest;
            //localtime_s(&timetest, &testTimestamp);
            std::string year = std::to_string(timetest.tm_year + 1900);
            std::string stringTemp = inputStringTime;
            std::regex re("(\\d+)(\\D+)(\\d+)");
            std::smatch s2;
            std::regex_search(stringTemp, s2, re);
            std::string mouth = s2[1];
            int mouthNum = std::stoi(mouth);
            if (mouthNum < 10)
                mouth = "0" + mouth;
            std::string day = s2[3];
            int dayNum = std::stoi(day);
            if (dayNum < 10)
                day = "0" + day;
            std::string timeString = year + ":" + mouth + ":" + day + " " + "00:00:00";
            char str_time[256];
            memset(str_time, 0, sizeof(str_time));
            for (int i = 0;timeString[i] != '\0';i++) {
                str_time[i] = timeString[i];
            }
            struct tm stm;
            int iY, iM, iD, iH, iMin, iS;
            memset(&stm, 0, sizeof(stm));
            iY = atoi(str_time);
            iM = atoi(str_time + 5);
            iD = atoi(str_time + 8);
            iH = atoi(str_time + 11);
            iMin = atoi(str_time + 14);
            iS = atoi(str_time + 17);

            stm.tm_year = iY - 1900;
            stm.tm_mon = iM - 1;
            stm.tm_mday = iD;
            stm.tm_hour = iH + 8;
            stm.tm_min = iMin;
            stm.tm_sec = iS;


            return (long)mktime(&stm);
        }
    private:
        static const std::map<int, std::string> COLORS;
    };

    typedef std::vector<std::string> Row;
    typedef std::function<void(int, Row&)> RowCallback;
    struct CSVLoader {
        CSVLoader(const std::string& filename);
        int load(RowCallback callback);

    private:
        std::string filename;
    };

    typedef std::map<int/*index of column*/, std::string/*value*/> IndexedRow;
    struct CSVWriter {
        CSVWriter(const std::string& filename, const Row& columns = {}, bool append = true);
        virtual ~CSVWriter();
        int write(Row& row);
        int write(const Row& row);
        int write(IndexedRow& row);

    private:
        std::string filename;
        std::ofstream ofs;
        const int countOfColumns;
    };

    struct FakeWriter : public CSVWriter {
        FakeWriter() : CSVWriter("") {};
        int write(Row& row) { return 0; };
        int write(const Row& row) { return 0; };
        int write(IndexedRow& row) { return 0; };
    };
}

#endif