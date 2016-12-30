#ifndef LIST_H
#define LIST_H
 
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <string>
#include <vector>

using namespace std;

string file_mode(struct stat* buf);
string file_gid_uid(int uid, int gid);
string listPath(const char *path);
vector<string> splitString(const string &s, const string &separator);
 
#endif
