#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <limits.h>

char* directory;


int receive_int (int conn, size_t receive_s){

        int s = 0;
        int r ;
        char* p = (char*) &r;

        while( (s = recv(conn, p, receive_s, 0)) > 0){
                if( s == 0 ){
                        return -1;
                }
                p += s;
                receive_s -= s;
        }


        return r;

}

char* receive_message (int conn, size_t receive_s){

	int len = 0 ;
	int s;
	char buf[receive_s+1];
	char * data = 0x0;
        
	while ( (s = recv(conn, buf, receive_s, 0)) > 0 ) {
		buf[s] = 0x0 ;
		if(data == 0x0){
			data = strdup(buf);
			len = s;
		} else {
			data = realloc(data, len + s + 1);
			strncpy( data + len, buf, s);
			data[len + s] = 0x0;
			len += s;
		}

		if( strlen(data) == receive_s ){
			break;
		}
	}

	printf("final: %s\n",data);
	return data;
}


void send_message (int sock_fd, char* data, int len){
	
	int s = 0 ;
	while (len > 0 && (s = send(sock_fd, data, len, 0)) > 0) {
		data += s ;
		len -= s ;
	}

	return;
}

void send_error(int conn){
	int err_id = 1;
	send_message(conn, (char*) &err_id, sizeof(int));
	shutdown(conn, SHUT_WR);
}

int receive_and_write(char* file_path, int conn){

	FILE* fp = fopen(file_path, "wb");
	if(fp == NULL){
		perror("file open error\n");
		send_error(conn);
	}

	char buf[1024] = {0};
	int s = 0;
	int receive_s = 1024;
	while((s = recv(conn, buf, sizeof(buf) - 1, 0)) > 0){
		printf("%d | buf %s\n", receive_s, buf);
		buf[s] = 0x0;
		fwrite(buf, sizeof(buf), 1, fp);
	}

	fclose(fp);

	return 1;
}

int read_and_send (char* data, int conn){

        FILE* fp;
        fp = fopen(data, "rb");

        if( fp == NULL ){
		send_error(conn);
        }

        size_t r_result = 0;
        size_t s_result = 0;
        char* buffer_ptr = 0x0;

        while( !feof(fp) ){
		char buffer[1024] = { 0 };
		buffer_ptr = 0x0;
                r_result = fread( &buffer, 1, sizeof(buffer) - 1, fp) ;
		buffer[r_result + 1] = 0x0;
                buffer_ptr = strdup(buffer);
		while (r_result > 0 && (s_result = send( conn, buffer_ptr, r_result, 0)) > 0) {
                        buffer_ptr += s_result ;
                        r_result -= s_result ;
                }
        }

        if( fclose(fp) != 0 ){
                return -1;
        }

        return 0;

}

void put(int conn){


	int receive_s = 1;
	int len_file_name = receive_int(conn, receive_s);
	char* file_name = receive_message(conn, len_file_name);
	char* path = strdup(directory);
	path = realloc( path, strlen(file_name) + 1);
	strcat(path, "/");
	strcat(path, file_name);
	
 
	printf("%s\n", path);
	if( access(path, F_OK) == 0){
		perror("File already exists\n");
		int err_id = 1;
		send_message(conn, (char*) &err_id, sizeof(int));
		shutdown(conn, SHUT_WR);
	}

	receive_and_write(path, conn);


	
}
void get(int conn){


	int receive_s = 1;
	int len_file_name = receive_int(conn, receive_s);
	char* file_name = receive_message(conn, len_file_name);
	
	char* path = strdup(directory);
	path = realloc(path, strlen(file_name) + 1);
	strcat(path, "/");
	strcat(path, file_name);

			
	if( access (path, R_OK) == -1 ){
		perror("Can not read file\n");
		send_error(conn);	
	}

	struct stat sf;
	stat(file_name, &sf);

	if((sf.st_mode & S_IFMT) == S_IFLNK){
		perror("wrong file : S_IFLNK\n");
		send_error(conn);
	}

	if((sf.st_mode & S_IFMT) == S_IFDIR){
		perror("This is DIR : S_IFDIR\n");
		send_error(conn);
	}

	read_and_send(path, conn);
	shutdown(conn, SHUT_WR);

}

