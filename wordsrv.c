#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "socket.h"
#include "gameplay.h"


#ifndef PORT
    #define PORT 59741
#endif
#define MAX_QUEUE 5
#define TURN_MSG "It is your turn.\n"
#define GUESS_MSG "It is your turn. Your guess?\n"


void add_player(struct client **top, int fd, struct in_addr addr);
void remove_player(struct client **top, int fd);

/* These are some of the function prototypes that we used in our solution 
 * You are not required to write functions that match these prototypes, but
 * you may find the helpful when thinking about operations in your program.
 */
/* Send the message in outbuf to all clients */
//void broadcast(struct game_state *game, char *outbuf);
//void announce_turn(struct game_state *game);
//void announce_winner(struct game_state *game, struct client *winner);
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game);


/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;

/* send message in outbuf to all active clients
*/
void broadcast(struct game_state *game, char *outbuf, int maxfd){
	struct client *p;
	int cur_fd;
	fd_set allsetcpy;
	allsetcpy = allset;
	for(cur_fd = 0; cur_fd <= maxfd; cur_fd++){
		if(FD_ISSET(cur_fd, &allsetcpy)){
			for(p = game->head; p != NULL; p = p->next) {
				if(cur_fd == p->fd) {
					write(cur_fd, outbuf, strlen(outbuf));
				}
			}
		}
	}
}

/* prompt the player who's turn it is for their guess
*/
void ask_guess(struct game_state *game, int curfd, int maxfd){
	struct client *p;
	for(p = game->head; p != NULL; p = p->next) {
				if(curfd == p->fd) {
					char *guess_msg = GUESS_MSG;
					write(curfd, guess_msg, strlen(guess_msg));
				}
			}
}

/* announce to all active players the current game state
*/
void announce_game_state(struct game_state *game, int maxfd){
	char msg[MAX_MSG];
	status_message(msg, game);
	struct client *p;
	int cur_fd;
	fd_set allsetcpy;
	allsetcpy = allset;
	for(cur_fd = 0; cur_fd <= maxfd; cur_fd++){
		if(FD_ISSET(cur_fd, &allsetcpy)){
			for(p = game->head; p != NULL; p = p->next) {
				if(cur_fd == p->fd) {
					write(cur_fd, msg, strlen(msg));
				}
			}
		}
	}
	
}

/* announce to all active players the guess that just went through
*/
void announce_guess(struct game_state *game, int maxfd, char *guess){
	struct client *p;
	int cur_fd;
	fd_set allsetcpy;
	allsetcpy = allset;
	for(cur_fd = 0; cur_fd <= maxfd; cur_fd++){
		if(FD_ISSET(cur_fd, &allsetcpy)){
			for(p = game->head; p != NULL; p = p->next) {
				if(cur_fd == p->fd) {
					write(cur_fd, game->has_next_turn->name, strlen(game->has_next_turn->name));
					write(cur_fd, " guessed ", 9);
					write(cur_fd, guess, strlen(guess));
					write(cur_fd, "\n", 1);
				}
			}
		}
	}
	
}

/* announce to all active players that a new player has joined
*/
void announce_player(struct game_state *game, int maxfd, char *name){
	struct client *p;
	int cur_fd;
	fd_set allsetcpy;
	allsetcpy = allset;
	for(cur_fd = 0; cur_fd <= maxfd; cur_fd++){
		if(FD_ISSET(cur_fd, &allsetcpy)){
			for(p = game->head; p != NULL; p = p->next) {
				if(cur_fd == p->fd) {
					write(cur_fd, name, strlen(name));
					write(cur_fd, " has joined.\n", 13);
				}
			}
		}
	}
	
}

/* announce to all active players whose turn it is
*/
void announce_turn(struct game_state *game, int maxfd){
	struct client *p;
	int cur_fd;
	fd_set allsetcpy;
	allsetcpy = allset;
	for(cur_fd = 0; cur_fd <= maxfd; cur_fd++){
		if(FD_ISSET(cur_fd, &allsetcpy)){
			for(p = game->head; p != NULL; p = p->next) {
				if(cur_fd == p->fd) {
					write(p->fd, "It is ", 6);
					write(p->fd, game->has_next_turn->name, strlen(game->has_next_turn->name));
					write(p->fd, " 's turn.\n", 10);
				}
			}
		}
	}
	if(game->head != NULL){
		printf("It is %s's turn.\n", game->has_next_turn->name);
	}
}

