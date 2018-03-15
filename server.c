#include <stdio.h>
#include <unistd.h>
#include<stdlib.h> 
#include <string.h>
#include <pwd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <dirent.h>
#include "threadpool.h"

#define STRING_LEN 4000
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define TIME_BUF_SIZE 128
#define FIRST_PART_OF_REQUEST "HTTP/1.1 200 OK\r\nServer: webserver/1.1\r\nDate: "
#define CONTENT_TYPE "\r\nContent-Type: "
#define CONTENT_LENGTH "\r\nContent-Length: "
#define LAST_MODIFIED "\r\nLast-Modified: "
#define CONNECTION_CLOSE "\r\nConnection: close\r\n\r\n"
#define DIR_RESP_START "<HTML>\r\n<HEAD><TITLE>Index of " 
#define DIR_RESP_MID1 "</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of "
#define DIR_RESP_MID2 "</H4>\r\n\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n" 
#define DIR_RESP_END "\r\n\r\n</table>\r\n\r\n<HR>\r\n\r\n<ADDRESS>webserver/1.1</ADDRESS>\r\n\r\n</BODY></HTML>\r\n\0"
#define FIRST_PART_400 "HTTP/1.1 400 Bad Request\r\nServer: webserver/1.1\r\nDate: "
#define SECOND_PART_400 "\r\nContent-Type: text/html\r\nContent-Length: 113\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n<BODY><H4>400 Bad request</H4>\r\nBad Request.\r\n</BODY></HTML>\r\n\0"
#define FIRST_PART_404 "HTTP/1.1 404 Not Found\r\nServer: webserver/1.1\r\nDate: "
#define SECOND_PART_404 "\r\nContent-Type: text/html\r\nContent-Length: 112\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n<BODY><H4>404 Not Found</H4>\r\nFile not found.\r\n</BODY></HTML>\r\n\0"
#define FIRST_PART_501 "HTTP/1.1 501 Not supported\r\nServer: webserver/1.1\r\nDate: "
#define SECOND_PART_501 "\r\nContent-Type: text/html\r\nContent-Length: 129\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n<BODY><H4>501 Not supported</H4>\r\nMethod is not supported.\r\n</BODY></HTML>\r\n\0"
#define FIRST_PART_500 "HTTP/1.1 500 Internal Server Error\r\nServer: webserver/1.1\r\nDate: "
#define SECOND_PART_500 "\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n\0"
#define FIRST_PART_403 "HTTP/1.1 403 Forbidden\r\nServer: webserver/1.1\r\nDate: "
#define SECOND_PART_403 "\r\nContent-Type: text/html\r\nContent-Length: 111\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n<BODY><H4>403 Forbidden</H4>\r\nAccess denied.\r\n</BODY></HTML>\r\n\0"
#define FIRST_PART_302 "HTTP/1.1 302 Found\r\nServer: webserver/1.1\r\nDate: "
#define SECOND_PART_302 "\r\nLocation: "
#define THIRD_PART_302 "/\r\nContent-Type: text/html\r\nContent-Length: 123\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n<BODY><H4>302 Found</H4>\r\nDirectories must end with a slash.\r\n</BODY></HTML>\r\n\0"
#define MAX_ARGS 4
#define SIZE_OF_LINE 500
#define SIZE_OF_FILE_BUFFER 1024
#define MAX_DIGITS 15
#define ERROR 1
#define NOT_A_NUMBER 0
#define NUMBER 1
#define SIZE_OF_POOL argv[2]
#define NUM_OF_REQUESTS argv[3]
#define NO_PERMITIONS 0
#define HAS_PERMITIONS 1
#define VALID_PATH 0
#define RESPONSE_WITHOUT_FILE -1
#define SIZE_OF_INDEX_HTML 11
#define EQUALS 0
//*****************************************************************************************//
//*****************************************************************************************//
//
char *get_mime_type(char *name)
{
	char *ext = strrchr(name, '.');
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".gif") == 0) return "image/gif";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".au") == 0) return "audio/basic";
	if (strcmp(ext, ".wav") == 0) return "audio/wav";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
	return NULL;
}
//*****************************************************************************************//
//*****************************************************************************************//
//function for counting the number of digits in a given number
int numOfDigits(int num)
{
	unsigned long count = 0;

	while(num > 0)
	{
		num = num/10;
		count++;
	}
	if(count == 0)
	    return 1;
	else
	    return count;
}
//*****************************************************************************************//
//*****************************************************************************************//
//functions that calculates the length in bytes of a given file
char *getContentLength(char *path)
{
	int fd = open (path, 'r');
	struct stat buf;
	fstat(fd, &buf);
	unsigned long size = buf.st_size;
	unsigned long len = numOfDigits(size);
	char *cl = (char*)malloc((sizeof(char))*len+1);
	sprintf(cl, "%ld", size);
	cl[strlen(cl)]='\0';
	char *res = strdup(cl);
	free(cl);
	cl = NULL;
	close(fd);
	return res;
}
//*****************************************************************************************//
//*****************************************************************************************//
//return a string with the current time in a specific format (as defined above)
char* getTime()
{
	time_t now;
	char timebuf[TIME_BUF_SIZE];
	char *currtime;
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	currtime = strdup(timebuf);
	return currtime;
}
//*****************************************************************************************//
//*****************************************************************************************//
//function for creating a response for a bad reaquest
char* badRequest(int type , char *path)
{
    char *str;//will hold the response
	char *date = getTime();
    if(type == 400) //request is not with 3 parts at the first line
	{
		str = (char*)malloc(sizeof(char)*(strlen(FIRST_PART_400)+strlen(date)+strlen(SECOND_PART_400)+1));
		strcpy(str,FIRST_PART_400);
		strcat(str,date);
		strcat(str,SECOND_PART_400);
		str[strlen(str)]='\0';
	}
    if(type == 501) //not supported request
	{
		str = (char*)malloc(sizeof(char)*(strlen(FIRST_PART_501)+strlen(date)+strlen(SECOND_PART_501)+1));
		strcpy(str,FIRST_PART_501);
		strcat(str,date);
		strcat(str,SECOND_PART_501);
		str[strlen(str)]='\0';
	} 
	if(type == 404) //path is not found
	{
		str = (char*)malloc(sizeof(char)*(strlen(FIRST_PART_404)+strlen(date)+strlen(SECOND_PART_404)+1));
		strcpy(str,FIRST_PART_404);
		strcat(str,date);
		strcat(str,SECOND_PART_404);
		str[strlen(str)]='\0';
	}

	if(type == 302) //path is a dir and it is without /
	{
		str = (char*)malloc(sizeof(char)*(strlen(FIRST_PART_302)+strlen(date)+strlen(path)+strlen(SECOND_PART_302)+strlen(THIRD_PART_302)+1));
		strcpy(str,FIRST_PART_302);
		strcat(str,date);
		strcat(str,SECOND_PART_302);
		strcat(str,path);
		strcat(str,THIRD_PART_302);
		str[strlen(str)]='\0';
	}
	if(type == 500) //malloc error or some other server errors
	{
		str = (char*)malloc(sizeof(char)*(strlen(FIRST_PART_500)+strlen(date)+strlen(SECOND_PART_500)+1));
		strcpy(str,FIRST_PART_500);
		strcat(str,date);
		strcat(str,SECOND_PART_500);
		str[strlen(str)]='\0';
	}
	if(type == 403) //file is not regular or is without read permissions 
	{
		str = (char*)malloc(sizeof(char)*(strlen(FIRST_PART_403)+strlen(date)+strlen(SECOND_PART_403)+1));
		strcpy(str,FIRST_PART_403);
		strcat(str,date);
		strcat(str,SECOND_PART_403);
		str[strlen(str)]='\0';
	}
	free(date);
	date = NULL;
	return str;  
}
//*****************************************************************************************//
//*****************************************************************************************//
//function that creates the data of file to be sent back to the client
void sendData(int *newsockfd, char *path)
{
	char newstr[strlen(path)+SIZE_OF_INDEX_HTML];
	size_t size = SIZE_OF_FILE_BUFFER;
	unsigned char buffer[size];
	FILE * f;
	if((strcmp(path,"/") == EQUALS) || (strcmp(path,"") == EQUALS) || (path[strlen(path)-1] == '/')) //if path is with index.html
	{
		strcpy(newstr,path);
		strcat(newstr,"index.html\0");
		f = fopen (newstr, "rb");
	}
	else
		f = fopen (path, "rb");

	fseek (f, 0, SEEK_SET);
	int nbytes;
 	while((nbytes = fread (buffer,1,SIZE_OF_FILE_BUFFER, f)) > 0) //write file to socket
         	write(*newsockfd,buffer,nbytes);
	 fclose(f);
}
//*****************************************************************************************//
//*****************************************************************************************//
//function that creates a response with a table with dir content 
char* createDirResponse(char *dir)
{
	struct dirent **namelist; //holds names of files in dir
    int n; //number of files in dir
	char timebuf[TIME_BUF_SIZE]; // will hold the mod time
    n = scandir(dir, &namelist, NULL, alphasort);
	int size = n*SIZE_OF_LINE; //size of memory needed for the table
	unsigned long filesize; //will hold file size
	struct stat buf; // holds file info
	char cl[MAX_DIGITS]; // holds content length
	char *str; // the header of the response
	char *date = getTime(); //holds current date
	char *cntType = "text/html"; //constant
	char * newresp; //the response that will be returned
	char *newpath; //path with addition of file names
	char *tablestr = (char*)malloc(sizeof(char)*(size+strlen(DIR_RESP_START)+strlen(dir)+strlen(DIR_RESP_MID1)+strlen(dir)+strlen(DIR_RESP_MID2)+strlen(DIR_RESP_END))+1);
	if(tablestr == NULL) //table str will hold the table html structure
	{
		free(date);
		date = NULL;
		return NULL;
	}
	strcpy(tablestr,DIR_RESP_START);
	strcat(tablestr,dir);
	strcat(tablestr,DIR_RESP_MID1);
	strcat(tablestr,dir);
	strcat(tablestr,DIR_RESP_MID2);
	char *firstPart = "<tr>\r\n<td><A HREF=\"";
	char *secondPart = "\">";
	char *thirdPart = "</A></td><td>";
	char *fourthPart = "</td><td>";
	char *last = "</td>\r\n</tr>";
    if (n < 0)
        perror("scandir");
    else
	{
		int i;
        for(i = 0 ; i < n ; i++) //loop through the files in the given folder
		{
				newpath = (char*)malloc(sizeof(char)*(strlen(namelist[i]->d_name)+strlen(dir))+1);
				if(newpath == NULL)
				{
					free(date);
					date = NULL;
					free(namelist);
					namelist = NULL;
					free(tablestr);
					tablestr = NULL;
					return NULL;
				}
				strcpy(newpath,dir);
				strcat(newpath,namelist[i]->d_name);
				newpath[strlen(newpath)] = '\0';
				//constructing the body
				stat(newpath, &buf);
				filesize = buf.st_size;
				strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&buf.st_mtime));
				sprintf(cl, "%ld", filesize);
				strcat(tablestr,firstPart);
				strcat(tablestr,namelist[i]->d_name);
				if((buf.st_mode & S_IFDIR))
					strcat(tablestr,"/");
				strcat(tablestr,secondPart);
				strcat(tablestr,namelist[i]->d_name);
				strcat(tablestr,thirdPart);
				strcat(tablestr,timebuf);
				strcat(tablestr,fourthPart);
				if(!(buf.st_mode & S_IFDIR))
					strcat(tablestr,cl);
				strcat(tablestr,last);
				strcat(tablestr,"\r\n");
				free(newpath);
				newpath = NULL;	
			free(namelist[i]);
        }
		strcat(tablestr,DIR_RESP_END);      
		tablestr[strlen(tablestr)]='\0';
    }
	sprintf(cl, "%d", (int)strlen(tablestr));
	stat(dir, &buf);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&buf.st_mtime)); //mod time of the folder
	//str holder the header
	str = (char*)malloc(sizeof(char)*(strlen(FIRST_PART_OF_REQUEST)+strlen(date)+strlen(CONTENT_TYPE)+strlen(cntType)+strlen(CONTENT_LENGTH)+strlen(cl)+strlen(LAST_MODIFIED)+strlen(timebuf)+strlen(CONNECTION_CLOSE))+1);
	if(str == NULL)
	{
		free(date);
		date = NULL;
		free(namelist);
		namelist = NULL;
		free(tablestr);
		tablestr = NULL;
		free(newpath);
		newpath = NULL;
		return NULL;
	}
	//construct the header
	strcpy(str,FIRST_PART_OF_REQUEST);
	strcat(str,date);
	strcat(str,CONTENT_TYPE);
	strcat(str,cntType);
	strcat(str,CONTENT_LENGTH);
	strcat(str,cl);
	strcat(str,LAST_MODIFIED);
	strcat(str,timebuf);
	strcat(str,CONNECTION_CLOSE);
	str[strlen(str)]='\0';
	
	newresp = (char*)malloc(sizeof(char)*(strlen(str)+strlen(tablestr))+1);
	if(newresp == NULL)
	{
		free(date);
		date = NULL;
		free(namelist);
		namelist = NULL;
		free(tablestr);
		tablestr = NULL;
		free(str);
		str = NULL;
		return NULL;
	}
	//merge two parts together
	strcpy(newresp,str);
	strcat(newresp,tablestr);

	free(date);
	date = NULL;
	free(tablestr);
	tablestr = NULL;
	free(namelist);
	namelist = NULL;
	free(str);
	str = NULL;
	
	return newresp;
}
//*****************************************************************************************//
//*****************************************************************************************//
//function that creates OK 200 response header need to add last modified
char *createFileResponseHeader(char *path, int *error)
{
	char *str;
	char *date = getTime();
	char *cntLentgh = getContentLength(path);
	char *cntType = get_mime_type(path);
	int fd;
	struct stat buf; 
	char timebuf[TIME_BUF_SIZE];
	fd = open (path, 'r');
	fstat(fd, &buf);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&buf.st_mtime)); //mod time of the folder
	close(fd);
	if(cntType != NULL) //no content type
		str = (char*)malloc(sizeof(char)*(strlen(FIRST_PART_OF_REQUEST)+strlen(date)+strlen(CONTENT_TYPE)+strlen(cntType)+strlen(CONTENT_LENGTH)+strlen(cntLentgh)+strlen(LAST_MODIFIED)+strlen(timebuf)+strlen(CONNECTION_CLOSE))+1);
	else
		str = (char*)malloc(sizeof(char)*(strlen(FIRST_PART_OF_REQUEST)+strlen(date)+strlen(CONTENT_LENGTH)+strlen(cntLentgh)+strlen(LAST_MODIFIED)+strlen(timebuf)+strlen(CONNECTION_CLOSE))+1);
	
	if(str == NULL) //malloc check
	{
		free(date);
		date == NULL;
		free(cntLentgh);
		cntLentgh = NULL;
		*error = RESPONSE_WITHOUT_FILE;
		return NULL;
	}//construct the header
	strcpy(str,FIRST_PART_OF_REQUEST);
	strcat(str,date);
	if(cntType!= NULL)
	{
		strcat(str,CONTENT_TYPE);
		strcat(str,cntType);
	}
	strcat(str,CONTENT_LENGTH);
	strcat(str,cntLentgh);
	strcat(str,LAST_MODIFIED);
	strcat(str,timebuf);
	strcat(str,CONNECTION_CLOSE);
	str[strlen(str)]='\0';
	
	free(date);
	date = NULL;
	free(cntLentgh);
	cntLentgh = NULL;

	return str;
}
//*****************************************************************************************//
//*****************************************************************************************//
//This function checks permitions of a give path (read and execute)
int checkPermitions(char *path)
{
	if(strcmp(path,"") == EQUALS) //if root
		return HAS_PERMITIONS;

	char *token;
	char *str;
	char checkedParts[strlen(path)];
	struct stat info;
	str = strdup(path);

	stat(str, &info);

	if(!(info.st_mode & S_IFDIR)) //is file , will check permitions	
		if(!(info.st_mode & S_IRUSR) || !(info.st_mode & S_IRGRP) || !(info.st_mode & S_IROTH))
		{
			free(str);
			str = NULL;
			return NO_PERMITIONS;
		}	

	token = strtok(str, "/"); 
	if(token == NULL)
	{
	    free(str);
	    str = NULL;
	    return HAS_PERMITIONS;
	}
	strcpy(checkedParts,token);

	while( token != NULL ) //loop through the pass
	{		
		stat(checkedParts, &info);
		if((info.st_mode & S_IFDIR)) //is is dir , will check permitions	
			if(!(info.st_mode & S_IXUSR) || !(info.st_mode & S_IXGRP) || !(info.st_mode & S_IXOTH))
			{
				free(str);
				str = NULL;
				return NO_PERMITIONS;
			}	
		else if(!(info.st_mode & S_IRUSR) || !(info.st_mode & S_IRGRP) || !(info.st_mode & S_IROTH)) //if file
		{
			free(str);
			str = NULL;
			return NO_PERMITIONS;
		}
		token = strtok(NULL, "/");
		if(token != NULL)
		{
			strcat(checkedParts,"/");
			strcat(checkedParts,token);
		}
	}
	free(str);
	str = NULL;
	return HAS_PERMITIONS; 
}
//*****************************************************************************************//
//*****************************************************************************************//
//function that replace all %20 to spaces
void str_replace(char *target, const char *needle, const char *replacement)
{
    char buffer[STRING_LEN] = { 0 };
    char *insert_point = &buffer[0];
    const char *tmp = target;
    size_t needle_len = strlen(needle);
    size_t repl_len = strlen(replacement);

    while (1) 
	{
        const char *p = strstr(tmp, needle);
        if (p == NULL) {
            strcpy(insert_point, tmp);
            break;
        }
        memcpy(insert_point, tmp, p - tmp);
        insert_point += p - tmp;
        memcpy(insert_point, replacement, repl_len);
        insert_point += repl_len;
        tmp = p + needle_len;
    }
    strcpy(target, buffer);
}
//*****************************************************************************************//
//*****************************************************************************************//
char* analizePacketAndCreateResponse(char *packet, int *size)
{
	char *token, *str, *requestType, *path, *protocol, *response;
    struct stat info;
	str = strdup(packet);
	token = strtok(str, "\r\n");
    char *temp;
	if(token == NULL) 
	{
		response = badRequest(400,NULL);
		*size = RESPONSE_WITHOUT_FILE;
		return response;
	}
	requestType = strtok(str, " ");
	path = strtok(NULL," ");
	protocol = strtok(NULL," ");
    temp = strtok(NULL," ");
    //validating that the request is valid
	if(path == NULL || requestType == NULL || temp != NULL || protocol == NULL || ((strcmp(protocol,"HTTP/1.1") != EQUALS) && (strcmp(protocol,"HTTP/1.0") != EQUALS)))
	{
		response = badRequest(400,NULL);
		free(str);
		str = NULL;
		*size = RESPONSE_WITHOUT_FILE;
		return response;
	}
	if(strcmp(requestType,"GET") != EQUALS) //only get supported
	{
		response = badRequest(501,NULL);
		free(str);
		str = NULL;
		*size = RESPONSE_WITHOUT_FILE;
		return response;
	}
	
	str_replace(path,"%20"," "); //if has spaces, change %20 from browser to space
	
	char *newPath = path + 1; //remove first slash
	char *string = strdup(newPath); //same path for other use
	if((stat( newPath, &info ) == VALID_PATH) && (checkPermitions(newPath) == NO_PERMITIONS)) //check permitions if file found
		response = badRequest(403,NULL);

	else if((stat( newPath, &info ) != VALID_PATH) && (strlen(newPath) != 0)) //if file not found
		response = badRequest(404,NULL);

	else if((strlen(newPath) == 0) || (info.st_mode & S_IFDIR))  //if is folder
	{
		if((newPath[strlen(newPath)-1] != '/') && (strlen(newPath) != 0 )) //if no end slash given
			response = badRequest(302,newPath);
		else
		{	//if is a folder
			char *unchangedPath =(char*)malloc(sizeof(char)*(strlen(path)+1)+1);
			if(unchangedPath == NULL)
			{
				free(str);
				str = NULL;
				*size = -1;
				free(string);
				string = NULL;
				return NULL;
			}
			strcpy(unchangedPath,".");
			strcat(unchangedPath,path);
			unchangedPath[strlen(unchangedPath)] = '\0';

			char *pathWithIndex = (char*)malloc(sizeof(char)*(strlen(newPath)+10)+1);
			if(pathWithIndex == NULL)
			{
				free(str);
				str = NULL;
				*size = -1;
				free(string);
				string = NULL;
				return NULL;
			}
			strcpy(pathWithIndex,newPath);
			strcat(pathWithIndex,"index.html");
			// no index html found, return content dir
			if(( stat( pathWithIndex, &info ) != VALID_PATH) && (checkPermitions(newPath) == HAS_PERMITIONS))
			{
				response = createDirResponse(unchangedPath);
				free(unchangedPath);
				unchangedPath = NULL;
				free(str);
				str = NULL;
				*size = RESPONSE_WITHOUT_FILE;
				free(string);
				string = NULL;
				free(pathWithIndex);
				pathWithIndex = NULL;
				if(response == NULL)
					return NULL;
				return response;
			}
			else if(checkPermitions(pathWithIndex) == HAS_PERMITIONS) //if index.html found
			{	
				*size = info.st_size;
				free(str);
				str = NULL;
				free(string);
				string = NULL;
				free(unchangedPath);
				unchangedPath = NULL;

				return pathWithIndex;
			}
			else
				response = badRequest(403,NULL);
		}		
	}    	
	else //if is a file
	{
		if(( checkPermitions(newPath) == HAS_PERMITIONS) && (S_ISREG(info.st_mode))) //if has permitions and the file is regular 
		{
			*size = info.st_size;
			free(str);
			str = NULL;
			return string;
		}
		else
			response = badRequest(403,NULL);		
	}
	free(string);
	string = NULL;
	free(str);
	str = NULL;
	*size = RESPONSE_WITHOUT_FILE;
	return response;
}
//*****************************************************************************************//
//*****************************************************************************************//
//main function that reads request from client and makes the response according to the given path
int handleRequest(void *arg)
{
	char buffer[STRING_LEN]; 
	int newsockfd = *(int*)arg;
	int recieve;
	char *response;    
	int size;
	char *path;
	char *file;
 	if((recieve=read(newsockfd, buffer, sizeof(buffer)))<0) //read request
	{
		perror("read");
		exit(ERROR);
	}
	buffer[recieve]='\0';
	response = analizePacketAndCreateResponse(buffer,&size);
	if(response == NULL) // if malloc error
		response = badRequest(500,NULL);

	if(size != -1) //if response includes a file, send the header and then send the file
	{
		path = createFileResponseHeader(response,&size);
		
		if((recieve = write(newsockfd,path, strlen(path)) < 0))
		{
			perror("write");
			exit(ERROR);
		}
		sendData(&newsockfd,response);

		free(path);
		path = NULL;
	}
	else //response contains a const part only (without file)
	{
		if((recieve = write(newsockfd,response, strlen(response)) < 0))
		{
			perror("write");
			exit(ERROR);
		}
	}
	free(response);
	response = NULL;
	close(newsockfd);
	return 0;
}
//*****************************************************************************************//
//*****************************************************************************************//
int checkIfStringIsNum(char *token) //function that checks if a string contains numbers only
{
    int i;
	for(i=0 ; i < strlen(token) ; i++)
		if(isdigit(token[i]) == 0)
			return NOT_A_NUMBER;
	return NUMBER; 
}
//*****************************************************************************************//
//*****************************************************************************************//
//main program for creating the server
int main(int argc, char **argv)
{ 
	struct sockaddr_in serv_addr; 
	struct sockaddr cli_addr; 
	int client = sizeof(serv_addr);   
	int sockfd;

   	if (argc != MAX_ARGS) //wrong terminal usage
   	{          
		printf("Usage: server <port> <pool-size> <max-number-of-request>\n");          
		exit(ERROR);
	}
	//validation of input from terminal
	if(checkIfStringIsNum(argv[1]) == NOT_A_NUMBER || checkIfStringIsNum(SIZE_OF_POOL) == NOT_A_NUMBER || checkIfStringIsNum(NUM_OF_REQUESTS) == NOT_A_NUMBER)
	{          
		printf("Usage: server <port> <pool-size> <max-number-of-request>\n");          
		exit(ERROR);
	}	
	int newsockfd[atoi(NUM_OF_REQUESTS)]; //holds the fd's for the threads

	threadpool *t = create_threadpool(atoi(SIZE_OF_POOL)); //creating the threads that will handle the requests
	if(t == NULL)
	{
		printf("Usage: server <port> <pool-size> <max-number-of-request>\n");          
		exit(ERROR);
	}

	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)  //creating the main socket
	{
		perror("socket");
		destroy_threadpool(t); 
		exit(ERROR);
	}
	//socket settings
	serv_addr.sin_family = AF_INET;  
	serv_addr.sin_addr.s_addr = INADDR_ANY; 
	serv_addr.sin_port = htons(atoi(argv[1]));   

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
	{
		perror("bind");
		destroy_threadpool(t);
		exit(ERROR);   
	}                

	if(listen(sockfd, 5) < 0) //listen for new connections
	{
		perror("listen");
		destroy_threadpool(t);
		close(sockfd);
		exit(ERROR);
	}
	int i;
	for(i = 0 ; i < atoi(NUM_OF_REQUESTS) ; i++) // loop for maximum request, each reaquest is handled by thread.
	{	
		if((newsockfd[i] = accept(sockfd, &cli_addr, (socklen_t *)&client)) < 0)
		{
			perror("accept");
			close(sockfd);
			exit(ERROR);
		}
		dispatch(t,handleRequest,&newsockfd[i]);	//dispatch work to threads..
	}
	destroy_threadpool(t);
	close(sockfd);
	return 0;		
}
