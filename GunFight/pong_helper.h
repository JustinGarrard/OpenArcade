#ifndef PONG_HELPER_H_
#define PONG_HELPER_H_ 

#include <ncurses.h>    //get keyboard input
#include <stdlib.h>
#include <pthread.h>    //threading
#include <unistd.h>
#include <string.h>     //mem stuff & strerror
#include <sys/time.h>   //timing
#include <arpa/inet.h>  //ntoa
#include <sys/socket.h> //uhhhh sockets
#include <netinet/in.h> //something useful for networking I'm sure
#include <netdb.h>      //probs same
#include <errno.h>      //errno
#include "logging.h"
#include "pong_helper.h"    //pong fns

struct listen_args{
    int sock_fd;
    int role;
};

struct connect_state{
    int fd;
    int refr;
};

//Send game_state to other player
struct game_state{
    int bullX, bullY;
    int dX;
    int shootY;
};

int sendUpdate(int fd, int role, struct game_state);

struct connect_state setupConnect(const char* host, int port, int role, unsigned long refrRate);

struct game_state receiveUpdate(int fd, int role);
#endif
