#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>

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
        char buf[receive_s + 1] ;
        char * data = 0x0 ;
        while ( (s = recv(conn, buf, receive_s, 0)) > 0 ) {
                buf[s + 1 ] = 0x0 ;
                if(data = 0x0){
                        data = strdup(buf);
                        len = s;
                } else {
                        data = realloc(data, len + s + 1);
                        strncpy( data + len, buf, s);
                        data[len + s] = 0x0;
                        len += s;
                }
        }

        return data;
}

void send_message (int sock_fd, char* data, int len){

	int s = 0 ;
        while (len > 0 && (s = send(sock_fd, data, len, 0)) > 0) {
                data += s ;
                len -= s ;
        }


}

void send_message_more (int sock_fd, char* data, int len){

        int s = 0 ;
        while (len > 0 && (s = send(sock_fd, data, len, MSG_MORE)) > 0) {
                data += s ;
                len -= s ;
        }


}

void send_error(int conn){
        int err_id = 1;
        send_message(conn, (char*) &err_id, sizeof(int));
        shutdown(conn, SHUT_WR);
}


void send_int (int sock_fd, int* in_data, int len){

	char* data = (char*) in_data;
        int s = 0 ;

        while (len > 0 && (s = send(sock_fd, data, len, 0)) > 0) {
                data += s ;
                len -= s ;
        }


}

int receive_and_write(char* file_path, int conn){

        FILE* fp = fopen(file_path, "wb");
        if(fp == NULL){
                perror("file open error\n");
                send_error(conn);
        }

        char buf[1024] = {0};
        int s = 0;
        while((s = recv(conn, buf, sizeof(buf), 0)) > 0){
                fwrite(buf, sizeof(buf), 1, fp);
        }

        fclose(fp);

        return 1;
}


int read_and_send (char* data, int conn){

        FILE* fp;
        fp = fopen(data, "rb");

        if( fp == NULL ){
               perror("file open error\n");
	       send_error(conn);
        }

        char* buffer_ptr = 0x0;

        while( feof(fp) == 0){
                char buffer[1024] = { 0 };
		char* buffer_ptr = 0x0;
		int s_result = 0;
		size_t	r_result = fread( &buffer, 1, sizeof(buffer)-1, fp) ;
                buffer[r_result + 1] = 0x0;
		buffer_ptr = buffer;
		//buffer_ptr = strdup(buffer);
                while (r_result > 0 && (s_result = send( conn, buffer_ptr, r_result, 0)) > 0) {
			buffer_ptr += s_result ;
                        r_result -= s_result ;
                }
        }
	
         fclose(fp);

        return 0;

}


void put(int conn, int* cmd_id, char* file_name){

	char* path = strdup("./");
	path = realloc(path, strlen(file_name));
	strcat(path, file_name);
	 //request
	int len_file_name = strlen(file_name);
	char* data = (char*) cmd_id;
	send_message(conn, data, 1);
	data = (char*) &len_file_name;
	send_message(conn, data, 1);
	send_message(conn, file_name, len_file_name);
	read_and_send(path, conn);
	shutdown(conn, SHUT_WR);
}

void get(int conn, int* cmd_id, char* file_name){

	//request
	int len_file_name = strlen(file_name);
	char* data = (char*) cmd_id;
	send_message(conn, data, 1);
	data = (char*) &len_file_name;
	send_message(conn, data, 1);
	send_message(conn, file_name, len_file_name);
	shutdown(conn, SHUT_WR);

	//take response
	char* path = strdup("./");
	path = realloc(path, strlen(file_name));
	strcat(path, file_name);

	receive_and_write(path, conn);
	close(conn);
}


void list(int conn, int* comm_id){

	// request
	int len ;
	char* data;
	data = (char*) comm_id;
	len = strlen(data) ;
	send_message(conn,data, len);
	shutdown(conn, SHUT_WR) ;

	// take response message
	char* receive_data = 0x0;
	receive_data = receive_message(conn, 1023);
	printf("%s\n", receive_data);

	close(conn);
}


int make_socket(int sock_fd, struct sockaddr_in* serv_addr, char* ip_addr, int port_num ){

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sock_fd <= 0){
		perror("socket failed : ");
		exit(EXIT_FAILURE);
	}

	memset(serv_addr, '\0', sizeof(*serv_addr));
	serv_addr->sin_family = AF_INET;
	serv_addr->sin_port = htons(port_num);

	if(inet_pton(AF_INET, ip_addr, &serv_addr->sin_addr) <= 0){
		perror("inet_pton failed: ");
		exit(EXIT_FAILURE);
	}

	if(connect(sock_fd, (struct sockaddr *) serv_addr, sizeof(*serv_addr)) < 0){
		perror("connect faild : ");
		exit(EXIT_FAILURE);
	}

	return sock_fd;
}

void check_opt(int argc, char* argv[], int* cmd_id){

	if( argc < 3 || argc > 4 ){
		printf("invalid argument\n");
		exit(1);
	}
	
	if( argc == 3 ){
		if( strcmp(argv[2], "list") == 0){
			*cmd_id = 1;
		}  else {
			printf("invalid argument.\n");
			exit(EXIT_FAILURE);
		}
	 } else if ( argc == 4 ){

		if(strcmp(argv[2], "get") == 0){
			*cmd_id = 2;
		} else if (strcmp(argv[2], "put") == 0){
			*cmd_id = 3;

		} else {
			printf("invalid argument.\n");
			exit(EXIT_FAILURE);
		}
	 } else {
		 printf("invalid argument.\n");
		 exit(EXIT_FAILURE);
	 }

	return;

}


int main (int argc, char* argv[]) {

	
	char* ip_addr = 0x0;
	char* port = 0x0;
	int port_num = 0;
	int cmd_id = 0;

	if( argc < 3 || argc > 4 ){
		printf("invalid argument\n");
		exit(EXIT_FAILURE);
	}
	
	check_opt(argc, argv, &cmd_id);
	printf("%d\n",cmd_id);
	char* nptr = 0x0;
	char* ptr = strtok_r(argv[1], ":", &nptr);
	ip_addr = (char*) malloc(sizeof(char) * strlen(ptr));
	strcpy(ip_addr, ptr);
	sscanf(nptr, "%d", &port_num);

	int sock_fd = 0;
	struct sockaddr_in serv_addr;
	sock_fd = make_socket(sock_fd, &serv_addr, ip_addr, port_num);
	

	char* file_name = 0x0;

	if(cmd_id == 1){
		list(sock_fd, &cmd_id);
	}
	if(cmd_id == 2 || cmd_id == 3){

		file_name = (char*) malloc(sizeof(char) * strlen(argv[3]));
		strcpy(file_name, argv[3]);
		printf("file_name : %s\n", file_name);
		
		if(cmd_id == 2){
			get(sock_fd, &cmd_id, file_name);
		}
		else if (cmd_id == 3){
			put(sock_fd, &cmd_id, file_name);
		}
	}

	close(sock_fd);
	free(ip_addr);
	if(argc == 4){
		free(file_name);
	}
}


