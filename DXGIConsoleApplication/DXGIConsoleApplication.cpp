// DXGIConsoleApplication.cpp : Defines the entry point for the console application.
//

#include "IntelEnoder.h"
#include <time.h>

clock_t start = 0, stop = 0, duration = 0;
int count = 0;
FILE *log_file;
char file_name[MAX_PATH];



int main()
{
	fopen_s(&log_file, "logY.txt", "w");

	IntelEncoder intelEncoder;
	intelEncoder.InitializeX();
	intelEncoder.RunVppAndEncode();
	intelEncoder.FlushVppAndEncoder();
	intelEncoder.CloseResources();

	fclose(log_file);
    return 0;
}
