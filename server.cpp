/*
 * server.cpp
 *
 *  Created on: 21 Jan 2012
 *      Author: nebril
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <time.h>
#include <iostream>

using namespace std;

#define MYPORT 3490

#define BACKLOG 10

#define BUFFER_SIZE 1024
string getResponse(string &request);
int getFileContent(string &content, string path);


void sigChildHandler(int s) {
	while(wait(NULL) > 0);
}

int main() {
	int sockfd, newfd;

	struct sockaddr_in myAddr;
	struct sockaddr_in otherAddr;

	unsigned int sinSize;

	struct sigaction sa;

	int yes = 1;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) {
		perror("setsockopt");
		exit(1);
	}

	myAddr.sin_family = AF_INET;
	myAddr.sin_port = htons(MYPORT);
	myAddr.sin_addr.s_addr = INADDR_ANY;
	memset(&(myAddr.sin_zero), '\0', 8);

	if(bind(sockfd, (struct sockaddr * )&myAddr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if(listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigChildHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	while(1) {
		sinSize = sizeof(struct sockaddr_in);

		if((newfd = accept(sockfd, (struct sockaddr *)&otherAddr, &sinSize)) == -1) {
			perror("accept");
			continue;
		}

		printf("server: got connection from %s\n", inet_ntoa(otherAddr.sin_addr));
		if(!fork()) {
			close(sockfd);
			string request = string();
			char buffer[BUFFER_SIZE];

			int r = read(newfd, buffer, BUFFER_SIZE);
			if(r == -1) {
				perror("read");
			}
			request += string(buffer).substr(0, r);

			string response = getResponse(request);
			//cout<<response;
			if(send(newfd, response.c_str(), response.length(), 0) == -1) {
				perror("send");
			}
			close(newfd);
			exit(0);
		}
		close(newfd);
	}

	return 0;
}

string getResponse(string &request) {
	string response = "HTTP/1.0 ";
	string content;
	if(!request.substr(0,3).compare("GET")) {
		string path = request.substr(5, request.find(' ', 5) - 5);

		if(path.find('.') == string::npos) {
			if(path.compare("")) {
				path += "/";
			}
			path += "index.html";
		}

		int status;
		if((status = getFileContent(content, path)) == 200) {
			response += "200 OK\r\n";
		}else if(status == 403) {
			response += "403 Forbidden\r\n";
		}else if(status == 404) {
			response += "404 Not Found\r\n";
		}else {
			response += status;
			response += " Bad Request";
		}

	}else {
		response += "405 Method Not Allowed\r\n";
	}

	time_t rawtime;
	time(&rawtime);
	response += "Connection:Keep-Alive\r\nKeep-Alive:timeout=15, max=100\r\nContent-Type:text/html; charset=UTF-8\r\nTransfer-Encoding:chunked\r\nDate:";
	response.append(ctime(&rawtime));
	response += "\r\n";

	response += content;
	return response;
}

int getFileContent(string &content, string path) {
	FILE *fp;
	long len;
	char * buf;
	if((fp = fopen(path.c_str(),"r")) == NULL) {
		if(errno == ENOENT) {
			content = "File not found";
			return 404;
		}else if(errno == EACCES){
			content = "Access denied.";
			return 403;
		}else {
			content = "Bad Request";
			return 400;
		}
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp,0,SEEK_SET);
	buf = new char[len];
	fread(buf,len,1,fp);
	fclose(fp);

	content.append(buf);
	content = content.substr(0, len);

	delete buf;
	return 200;
}
