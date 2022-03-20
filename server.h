#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>


#define MAX_PLAYER 50
#define BUFFER_SZ 2048
#define MAX_ROOM_NUMBER 5
#define MAX_PLAYER_IN_ROOMS 10


typedef struct {
	char race[10];
	char class[10];
	char gender[10];
	int LVL;  
	int health;
	//int stamina;
	int strength;
	int dexterity;
	int intelligence;
	int damage;
	int gold;
} character_st;

typedef struct {
	character_st *character;
	int connfd;					//Connection file descriptor
	char *nick;					//Player name
	int id;						//User identifier
	int room_id;				//group -1 is loby, update when group change
	int GM;						//if GM 1 if not 0
	int muted;					//GM can toggle it 1 to mute player 0 to unmute
	int ready;					//When GM starts the game it toggles 1 for GM
	int dice;
} player_st;

typedef struct {
	char name[50];
	int LVL;                
	int health;
	int damage;
	int gold_drop;          //Monster drop loot when it dies
	int ready;
} monster_st;


int roundi(float number);									//round to an integer nuber

int eliminate_r_and_n_at_the_end(char *s);

void add_players_to_array(player_st *player);

void delete_players_from_array(int id);

void server_to_the_player_self(char *s, int connfd);		//Server to The Client

void server_to_all_players(char *s);						//Server to all Clients

void server_to_room(char *s, int room_id);					//Server to the Clients in a specific room

void send_message_to_room(char *s, int id, int room_id);	//Client to the Clients in a specific room

int dc(int id, int room_id);								//Disconnect a plater from room

void close_room(int room_id);

void create_room(player_st *player ,char *name, char *password);

int join_room(player_st *player, int room_id, char *password);

void show_rooms(int connfd);				//Show the list of rooms

int list_room(int room_id, int connfd);		//Show the players in the room 

int mute_player(int room_id, int id);		//Mute a player only GM can mute

int unmute_player(int room_id, int id);		//Unmute a muted player only GM can unmute

int kick_player(int room_id, int id);		//Kick player from the room only GM can kick

void show_player(int connfd, player_st *player);	//Show Player's stats and informations 

void show_monster(int room_id, char *s);	//Show Monster's stats and informations

void choose_class(player_st *player);

void choose_race(player_st *player);

void choose_gender(player_st *player);

void lvlup(player_st *player);

void create_character(player_st *player);

void check_start(player_st *player);		//Check if GM give </start> command

void random_monster_generator(int room_id);	//Generate a monster according to players in the room

int random_event_generator(int room_id);	//Generate a random number

int count_room(int room_id);				//Count how many player are there in the room

int ready_check(int room_id);				//Check if the players ready or not

int alive_check(int room_id);				//Check the players in the room if there is alive return 1

void ready_room(int room_id);				//Make the player in the room ready

void unready_room(int room_id);				//Make the player in the room unready

void unroll_room(int room_id);				//Reset rolled die in the room

int roll_a_dice();							//Roll a 6 faced dice

int dice_buff(int dice);					//Turn rolled dice number to damage

int fight_handler(int room_id);				

void *player_handler(void *arg);			

