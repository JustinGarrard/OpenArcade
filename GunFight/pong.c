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
#include <signal.h>     //sigint
#include "logging.h"
#include "pong_helper.h"

#define WIDTH 43
#define HEIGHT 21
#define LEDGE 1
#define REDGE WIDTH - 2

// Global variables recording the state of the game
// Position of bullets
int bullRX, bullRY, bullLX, bullLY;
// Movement of bullets
int dR, dL;
// Position of shooters
int shootLY, shootRY;
// Player scores
int scoreL, scoreR;
// Status of threaded point scored
int ptScored;
// Global variable to inform other threads of quitting
int die;
// Global file descriptor for cleaning up after a player quits
int s_fd;
// Global variable for increasing difficulty after a death
int level;

// Lock for bullR position
pthread_mutex_t bullR_pos_lock = PTHREAD_MUTEX_INITIALIZER;
// Lock for bullL position
pthread_mutex_t bullL_pos_lock = PTHREAD_MUTEX_INITIALIZER;
// Lock for ballR velocity
pthread_mutex_t bullR_vel_lock = PTHREAD_MUTEX_INITIALIZER;
// Lock for ballL velocity
pthread_mutex_t bullL_vel_lock = PTHREAD_MUTEX_INITIALIZER;
// Lock for shootLY
pthread_mutex_t shootLY_lock = PTHREAD_MUTEX_INITIALIZER;
// Lock for shootRY
pthread_mutex_t shootRY_lock = PTHREAD_MUTEX_INITIALIZER;

// ncurses window
WINDOW *win;

/* Draw the current game state to the screen
 * ballX: X position of the ball
 * ballY: Y position of the ball
 * padLY: Y position of the left paddle
 * padRY: Y position of the right paddle
 * scoreL: Score of the left player
 * scoreR: Score of the right player
 */
void draw(int bullRX, int bullRY, int bullLX, int bullLY,  int shootLY, int shootRY, int scoreL, int scoreR) {
    int y;
    /* 
    int x;
    for(x = 0; x < level; x++) {
      int loc = obstacles[level];
      for(y = loc; y < loc+8; y++){
        mvwaddch(win, y, WIDTH / 2, ACS_VLINE);
      }
    }
    */
    // Score
    mvwprintw(win, 1, WIDTH / 2 - 3, "%2d", scoreL);
    mvwprintw(win, 1, WIDTH / 2 + 2, "%d", scoreR);
    // Right Bullet
    pthread_mutex_lock(&bullR_pos_lock);
    mvwaddch(win, bullRY, bullRX, ACS_BLOCK);
    pthread_mutex_unlock(&bullR_pos_lock);
    // Left Bullet
    pthread_mutex_lock(&bullL_pos_lock);
    mvwaddch(win, bullLY, bullLX, ACS_BLOCK);
    pthread_mutex_unlock(&bullL_pos_lock);
    // Left shooter
    pthread_mutex_lock(&shootLY_lock);
    for(y = 1; y < HEIGHT - 1; y++) {
	int ch = (y >= shootLY - 2 && y <= shootLY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, LEDGE, ch);
    }
    pthread_mutex_unlock(&shootLY_lock);
    // Right shooter
    pthread_mutex_lock(&shootRY_lock);
    for(y = 1; y < HEIGHT - 1; y++) {
	int ch = (y >= shootRY - 2 && y <= shootRY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, REDGE, ch);
    }
    pthread_mutex_unlock(&shootRY_lock);
    // Print the virtual window (win) to the screen
    wrefresh(win);
    // Finally erase bullets for next time (allows ball to move before next refresh)
    pthread_mutex_lock(&bullL_pos_lock);
    mvwaddch(win, bullLY, bullLX, ' ');
    pthread_mutex_unlock(&bullL_pos_lock);
    
    pthread_mutex_lock(&bullR_pos_lock);
    mvwaddch(win, bullRY, bullRX, ' ');
    pthread_mutex_unlock(&bullR_pos_lock);
}

/* Return ball and paddles to starting positions
 * Horizontal direction of the ball is randomized
 */
