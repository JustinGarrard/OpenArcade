#include "logging.h"
#include "fight_helper.h"    //pong fns

int sendUpdate(int fd, int role, struct game_state params){
    char log_msg[1024];
    int ret = 1;
    struct game_state send_pars;
    send_pars.shootY = htonl(params.shootY);
    send_pars.bullX = htonl(params.bullX);
    send_pars.bullY = htonl(params.bullY);
    send_pars.dX = htonl(params.dX);

    if(send(fd, &send_pars, sizeof(send_pars), 0) == -1){
        sprintf(log_msg, "%s\n","could not send param update");
        w_log(log_msg, role);
        ret = -1;
    }
    sprintf(log_msg, "sent param update-> shootY:%d, bullX:%d, bullY:%d, dX:%d\n", params.shootY, params.bullX, params.bullY, params.dX);
    w_log(log_msg, role);
    return ret;
}

struct connect_state setupConnect(const char* host, int port, int role, unsigned long refrRate){
    //Have client connect
    int sock_fd;
    char log_msg[1024];
    struct connect_state state;
    state.fd = -1;
    
    if(role){
        struct hostent *hp;
        struct sockaddr_in sin;

        //Get IP from host name
        if( !(hp = gethostbyname(host)) ){
            sprintf(log_msg, "unkown host: %s\n", host);
            w_log(log_msg, role);
            return state;
        }
        sprintf(log_msg, "%s\n", "resolved host");
        w_log(log_msg, role);

        // Build address data structure
        bzero((char *)&sin, sizeof(sin));
        sin.sin_family = AF_INET;
        bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
        sin.sin_port = htons(port);

        // Create socket fd
        if( (sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            sprintf(log_msg, "%s\n", "failed to create socket");
            w_log(log_msg, role);
            return state;
        }
        sprintf(log_msg, "%s\n", "created socket");
        w_log(log_msg, role);

        // Connect to 'server'
        if(connect(sock_fd, (struct sockaddr*)&sin, sizeof(sin)) < 0){
            sprintf(log_msg, "%s\n", "failed to connect to socket");
            w_log(log_msg, role);
            return state;
        }
        sprintf(log_msg, "%s\n", "connected to socket");
        w_log(log_msg, role);
                
        state.fd = sock_fd;
        unsigned long rate;

        //Recv refrRate
        if(recv(sock_fd, &rate, sizeof(rate), 0) == -1){
            sprintf(log_msg, "%s\n", "failed to receive refresh rate");
            w_log(log_msg, role);
            close(sock_fd);
            return state;
        }
        state.refr = ntohl(rate);
        state.refr = (int)state.refr;
        return state;
    }

    else{
        int acc_fd;
        struct sockaddr_in serv_addr;
        struct sockaddr_in c_addr;
        int c_len = sizeof(c_addr);
        struct hostent *c_host;
        char *c_addr_ip;

        if( (sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            sprintf(log_msg, "%s\n", "failed to create socket");
            w_log(log_msg, role);
            return state;
        }
        sprintf(log_msg, "%s\n", "created socket");
        w_log(log_msg, role);

        //Set sockaddr_in properties
        memset((char *)&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        //Bind to port
        if(bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
            sprintf(log_msg, "failed to bind: %s\n", strerror(errno));
            w_log(log_msg, role);
            return state;
        }
        sprintf(log_msg, "%s\n", "bound to socket");
        w_log(log_msg, role);

        //Listen
        if(listen(sock_fd, 1) < 0){
            sprintf(log_msg, "%s\n", "failed to listen to socket");
            w_log(log_msg, role);
            return state;
        }
        sprintf(log_msg, "%s\n", "listening on socket");
        w_log(log_msg, role);

        //Accept connection
        if( (acc_fd = accept(sock_fd, (struct sockaddr *)&c_addr, (socklen_t *)&c_len)) < 0){
            sprintf(log_msg, "failed to accept connection: %s\n", strerror(errno));
            w_log(log_msg, role);
            return state;
        }
        sprintf(log_msg, "%s\n", "accepted connection");
        w_log(log_msg, role);

        //Resolve host
        if( (c_host = gethostbyaddr((const char *)&c_addr.sin_addr.s_addr, sizeof(c_addr.sin_addr.s_addr), AF_INET)) == NULL){
            sprintf(log_msg, "failed to gethostaddr: %s\n", strerror(errno));
            w_log(log_msg, role);
            return state;
        }
        sprintf(log_msg, "%s\n", "gothostaddr");
        w_log(log_msg, role);


        if( (c_addr_ip = inet_ntoa(c_addr.sin_addr)) == NULL){
            sprintf(log_msg, "failed to inet_ntoa client addr: %s\n", strerror(errno));
            w_log(log_msg, role);
            return state;
        } 
        sprintf(log_msg, "Connected to %s:%s\n", c_host->h_name, c_addr_ip);
        w_log(log_msg, role);

        state.fd = acc_fd;
        state.refr = -1;

        //Send refrRate
        unsigned long send_rate = htonl(refrRate);
        if(send(acc_fd, &send_rate, sizeof(send_rate), 0) == -1){
            sprintf(log_msg, "failed to send refresh rate %s\n", strerror(errno));
            w_log(log_msg, role);
            //send msg to tear down
            close(acc_fd);
            return state;
        }
        sprintf(log_msg, "sent refresh rate: %lu\n", refrRate);
        w_log(log_msg, role);

        return state;
    }
    return state;
}

struct game_state receiveUpdate(int fd, int role){
    struct game_state params;
    struct game_state loc_params;
    char log_msg[1024];
    
    //Recv new parameters
    if(recv(fd, &params, sizeof(params), 0) == -1){
        sprintf(log_msg, "%s\n", "failed to receive new parameters");
        w_log(log_msg, role);
        params.shootY = -1;
    }
    loc_params.shootY = ntohl(params.shootY);
    loc_params.bullX = ntohl(params.bullX);
    loc_params.bullY = ntohl(params.bullY);
    loc_params.dX = ntohl(params.dX);
    
    return loc_params;
}

