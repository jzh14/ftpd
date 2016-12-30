#ifndef MESSAGES_H
#define MESSAGES_H
static const char* MESSAGE_READY = "220 ftpd ready.\r\n";
static const char* MESSAGE_UTF8 = "200 Always in UTF8 mode.\r\n";
static const char* MESSAGE_NO_USERNAME_OR_PASSWORD = "530 Please login with USER and PASS.\r\n";
static const char* MESSAGE_LOGIN_INCORRECT = "530 Login incorrect.\r\n";
static const char* MESSAGE_SPECIFY_PASSWORD = "331 Please specify the password.\r\n";
static const char* MESSAGE_LOGIN_SUCCESSFUL = "230 Login successful.\r\n";
static const char* MESSAGE_QUIT = "221 Goodbye.\r\n";
static const char* MESSAGE_PASV_BEGIN = "227 Entering Passive Mode (";
static const char* MESSAGE_PASV_END = ").\r\n";
static const char* MESSAGE_ILLEGAL_PORT = "500 Illegal PORT command.\r\n";
static const char* MESSAGE_SYSTEM_TYPE = "215 UNIX Type: L8\r\n";
static const char* MESSAGE_PORT_SUCCESSFUL = "200 PORT command successful. Consider using PASV.\r\n";
static const char* MESSAGE_PWD_BEGIN = "257 \"";
static const char* MESSAGE_PWD_END = "\"\r\n";
static const char* MESSAGE_DIR_BEGIN = "150 Here comes the directory listing.\r\n";
static const char* MESSAGE_DIR_END = "226 Directory send OK.\r\n";
static const char* MESSAGE_SELECT_MODE_FIRST = "425 Use PORT or PASV first.\r\n";
static const char* MESSAGE_CD_FAILED = "550 Failed to change directory.\r\n";
static const char* MESSAGE_CD_SUCCESS = "250 Directory successfully changed.\r\n";
static const char* MESSAGE_HELP_BEGIN= "214-The following commands are recognized.\r\n";
static const char* MESSAGE_HELP = " APPE CWD  DELE HELP LIST MKD  NLST NOOP OPTS PASS PASV\r\n"
                                    " PORT PWD  QUIT RNTR RMD  RNFR RNTO STOR SYST TYPE USER\r\n"
                                    " XPWD\r\n";
static const char* MESSAGE_HELP_END = "214 Help OK.\r\n";
static const char* MESSAGE_NOOP_OK = "200 NOOP ok.\r\n";
static const char* MESSAGE_UNKNOWN_COMMAND = "500 Unknown command.\r\n";
static const char* MESSAGE_SEND_PART1 = "150 Opening BINARY mode data connection for ";
static const char* MESSAGE_SEND_PART2 = " (";
static const char* MESSAGE_SEND_PART3 = " bytes).\r\n";
static const char* MESSAGE_TRANSFER_COMPLETE = "226 Transfer complete.\r\n";
static const char* MESSAGE_FAIL_OPEN_FILE = "550 Failed to open file.\r\n";
static const char* MESSAGE_BINARY_MODE = "200 Switching to Binary mode.\r\n";
static const char* MESSAGE_ASCII_MODE = "200 Switching to ASCII mode.\r\n";
static const char* MESSAGE_UNRECOGNISED_MODE = "500 Unrecognised TYPE command.\r\n";
static const char* MESSAGE_FAIL_CREATE_FILE = "553 Could not create file.";
static const char* MESSAGE_STOR_OK = "150 Ok to send data.\r\n";
static const char* MESSAGE_STOR_COMPLETE = "226 Transfer complete.\r\n";
static const char* MESSAGE_MKDIR_SUCCESS_BEGIN = "257 \"";
static const char* MESSAGE_MKDIR_SUCCESS_END = "\" created\r\n";
static const char* MESSAGE_MKDIR_FAIL = "550 Create directory operation failed.\r\n";
static const char* MESSAGE_DELE_SUCCESS = "250 Delete operation successful.\r\n";
static const char* MESSAGE_DELE_FAIL = "550 Delete operation failed.\r\n";
static const char* MESSAGE_RMDIR_SUCCESS = "250 Remove directory operation successful.\r\n";
static const char* MESSAGE_RMDIR_FAIL = "550 Remove directory operation failed.\r\n";
static const char* MESSAGE_RNFR_SUCCESS = "350 Ready for RNTO.\r\n";
static const char* MESSAGE_RNFR_FAIL = "550 RNFR command failed.\r\n";
static const char* MESSAGE_EXPECTING_RNFR = "503 RNFR required first.\r\n";
static const char* MESSAGE_RNTO_SUCCESS = "250 Rename successful.\r\n";
static const char* MESSAGE_RNTO_FAIL = "550 Rename failed.\r\n";
static const char* MESSAGE_GREETING = "220 ";
#endif