/* advance the game state to the next player's turn
*/
void advance_turn(struct game_state *game){
	if(game->has_next_turn->next != NULL){
		game->has_next_turn = game->has_next_turn->next;
	}
	else{
		game->has_next_turn = game->head;
	}
}


/* return if the guess is in the current game's word
*/
int guess_in_word(struct game_state *game, char* guess){
	for (int i = 0; i < strlen(game->word); i++ ){
		if(guess[0] == game->word[i]){
			return 1;
		}
	}
	return 0;
}

/* read and process the name of new users
*/
int read_and_check_name(struct game_state *state, struct client *p, int fd, int num_to_read){
	struct client *j;
	p->in_ptr = p->inbuf;
	int name_set = 0;
	int num_read;
	while(name_set == 0){
	
		num_read = read(fd, p->in_ptr, num_to_read);
		printf("Read %d bytes\n", num_read);
		if (num_read <= 0){
			int dc_check = write(fd, "test\n", 5);
			if (dc_check <= 0){
				return 2;
			}
		}
		p->in_ptr += num_read;
		
		for(int i = 0; i < 20; i++){
			if (p->inbuf[i] == '\r' && p->inbuf[i + 1] == '\n'){
				p->inbuf[i] = '\0';
				
				for(j = state->head; j != NULL; j = j->next) {
					if(strcmp(p->inbuf, j->name) == 0){
						write(p->fd, "Name is already in use. Try again.\n", 35);
						return 0;
					}
				}
				
				strcpy(p->name, p->inbuf);
				if(strcmp(p->name, "") == 0){
					write(p->fd, "Invalid name, try again.\n", 25);
					return 0;
				}
				printf("Read new player name %s\n", p->name);
				return 1;
			}
		}
	}
	return 0;
	
}


/* convert a letter from a-z to 0-25 respectively
*/
int letter_to_value(char letter){
		return (int)letter - 97;
}

/* read and check if a guess from an active user is valid
*/
int read_and_check_guess(struct game_state *game, struct client *p, int fd, int num_to_read, char *guess){
	int guess_set = 0;
	int num_read;
	char *char_ptr;
	char readbuf[30];
	char_ptr = readbuf;
	while(guess_set == 0){
	
		num_read = read(fd, char_ptr, num_to_read);
		printf("Read %d bytes\n", num_read);
		if (num_read <= 0){
			int dc_check = write(fd, "test\n", 5);
			if (dc_check <= 0){
				return 2;
			}
		}
		char_ptr += num_read;
		
		for(int i = 0; i < 20; i++){
			if (readbuf[i] == '\r' && readbuf[i + 1] == '\n'){
				readbuf[i] = '\0';
				strcpy(guess, readbuf);
				
				if(strcmp(guess, "") == 0 || game->letters_guessed[letter_to_value(guess[0])] == 1 || strlen(guess) > 1){
					write(p->fd, "Invalid guess, try again.\n", 26);
					printf("Invalid guess from %s\n", p->name);
					return 0;
				}
				printf("Read guess %s\n", guess);
				return 1;
			}
		}
	}
	return 0;
	
}
	


/* check if the game is over and
* return with appropriate game ending code
* 0 - not over 
* 1 - out of guesses
* 2 - win
*/
int game_over_check(struct game_state *state){
	//int length_of_word = strlen(state->word);
	if(state->guesses_left == 0){
		return 1;
	}
	
	if(strcmp(state->word, state->guess) == 0){
		return 2;
	}
	return 0;
	
}