int list (int conn){

	
	struct dirent *ent;
	DIR* dir = opendir(directory);
	int file_cnt = 0;
	char* message = 0x0;
	
	for(;ent = readdir(dir);){
		
		if(strcmp(ent->d_name, ".") == 0){
			continue;
		}

		if(strcmp(ent->d_name, "..") ==0){
			continue;
		}

		if(ent->d_type == DT_LNK){
			continue;
		}

		if(ent->d_type == DT_DIR){
			continue;
		}


		if( !message ){
			message = (char*) malloc (sizeof(char) * strlen(ent->d_name));
			strcpy(message, ent->d_name);
			message = realloc(message, strlen(message) + 1);
			char newline = '\n';
			strcat(message, &newline);
		} else{
			int len = strlen(message);
			int s = strlen(ent->d_name);
			message = realloc(message, len + s + 1);
			strcat(message, ent->d_name);
			char newline = '\n';
			strcat(message, &newline);
		}
			

		file_cnt++;		

	}
	closedir(dir);
	if(file_cnt == 0){
       		char* data = "This is empty directory\n";
		message = (char*) malloc (sizeof(char) * strlen(data));
		strcpy(message, data);
	}

	send_message(conn, message, strlen(message));
	
	free(message);
	shutdown(conn, SHUT_WR);
	return 1;
}	


void*
worker_thread(void* arg){

	int* conn_ptr = (int*) arg;
	int conn = *conn_ptr;
	size_t receive_s = 1;
	char* r_data;
	int err_id = 0;
	int r = receive_int(conn, 1);
	printf(">%d\n",r) ;
	
	switch(r){
		case 1:
			list(conn);
			break;
		case 2:
			get(conn);
			break;
		case 3:
			put(conn);
			break;
	}


	close(conn);
	free(arg);
}


int make_socket(int listen_fd, struct sockaddr_in* address, int port_num){
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_fd == 0){
		perror("socket failed : ");
		exit(EXIT_FAILURE);
	}

	memset(address, '\0', sizeof(*address));
	address->sin_family = AF_INET;
	address->sin_addr.s_addr = INADDR_ANY;
	address->sin_port = htons(port_num);

	if (bind(listen_fd, (struct sockaddr *)address, sizeof(*address)) < 0) {
		perror("bind failed : ");
		exit(EXIT_FAILURE);
	}

	return listen_fd;
}


int main(int argc, char* argv[]){

	int opt;
	int listen_fd;	
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	int port_num;

	while( (opt = getopt(argc, argv, "p:d:")) != -1){

		switch(opt){

			case 'p':
				port_num = atoi(optarg);
				break;
			case 'd':
				directory = (char*) malloc(sizeof(char) * strlen(optarg));
				strcpy(directory, optarg);
				break;
			default:
				exit(1);
		}
	}




	listen_fd = make_socket(listen_fd, &address, port_num);

	DIR* dir;
	if( (dir = opendir(directory)) == NULL ){
		perror("can not open directory\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		if (listen(listen_fd, 16) < 0) {
			 perror("listen failed : ");
			 exit(EXIT_FAILURE);
		}


		int* new_socket = (int*) malloc(sizeof(int));
		*new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen) ;
		if (new_socket < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		
		pthread_t new_thread;
		if( pthread_create( &new_thread, NULL, worker_thread, (void*) new_socket) < 0){
			perror("thread error");
			exit(1);
		}


	}

}
	int listen_fd;	
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	int port_num;

	while( (opt = getopt(argc, argv, "p:d:")) != -1){

		switch(opt){

			case 'p':
				port_num = atoi(optarg);
				break;
			case 'd':
				directory = (char*) malloc(sizeof(char) * strlen(optarg));
				strcpy(directory, optarg);
				break;
			default:
				exit(1);
		}
	}




	listen_fd = make_socket(listen_fd, &address, port_num);

	DIR*