void reset() {
    pthread_mutex_lock(&bullL_pos_lock);
    pthread_mutex_lock(&bullR_pos_lock);
    pthread_mutex_lock(&shootRY_lock);
    pthread_mutex_lock(&shootLY_lock);
    bullLX = LEDGE;
    bullRX = REDGE;
    shootLY = shootRY = bullLY = bullRY = HEIGHT / 2;
    pthread_mutex_unlock(&shootLY_lock);
    pthread_mutex_unlock(&shootRY_lock);
    pthread_mutex_unlock(&bullL_pos_lock);
    pthread_mutex_unlock(&bullR_pos_lock);
    // dx is randomly either -1 or 1
    pthread_mutex_lock(&bullR_vel_lock);
    dR = 0;
    pthread_mutex_unlock(&bullR_vel_lock);
    pthread_mutex_lock(&bullL_vel_lock);
    dL = 0;
    pthread_mutex_unlock(&bullL_vel_lock);
    // Draw to reset everything visually
    draw(bullRX, bullRY, bullLX, bullLY, shootLY, shootRY, scoreL, scoreR);
    //draw(bullRX, bullRY, bullLX, bullLY, shootLY, shootRY, scoreL, scoreR, obstacles);
}

/* Display a message with a 3 second countdown
 * This method blocks for the duration of the countdown
 * message: The text to display during the countdown
 */
void countdown(const char *message) {
    int h = 4;
    int w = strlen(message) + 4;
    WINDOW *popup = newwin(h, w, (LINES - h) / 2, (COLS - w) / 2);
    box(popup, 0, 0);
    mvwprintw(popup, 1, 2, message);
    int countdown;
    for(countdown = 3; countdown > 0; countdown--) {
        mvwprintw(popup, 2, w / 2, "%d", countdown);
        wrefresh(popup);
        sleep(1);
    }
    wclear(popup);
    wrefresh(popup);
    delwin(popup);
    shootLY = shootRY = bullLY = bullRY = HEIGHT / 2; // Wipe out any input that accumulated during the delay
}

/* Perform periodic game functions:
 * 1. Move the bullets and shooters
 * 2. Detect hits
 * 3. Detect scored points and react accordingly
 * 4. Draw updated game state to the screen
 */
void tock(int fd, int role) {
    // Move the left bullet
    pthread_mutex_lock(&bullL_pos_lock);
    bullLX += dL;
    bullLY = shootLY;
    pthread_mutex_unlock(&bullL_pos_lock);
    
    // Move the right bullet
    pthread_mutex_lock(&bullR_pos_lock);
    bullRX += dR;
    bullRY = shootRY;
    pthread_mutex_unlock(&bullR_pos_lock);
    
    //send event handler
    struct game_state params;
    char log_msg[1024];
    
    //Role = 0 is the host, on the right side of screen
    if(!role){
        params.bullX = bullRX;
        params.bullY = bullRY;
        params.dX = dR;
        params.shootY = shootRY;
        //sprintf(log_msg, "host_yPos: %d\n", shootLY);
        //w_log(log_msg, role);
        sendUpdate(fd, role, params);
    }
    else if(role){
        params.bullX = bullLX;
        params.bullY = bullLY;
        params.dX = dL;
        params.shootY = shootLY;
        //sprintf(log_msg, "client_yPos: %d\n", shootRY);
        //w_log(log_msg, role);
        sendUpdate(fd, role, params);

    }


    //This should pick A side and check if a hit occurred :(

    //Check if right shooter has hit left shooter
    //pthread_mutex_lock(&bullR_pos_lock);
    //pthread_mutex_lock(&shootLY_lock);
    if(bullRX == LEDGE && abs(bullRY - shootLY) <= 2){
        scoreR = (scoreR + 1) % 100;
	    params.shootY = -100;
        sprintf(log_msg, "%s\n", "host player scored");
        w_log(log_msg, role);
        sendUpdate(fd, role, params);
        reset();
	    countdown("SCORE -->");
    }
    //pthread_mutex_unlock(&bullR_pos_lock);
    //pthread_mutex_unlock(&shootLY_lock);
    
    //Check if right shooter has hit left shooter
    //pthread_mutex_lock(&bullL_pos_lock);
    //pthread_mutex_lock(&shootRY_lock);
    else if(bullLX == REDGE && abs(bullLY - shootRY) <= 2){
        scoreL = (scoreL + 1) % 100;
	    params.shootY = -100;
        sprintf(log_msg, "%s\n", "client player scored");
        w_log(log_msg, role);
        sendUpdate(fd, role, params);
        reset();
	    countdown("<-- SCORE");
    
    }
    //pthread_mutex_unlock(&bullL_pos_lock);
    //pthread_mutex_unlock(&shootRY_lock);

    //How do roles impact what's happening up here^^
 
    // Finally, redraw the current state
    draw(bullRX, bullRY, bullLX, bullLY, shootLY, shootRY, scoreL, scoreR);
    //draw(bullRX, bullRY, bullLX, bullLY, shootLY, shootRY, scoreL, scoreR, obstacles);
}