/* handle the output for different types of game overs
*/
void game_over_output(struct game_state *state, int maxfd, int curfd, int game_over_type){
	struct client *p;
	int cur_fd;
	fd_set allsetcpy;
	allsetcpy = allset;
	switch(game_over_type){
		case 1:
			for(cur_fd = 0; cur_fd <= maxfd; cur_fd++){
				if(FD_ISSET(cur_fd, &allsetcpy)){
					for(p = state->head; p != NULL; p = p->next) {
						if(cur_fd == p->fd) {
							write(cur_fd, "Out of guesses!\n", 16);
							write(cur_fd, "The word was ", 13);
							write(cur_fd, state->word, strlen(state->word));
							write(cur_fd, "\r\nGame over.\n", 13);
							write(cur_fd, "\r\nStarting new game...\n", 23);
							break;
							
						}
					}
				}
			}
			break;
			
		case 2:
			for(cur_fd = 0; cur_fd <= maxfd; cur_fd++){
				if(FD_ISSET(cur_fd, &allsetcpy)){
					for(p = state->head; p != NULL; p = p->next) {
						if(cur_fd == p->fd && cur_fd != curfd) {
							write(cur_fd, "The word was ", 13);
							write(cur_fd, state->word, strlen(state->word));
							write(cur_fd, "\r\nGame over. The winner is ", 27);
							write(cur_fd, state->has_next_turn->name, strlen(state->has_next_turn->name));
							write(cur_fd, "\r\nStarting new game...\n", 23);
							break;
						}
					}
				}
			}
			write(curfd, "The word was ", 13);
			write(curfd, state->word, strlen(state->word));
			write(curfd, "\r\nGame over! You Win!\n", 22);
			write(curfd, "Starting new game...\n", 21);
			break;
	}
}

/* process a guess from an active user
*/
void handle_guess(struct game_state *state, char *guess_letter){
	
	char* guess_cpy = guess_letter;
	guess_cpy[1] = '\0';
	int letter_in_word = 0;
	int length_of_word = strlen(state->word);
	state->letters_guessed[letter_to_value(guess_cpy[0])] = 1;
	
	for(int i = 0; i < length_of_word; i++){
		if(guess_cpy[0] == state->word[i]){
			letter_in_word = 1;
			state->guess[i] = guess_cpy[0];
			printf("Letter %c is in the word.\n", guess_cpy[0]);
			
		}
	}
	if (letter_in_word == 0){
		state->guesses_left -= 1;
		printf("Letter %c is not in the word.\n", guess_cpy[0]);
	}
	
}

/* Add a client to the head of the linked list
 */
void add_player(struct client **top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));

    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->name[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *top;
    *top = p;
}

/* Removes client from the linked list and closes its socket.
 * Also removes socket descriptor from allset 
 */
void remove_player(struct client **top, int fd) {
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}

