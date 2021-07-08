# fshare

  - server: ./fshared -p <port_num> -d <shared directory name>
  - client: ./fshare \<ip address\>\:\<port_num\> \<command\> 
      - commmand 
          * list : list all files of shared directory
          * get <file_name> : clients request to download specific file in shared directory to server
          * put <file_name> : clients request to upload specific file in shared directory to server
  
          
