#include "list.h"

string file_mode(struct stat *buf) {
    int i;
    char buff[11] = {"----------"};
    switch (buf->st_mode & S_IFMT) {
        case S_IFIFO:
            buff[0] = 'f';
            break;
        case S_IFDIR:
            buff[0] = 'd';
            break;
        case S_IFSOCK:
            buff[0] = 's';
            break;
        case S_IFBLK:
            buff[0] = 'b';
            break;
        case S_IFLNK:
            buff[0] = 'l';
            break;
    }

    if (buf->st_mode & S_IRUSR) {
        buff[1] = 'r';
    }
    if (buf->st_mode & S_IWUSR) {
        buff[2] = 'w';
    }
    if (buf->st_mode & S_IXUSR) {
        buff[3] = 'x';
    }
    if (buf->st_mode & S_IRGRP) {
        buff[4] = 'r';
    }
    if (buf->st_mode & S_IWGRP) {
        buff[5] = 'w';
    }
    if (buf->st_mode & S_IXGRP) {
        buff[6] = 'x';
    }
    if (buf->st_mode & S_IROTH) {
        buff[7] = 'r';
    }
    if (buf->st_mode & S_IWOTH) {
        buff[8] = 'w';
    }
    if (buf->st_mode & S_IXOTH) {
        buff[9] = 'x';
    }

    return string(buff);
}

string file_gid_uid(int uid, int gid) {
    struct passwd *ptr;
    struct group *str;

    ptr = getpwuid(uid);
    str = getgrgid(gid);

    string result("\t");
    result.append(ptr->pw_name);
    result.append("\t");
    result.append(str->gr_name);
    return result;
}

string listPath(const char* path) {
    DIR* fd;
    struct dirent* fp;
    string result;

    struct stat buf;
    int ret;

    if ((fd = opendir(path)) == NULL) {
        perror("open file failed!");
        exit(0);
    }
    while ((fp = readdir(fd)) != NULL) {
        string tempPath(path);
        tempPath.append(fp->d_name);
        if ((ret = stat(tempPath.c_str(), &buf)) == -1) {
            perror("stat");
            exit(0);
        }

        result.append(file_mode(&buf));
        result.append("    ");
        result.append(to_string(buf.st_nlink));
        result.append(" ");
        result.append(to_string(buf.st_uid));
        result.append("        ");
        result.append(to_string(buf.st_gid));
        result.append("        ");
        result.append(to_string(buf.st_size));
        result.append(" ");
        string tempTime(4 + ctime(&buf.st_mtime));
        vector<string> timeParts = splitString(tempTime, " ");
        result.append(timeParts[0]);
        result.append(" ");
        result.append(timeParts[1]);
        result.append(" ");
        result.append(timeParts[3]);
        result.pop_back();
        result.append(" ");
        result.append(fp->d_name);
        result.append("\r\n");
    }

    closedir(fd);
    return result;
}

