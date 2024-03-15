#include<stdio.h>
#include<syslog.h>
#include<string.h>

//Writer app to write the content passed in the arg to file passed in arg
int main(int argc, char**argv)
{
	FILE *fp;
	const char *path;
	const char *content;
	size_t content_len = 0;

//	printf("Starting writer app\n");
	openlog(NULL, 0, LOG_USER);

//	printf("argc = %d\n", argc);

	if(argc < 3)
	{
		syslog(LOG_ERR, "Invalid Number of arguments");
		syslog(LOG_ERR, "1) File Path.");
		syslog(LOG_ERR, "2) String to be entered to the file.");
		return 1;
	}

	path = argv[1];
	content = argv[2];

//	printf("File Path = %s\n", path);
//	printf("String to be entered to the file = %s\n", content);

	fp = fopen(path, "w");
	if(fp == NULL)
	{
//		printf("fopen() error for path: %s\n", path);
		syslog(LOG_ERR, "fopen() error for path: %s", path);
		return 1;
	}

	syslog(LOG_DEBUG, "Writing %s to %s", content, path);

	content_len = strlen(content);

	if(content_len == fwrite(content, sizeof(char), content_len, fp))
	{
//		printf("fwrite() success\n");
	}

	fclose(fp);

	return 0;
}