// check if the file descriptor is in the list of active players
int fd_in_active_players(struct game_state *game, int fd){
	struct client *p;
	for (p = game->head; p!= NULL; p = p->next){
		if (p->fd == fd){
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;
	

	// Add the following code to main in wordsrv.c:
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	if(sigaction(SIGPIPE, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
    
    if(argc != 2){
        fprintf(stderr,"Usage: %s <dictionary filename>\n", argv[0]);
        exit(1);
    }
    
    // Create and initialize the game state
    struct game_state game;

    srandom((unsigned int)time(NULL));
    // Set up the file pointer outside of init_game because we want to 
    // just rewind the file when we need to pick a new word
    game.dict.fp = NULL;
    game.dict.size = get_file_length(argv[1]);

    init_game(&game, argv[1]);
    
    // head and has_next_turn also don't change when a subsequent game is
    // started so we initialize them here.
    game.head = NULL;
    game.has_next_turn = NULL;
    
    /* A list of client who have not yet entered their name.  This list is
     * kept separate from the list of active players in the game, because
     * until the new playrs have entered a name, they should not have a turn
     * or receive broadcast messages.  In other words, they can't play until
     * they have a name.
     */
    struct client *new_players = NULL;
    
    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, MAX_QUEUE);
    
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_player(&new_players, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if(write(clientfd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_player(&new_players, clientfd);
            };
        }
        
        /* Check which other socket descriptors have something ready to read.
         * The reason we iterate over the rset descriptors at the top level and
         * search through the two lists of clients each time is that it is
         * possible that a client will be removed in the middle of one of the
         * operations. This is also why we call break after handling the input.
         * If a client has been removed the loop variables may not longer be 
         * valid.
         */
        int cur_fd;
        for(cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
            if(FD_ISSET(cur_fd, &rset)) {
                // Check if this socket descriptor is an active player
                
                for(p = game.head; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
						
						
                        //TODO - handle input from an active client
						
						char guess[2];
						int guess_set = 0;
						char trash_buf[10];
						
						// reading a guess
						if(cur_fd == game.has_next_turn->fd){
							guess_set = read_and_check_guess(&game, p, cur_fd, 30, guess);
						}
						// reading a guess or disconnecting when it is not your turn
						else if(fd_in_active_players(&game, cur_fd) == 1){
							int dc_check = write(cur_fd, "It is not your turn.\n", 21);
							if (dc_check <= 0){
								broadcast(&game, p->name, maxfd);
								broadcast(&game, " has disconnected.\n", maxfd);
								printf("%s has disconnected.\n", p->name);
								remove_player(&game.head, p->fd);
								
							}
							read(cur_fd, trash_buf, 10);
							
						}
						
						// a valid guess was made
						if(guess_set == 1){
							handle_guess(&game, guess);
							
							
							
							announce_guess(&game, maxfd, guess);
							announce_game_state(&game, maxfd);
							
							//check for game over
							if(game_over_check(&game) != 0){
								// gameover actions
								
								game_over_output(&game, maxfd, cur_fd, game_over_check(&game));
								init_game(&game, argv[1]);
								announce_game_state(&game, maxfd);
							}
							
							// guess was not in word
							if(guess_in_word(&game, guess) == 0){
								advance_turn(&game);
							}
							announce_turn(&game, maxfd);
							
							ask_guess(&game, game.has_next_turn->fd, maxfd);
							
						}
						// an error occured when reading the guess, or the user disconnected during their turn
						else if(guess_set == 2){
							
							
							broadcast(&game, game.has_next_turn->name, maxfd);
							broadcast(&game, " has disconnected.\n", maxfd);
							printf("%s has disconnected.\n", game.has_next_turn->name);
							advance_turn(&game);
							remove_player(&game.head, cur_fd);
							if(game.has_next_turn == NULL){
								game.head = NULL;
								break;
							}
							announce_turn(&game, maxfd);
							ask_guess(&game, game.has_next_turn->fd, maxfd);
						}
						

						
						
                        
                        break;
                    }
                }
        
                // Check if any new players are entering their names
                for(p = new_players; p != NULL; p = p->next) {
					struct client *prev_p;
                    if(cur_fd == p->fd) {
                        // TODO - handle input from an new client who has
                        // not entered an acceptable name.
						
						// reading the name
						int name_read = 0;
						name_read = read_and_check_name(&game, p, cur_fd, MAX_NAME);
						if (name_read == 1){
							
							// giving this player the first turn if they are joining an empty server
							if (game.head == NULL){
								game.has_next_turn = p;
							}
							
							// initial input
							char msg[MAX_MSG];
							status_message(msg, &game);
							write(cur_fd, "To guess, type one letter and hit enter.\n", 41);
							write(cur_fd, "Inputs will only be accepted in this format.\n", 45);
							
							// removing from new players list without closing socket (one user)
							if(p->next == NULL){
								new_players = NULL;
							}
							
							// adding to active player list
							p->next = game.head;
							game.head = p;
							
							// announcing new player
							announce_player(&game, maxfd, p->name);
							printf("%s has joined the game.\n", p->name);
							write(cur_fd, msg, strlen(msg));
							announce_turn(&game, maxfd);
							
							//first one in, display the guess message
							if(game.has_next_turn == p){
								char *guess_msg = GUESS_MSG;
								write(cur_fd, guess_msg, strlen(guess_msg));
							}
							
							// removing from new players list without closing socket (multiple users)
							if(prev_p != NULL){
								prev_p->next = p->next;
							}
						}
                        break;
                    }
					prev_p = p;
                }
            }
        }
    }
    return 0;
}