/* Listen to keyboard input
 * Updates global pad positions
 * Needs to sendUpdate
 */
void *listenInput(void *args) {
    struct listen_args temp = *((struct listen_args *) args);
    int sfd = temp.sock_fd;
    int role = temp.role;
    struct game_state params;
        
    while(!die) {
      if(!role){
        switch(getch()) {
            case KEY_UP: 
                params.bullX = bullRX;
                params.bullY = bullRY;
                params.dX = dR;

                pthread_mutex_lock(&shootRY_lock);
                shootRY--;
                pthread_mutex_unlock(&shootRY_lock);
	            params.shootY = shootRY;
                sendUpdate(sfd, role, params);
                break;
            case KEY_DOWN: 
                params.bullX = bullRX;
                params.bullY = bullRY;
                params.dX = dR;

                pthread_mutex_lock(&shootRY_lock);
                shootRY++;
                pthread_mutex_unlock(&shootRY_lock);
	            params.shootY = shootRY;
                sendUpdate(sfd, role, params);
                break;
            default: break;
	    }
      }
      else{
        switch(getch()) {
            case KEY_UP: 
                params.bullX = bullLX;
                params.bullY = bullLY;
                params.dX = dL;
                
                pthread_mutex_lock(&shootLY_lock);
                shootLY--;
                pthread_mutex_unlock(&shootLY_lock);
	            params.shootY = shootLY;
			    sendUpdate(sfd, role, params); 
			    break;
            case KEY_DOWN: 
                params.bullX = bullLX;
                params.bullY = bullLY;
                params.dX = dL;
                
                pthread_mutex_lock(&shootLY_lock);
                shootLY++;
                pthread_mutex_unlock(&shootLY_lock);
	            params.shootY = shootLY;
			    sendUpdate(sfd, role, params); 
			    break;
            default: break;
	    }
      }
	
    }	    
    return NULL;
}

void* listenRecv(void *args){
    struct listen_args temp = *((struct listen_args *) args);
    int sfd = temp.sock_fd;
    int role = temp.role;
    char log_msg[1024];

    //receive other player's game state
    struct game_state state;
    while(!die) {
        state = receiveUpdate(sfd, role);
        if(state.shootY == -1){
            sprintf(log_msg, "%s\n", "received terminate connection");
            w_log(log_msg, role);
            die = 1;
            return NULL;
        }
   
        else if(state.shootY == -100){
            sprintf(log_msg, "%s\n", "I scored!");
            w_log(log_msg, role);
            if(!role){
                ptScored = 1; 
                scoreR = (scoreR + 1) % 100;
            }
            else if(role){
                ptScored = 1; 
                scoreL = (scoreL + 1) % 100;
            }
        }
        else{
            // Client updates if ball on host side
            if(role){
                // Left side player updating right side's state
                pthread_mutex_lock(&shootRY_lock);
                shootRY = state.shootY;
                pthread_mutex_unlock(&shootRY_lock);

                pthread_mutex_lock(&bullR_pos_lock);
                bullRX = state.bullX;
                bullRY = state.bullY;
                pthread_mutex_unlock(&bullR_pos_lock);
               
                pthread_mutex_lock(&bullR_vel_lock);
                dR = state.dX;
                pthread_mutex_unlock(&bullR_vel_lock);

                /*_
                // Error correct by letting the closer player be correct
                if (state.bullX >= WIDTH/2){
                    pthread_mutex_lock(&ball_pos_lock);
                    ballX = state.bX;
                    ballY = state.bY;
                    pthread_mutex_unlock(&ball_pos_lock);
                    pthread_mutex_lock(&ball_vel_lock);
                    dx = state.b_dX;
                    dy = state.b_dY;
                    pthread_mutex_unlock(&ball_vel_lock);
                }
                */
            }
            else{
                // Right side player updating left side's state
                pthread_mutex_lock(&shootLY_lock);
                shootLY = state.shootY;
                pthread_mutex_unlock(&shootLY_lock);

                pthread_mutex_lock(&bullL_pos_lock);
                bullLX = state.bullX;
                bullLY = state.bullY;
                pthread_mutex_unlock(&bullL_pos_lock);
               
                pthread_mutex_lock(&bullL_vel_lock);
                dL = state.dX;
                pthread_mutex_unlock(&bullL_vel_lock);

                /*
                // Error correct by letting the closer player be correct
                if (state.bX < WIDTH/2){
                    pthread_mutex_lock(&ball_pos_lock);
                    ballX = state.bX;
                    ballY = state.bY;
                    pthread_mutex_unlock(&ball_pos_lock);
                    pthread_mutex_lock(&ball_vel_lock);
                    dx = state.b_dX;
                    dy = state.b_dY;
                    pthread_mutex_unlock(&ball_vel_lock);
                }
                */
            }
        }
        sprintf(log_msg, "received update-> shootY:%d, bullX:%d, bullY:%d, dX:%d\n", state.shootY, state.bullX, state.bullY, state.dX);
        w_log(log_msg, role);
    }
    return NULL;
}

void initNcurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    refresh();
    win = newwin(HEIGHT, WIDTH, (LINES - HEIGHT) / 2, (COLS - WIDTH) / 2);
    box(win, 0, 0);
    mvwaddch(win, 0, WIDTH / 2, ACS_TTEE);
    mvwaddch(win, HEIGHT-1, WIDTH / 2, ACS_BTEE);
}

void sigint_handler(int signal){
    w_log("received sigint, cleaning up\n", 0);

    // Send exit message
    struct game_state params;
    params.bullX = 0;
    params.bullY = 0;
    params.dX = 0;
    params.shootY = -1;
        
    sendUpdate(s_fd, 0, params);
    w_log("sent teardown\n", 0);
 
    endwin();
    exit(0);
}

int main(int argc, char *argv[]) {
    // Process args
    // refresh is clock rate in microseconds
    // This corresponds to the movement speed of the ball
    int refresh = -1;
    int role;
    int port;
    const char* host;

    die = 0;
    signal(SIGINT, sigint_handler);
    
    if(argc == 4) {
        char *difficulty = argv[3];
        if(strcmp(difficulty, "easy") == 0) refresh = 80000;
        else if(strcmp(difficulty, "medium") == 0) refresh = 40000;
        else if(strcmp(difficulty, "hard") == 0) refresh = 20000;
        else {
            printf("ERROR: Difficulty should be one of easy, medium, hard.\n");
            exit(1);
        }
        role = 0;
        port = atoi(argv[2]);
    } 
    else if(argc == 3){
        role = 1;
        port = atoi(argv[2]);
        host = argv[1];
    }
    else {
        printf("Usage: ./pong DIFFICULTY\n");
        exit(0);
    }
    
    // Initialize log file
    logfile_init(role);

    // Set up ncurses environment
    initNcurses();

    // Connect here
    struct connect_state state;
    state = setupConnect(host, port, role, (unsigned long)refresh);
    s_fd = state.fd;
    if(s_fd == -1){
        w_log("failed to setup connection\n", role);
    }

    //If we are the client, we want to set the refresh to the value returned by the connection setup
    char log_msg[1024];
    if(role){
        refresh = state.refr;
        sprintf(log_msg, "refresh rate: %d\n", refresh);
        w_log(log_msg, role);
    }
    // Set starting game state and display a countdown
    reset();
    countdown("Starting Game");
    
    // Listen to keyboard input in a background thread
    struct listen_args largs;
    largs.sock_fd = s_fd;
    largs.role = role;
    pthread_t pth;
    pthread_create(&pth, NULL, listenInput, (void*)&largs);

    // Need a thread for receiving other player's events
    pthread_t prec;
    pthread_create(&prec, NULL, listenRecv, (void*)&largs);

    // Main game loop executes tock() method every REFRESH microseconds
    struct timeval tv;
    while(!die) {
        gettimeofday(&tv,NULL);
        unsigned long before = 1000000 * tv.tv_sec + tv.tv_usec;
        if(ptScored == 1){
            if(role){
                reset();
                countdown("<--SCORE");
            }
            else if(!role){
                reset();
                countdown("SCORE-->");
            }
            ptScored = 0;
        }

        tock(s_fd ,role); // Update game state
        gettimeofday(&tv,NULL);
        unsigned long after = 1000000 * tv.tv_sec + tv.tv_usec;
        unsigned long toSleep = refresh - (after - before);
        // toSleep can sometimes be > refresh, e.g. countdown() is called during tock()
        // In that case it's MUCH bigger because of overflow!
        if(toSleep > refresh) toSleep = refresh;
        usleep(toSleep); // Sleep exactly as much as is necessary
    }
    
    // Clean up
    pthread_join(pth, NULL);
    endwin();
    return 0;
}
