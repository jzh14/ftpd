#ifndef UTILS_H
#define UTILS_H
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

pair<string, string> generatePath(string rootPath, string basePath, string newPath);
string chDir(const string& dir, const string& cdParam);
bool isDir(const char *path);
bool isFile(const char *path);
bool fileExists(const char *path);
vector<string> listDir(const string& dir);
string strToUpper(const string& str);
string strToLower(const string& str);
vector<string> splitString(const string &s, const string &separator);

string getCurrentPath() {
    char cCurrentPath[FILENAME_MAX];
    if (!getcwd(cCurrentPath, sizeof(cCurrentPath))) {
        return string("");
    }
    cCurrentPath[sizeof(cCurrentPath) - 1] = '\0';
    return string(cCurrentPath);
}

pair<string, string> generatePath(string rootPath, string basePath, string newPath) {
    //cout << "basePath: " << basePath << endl;
    //cout << "newPath: " << newPath << endl;
    if (newPath.front() == '/') {
        basePath = "/";
        newPath = newPath.substr(1);
    }
    while (newPath.back() == '/') {
        newPath.pop_back();
    }
    vector<string> pathParts = splitString(newPath, string("/"));
    string tempPath = basePath;
    bool specialFile = false;
    if (pathParts.size() > 1) {
        for (int i = 0; i < pathParts.size() - 1; ++i) {
            tempPath = chDir(tempPath, pathParts[i]);
        }
        if (pathParts.back().compare("..") == 0 || pathParts.back().compare(".") == 0) {
            specialFile = true;
        }
    }
    else if (newPath.compare("..") == 0) {
        //cout << "chDir(tempPath, \"..\") = " << chDir(tempPath, "..") << endl;
        tempPath = chDir(tempPath, "..");
        specialFile = true;
    }
    else if (newPath.compare(".") == 0) {
        tempPath = chDir(tempPath, ".");
        specialFile = true;
    }
    //cout << "resultPath: " << rootPath + tempPath + (!specialFile ? pathParts.back() : "") << endl;
    return pair<string, string>(rootPath + tempPath, (!specialFile && pathParts.size()) ? pathParts.back() : "");
}

string chDir(const string& dir, const string& cdParam) {
    string result(dir);
    if (cdParam.compare("..") == 0) {
        if (result.back() == '/') {
            result.pop_back();
        }
        int pos = result.find_last_of("/");
        result = result.substr(0, pos);
        result.append("/");
    }
    else if (cdParam.compare(".") != 0) {
        result.append(cdParam);
        result.append("/");
    }
    return result;
}

bool isDir(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

bool isFile(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

bool fileExists(const char *path) {
    return access(path, F_OK) != -1;
}

vector<string> listDir(const string& dir) {
    DIR *dp;
    struct dirent *ep;
    vector<string> result;

    dp = opendir(dir.c_str());
    if (dp != NULL) {
        while (ep = readdir (dp)) {
            if (strcmp(ep->d_name, "..") != 0 && strcmp(ep->d_name, ".") != 0) {
                result.push_back(ep->d_name);
            }
        }
        closedir(dp);
    }
    else {
        perror ("Couldn't open the directory");
    }

    return result;
}

string strToUpper(const string& str) {
    string result = str;
    for (string::iterator it = result.begin(); it != result.end(); ++it) {
        *it = toupper(*it);
    }
    return result;
}

string strToLower(const string& str) {
    string result = str;
    for (string::iterator it = result.begin(); it != result.end(); ++it) {
        *it = tolower(*it);
    }
    return result;
}

vector<string> splitString(const string &s, const string &separator) {
    vector<string> result;
    typedef string::size_type string_size;
    string_size i = 0;

    while(i != s.size()) {
        //找到字符串中首个不等于分隔符的字母；
        int flag = 0;
        while(i != s.size() && flag == 0){
            flag = 1;
            for(string_size x = 0; x < separator.size(); ++x) {
                if (s[i] == separator[x]) {
                    ++i;
                    flag = 0;
                    break;
                }
            }
        }

        //找到又一个分隔符，将两个分隔符之间的字符串取出；
        flag = 0;
        string_size j = i;
        while(j != s.size() && flag == 0){
            for(string_size x = 0; x < separator.size(); ++x) {
                if (s[j] == separator[x]){
                    flag = 1;
                    break;
                }
            }
            if(flag == 0) {
                ++j;
            }
        }
        if(i != j){
            result.push_back(s.substr(i, j-i));
            i = j;
        }
    }
    return result;
}
#endif