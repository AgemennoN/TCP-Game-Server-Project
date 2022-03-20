#include "server.h"

char *room_names[MAX_ROOM_NUMBER];
char *room_passwords[MAX_ROOM_NUMBER];
char event[MAX_ROOM_NUMBER][BUFFER_SZ];

static _Atomic unsigned int player_counter = 0; // Uninterruptible variable
static int g_id = 1; // Global variable id
	
player_st *player_array[MAX_PLAYER];
player_st *rooms[MAX_ROOM_NUMBER][MAX_PLAYER_IN_ROOMS]; 

monster_st *monster_array[MAX_ROOM_NUMBER];

int roundi(float number){
	return number + 0.5;
}

int eliminate_r_and_n_at_the_end(char *s){
	int i;
	for (i = 0; s[i] != '\0'; i++ ){          //eliminate "\r\n at the end"
		if (s[i] == '\n' || s[i] == '\r'){
			s[i] = '\0';
			break;
		}
	}
	return i;
}

void add_players_to_array(player_st *player){
	int i;
	for (i = 0; i < MAX_PLAYER; i++) {
		if (!player_array[i]) {
			player_array[i] = player;
			break;
		}
	}
}

void delete_players_from_array(int id){
	int i;
	for (i = 0; i < MAX_PLAYER; i++) {
		if(player_array[i]){
			if (player_array[i]->id == id) {
				player_array[i] = NULL;
				break;
			}
		}
	}
}

void server_to_the_player_self(char *s, int connfd){
	write(connfd, s, strlen(s));
}

void server_to_all_players(char *s){
	int i;
	for (i = 0; i < player_counter; i++){
		if (player_array[i]){
			write(player_array[i]->connfd, s, strlen(s));
		}
	}
}

void server_to_room(char *s, int room_id){
	int i;
	for (i = 0; i < player_counter; i++){
		if (player_array[i]){
			if(player_array[i]->room_id == room_id){
				write(player_array[i]->connfd, s, strlen(s));
			}
		}
	}
}

void send_message_to_room(char *s, int id, int room_id){
	int i;
	if (room_id == -1){      // To loby only
		for (i = 0; i < MAX_PLAYER; i++){
			if (player_array[i]){                       //if player exist
				if (player_array[i]->room_id == -1){    //to send only the players in the loby
					if (player_array[i]->id != id){     //to not send the sender
						write(player_array[i]->connfd, s, strlen(s));
					}
				}
			}
		}   
	}
	else if (room_id >= 0){                   // To room
		for (i = 0; i < MAX_PLAYER_IN_ROOMS; i++){
			if (rooms[room_id][i]){                     //if player exist
				if (rooms[room_id][i]->id != id){       //to not send the sender;
					write(rooms[room_id][i]->connfd, s, strlen(s));
				}
			}
		} 
	}
}

int dc(int id, int room_id){
	int i;
	if(room_id == -1){
		return -1;      //you can't DC from lobby if you want to quit /quit
	}
	else{
		for (i = 0; i < MAX_PLAYER_IN_ROOMS; i++){
			if (rooms[room_id][i]){              //if player exist
				if (rooms[room_id][i]->id == id){//find the player
					rooms[room_id][i]->room_id = -1;
					rooms[room_id][i]->GM = 0;
					rooms[room_id][i]->ready = 0;
					rooms[room_id][i]->muted = 0;
					rooms[room_id][i]->dice = 0;
					rooms[room_id][i]->character = NULL;
					rooms[room_id][i] = NULL;
					if (i == 0){                //if the player is GM
						close_room(room_id);
					}
					return 0;
				}
			}
		}
		return 1;       //the person does not exist
	}    
}

void close_room(int room_id){
	int i;
	for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){             //if player exist
			server_to_the_player_self("GM has been disconnected, Room has been closed\r\n\n",rooms[room_id][i]->connfd);
			dc(rooms[room_id][i]->id, room_id);
		}
	}
	room_names[room_id] = NULL;
	room_passwords[room_id] = NULL;
	monster_array[room_id] = NULL;
}

void create_room(player_st *player ,char *name, char *password){
	int room_id;
	int flag = 0;
	char buff_out[BUFFER_SZ];
	for (room_id = 0; room_id < MAX_ROOM_NUMBER; room_id++){
		if (!rooms[room_id][0]){
			flag = 1;

			player->GM = 1;
			player->room_id = room_id;
			rooms[room_id][0] = player;

			room_names[room_id] = malloc(sizeof(name));
			strcpy(room_names[room_id], name);

			sprintf(buff_out,"ROOM: [%d] %s has been created by [GM] %s\r\n",room_id, room_names[room_id], player->nick);
			server_to_room(buff_out, -1);       //-1 is lobby;
			


			if(password){           //if room has password
				room_passwords[room_id] = malloc(sizeof(password));
				strcpy(room_passwords[room_id], password);
			
				sprintf(buff_out,"ROOM: [%d] %s has been created with password %s\r\n",room_id, room_names[room_id], room_passwords[room_id]);
				server_to_the_player_self(buff_out, player->connfd);
				printf("%s",buff_out);              //prints in server;
			}
			else{                   //if room does not have password
				sprintf(buff_out,"ROOM: [%d] %s has been created\r\n",room_id, room_names[room_id]);
				server_to_the_player_self(buff_out, player->connfd);
				printf("%s",buff_out);              //prints in server;
			}

			sprintf(event[room_id], "Game haven't begun yet\r\n\n");

			server_to_the_player_self("To see GM comands /help_GM\r\n\n", player->connfd);
			break;
		}
	}
	if (flag == 0){ //There is no room for a new room;
		server_to_the_player_self("There is no room for a new room\n To see available rooms </rooms>\r\n\n", player->connfd);
	}
}

int join_room(player_st *player, int room_id, char *password){
	int i;
	int flag = 0;
	char buffer_out[BUFFER_SZ];

	if (rooms[room_id][0]){     //if the room exist
		if (room_passwords[room_id]){
			if (password){
				if (!strcmp(room_passwords[room_id], password)){
					for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
						if (!(rooms[room_id][i])){
							dc(player->id, player->room_id);    //leave the room before join another;
							
							sprintf(buffer_out,"%s has been joined\r\n\n",player->nick);
							server_to_room(buffer_out, room_id);

							player->room_id = room_id;
							rooms[room_id][i] = player;

							server_to_the_player_self("You have successfully joined\n\t To see player comands /help_p\r\n\n",player->connfd);
							break;
						}
						else{
							server_to_the_player_self("There is no room for a new Player\r\n\n",player->connfd);
						}
					}
				}
				else{
					server_to_the_player_self("Password does not match\r\n\n",player->connfd);   
				}
			}
			else{
				server_to_the_player_self("Password does not match\r\n\n",player->connfd);   
			}
		}
		else{               //if there is no password dont compare;
			for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
				if (!(rooms[room_id][i])){
					sprintf(buffer_out,"%s has been joined\r\n\n",player->nick);
					server_to_room(buffer_out,room_id);
					
					player->room_id = room_id;
					rooms[room_id][i] = player;

					server_to_the_player_self("To see player comands /help_p\r\n\n",player->connfd);
					break;
				}
				else{
					server_to_the_player_self("There is no room for a new Player\r\n\n",player->connfd);
				}
			}
		}
	}
	else{
		server_to_the_player_self("Room does not exist\r\n\n",player->connfd);
	}
}

void show_rooms(int connfd){
	int i,j;
	char buff_out[BUFFER_SZ];
	int p_counter;
	int flag = 0;
	for (i = 0; i < MAX_ROOM_NUMBER; i++){
		if(rooms[i][0]){
			flag = 1;   //Room exist
			p_counter = 0;
			for (j = 0; j<MAX_PLAYER_IN_ROOMS; j++){
				if (rooms[i][j]){
					p_counter++;}}      //count players in the room
			if (rooms[i][0]->ready == 1){
				sprintf(buff_out, " [%d] %s \tGM: %s with %d players [Started]\r\n", i, room_names[i], rooms[i][0]->nick, p_counter);
				server_to_the_player_self(buff_out, connfd);
			}
			else if (p_counter == MAX_PLAYER_IN_ROOMS) {
				sprintf(buff_out, " [%d] %s \tGM: %s with %d players [FULL]\r\n", i, room_names[i], rooms[i][0]->nick, p_counter);
				server_to_the_player_self(buff_out, connfd);
			}
			else if (room_passwords[i]){
				sprintf(buff_out, " [%d] %s \tGM: %s with %d players [Need Password]\r\n", i, room_names[i], rooms[i][0]->nick, p_counter);
				server_to_the_player_self(buff_out, connfd);
			}
			else{
				sprintf(buff_out, " [%d] %s \tGM: %s with %d players [Available]\r\n", i, room_names[i], rooms[i][0]->nick, p_counter);
				server_to_the_player_self(buff_out, connfd);
			}
		}
	}
	server_to_the_player_self("\n", connfd);
	if (flag == 0){
		server_to_the_player_self("There isn't any available room\nTo create one /create\r\n\n", connfd);
	}

}

int list_room(int room_id, int connfd){
	char buff_out[BUFFER_SZ];
	int i;
	int counter = 0;
	if (room_id == -1){             // LOBBY
		for (i = 0; i < MAX_PLAYER; i++){
			if(player_array[i]){
				if(player_array[i]->room_id == room_id){
					counter++;
					sprintf(buff_out, "[%d] %s\r\n", player_array[i]->id, player_array[i]->nick);
					server_to_the_player_self(buff_out, connfd);
				}
			}
		}
	}
	else{           //ROOM
		for (i = 0; i < MAX_PLAYER_IN_ROOMS; i++){
			if(rooms[room_id][i]){
				counter++;
				buff_out[0] = '\0';
				if (i == 0){
					sprintf(buff_out, "<GM>\t[%d] %s", rooms[room_id][i]->id, rooms[room_id][i]->nick);
				}
				else{
					sprintf(buff_out, "\t[%d] %s", rooms[room_id][i]->id, rooms[room_id][i]->nick);
					
					if (rooms[room_id][i]->ready == 1){
						strcat(buff_out, "\t[Ready]");
					}
					else if (rooms[room_id][i]->ready == 0){
						strcat(buff_out, "\t[Not Ready]");
					}
					if (rooms[room_id][i]->muted == 1){
						strcat(buff_out, "\t[MUTED]");
					}
					if (rooms[room_id][i]->dice != 0){
						char temp[BUFFER_SZ];
						sprintf(temp, "\tDice: %d", rooms[room_id][i]->dice);
						strcat(buff_out, temp);
					}
					if(rooms[room_id][i]->character){
						if(rooms[room_id][i]->character->health <=0){
							strcat(buff_out, "\t[DEAD]");
						}
					}
					
				}
				strcat(buff_out, "\n");
				server_to_the_player_self(buff_out, connfd);
			}
		}
	}
	server_to_the_player_self("\r\n", connfd);
	return counter;
}

int mute_player(int room_id, int id){       //return 1 if player is already muted
	int i;
	for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){
			if (rooms[room_id][i]->id == id){
				if (rooms[room_id][i]->muted == 0){
					rooms[room_id][i]->muted = 1;
					server_to_the_player_self("You are Muted by GM\r\n\n", rooms[room_id][i]->connfd);
					return 0;
				}
				else{
					return 1;
				}
			} 
		}
	}
	return -1;      //Player with that id is not in the room
}

int unmute_player(int room_id, int id){       //return 1 if player is already muted
	int i;
	for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){
			if (rooms[room_id][i]->id == id){
				if (rooms[room_id][i]->muted == 1){
					rooms[room_id][i]->muted = 0;
					server_to_the_player_self("You are Unmuted by GM\r\n\n", rooms[room_id][i]->connfd);
					return 0;
				}
				else{
					return 1;
				}
			} 
		}
	}
	return -1;      //Player with that id is not in the room
}

int kick_player(int room_id, int id){
	int i;
	for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){
			if (rooms[room_id][i]->id == id){
				server_to_the_player_self("You have been kicked by GM\r\n\n", rooms[room_id][i]->connfd);                
				dc(id, room_id);
				return 1;
			} 
		}
	}
	return 0;      //Player with that id is not in the room
}

void show_player(int connfd, player_st *player){
	char buff_out[BUFFER_SZ];
	char temp[BUFFER_SZ];
	buff_out[0] = '\0';
	temp[0] = '\0';
	sprintf(temp, "\r\n[%d] %s ", player->id, player->nick);
	strcat(buff_out, temp);

	if(player->character->health == 0){
		strcat(buff_out, "\t[Dead]");
	}
	else {
		sprintf(temp, "\tHP: %d", player->character->health);
		strcat(buff_out, temp);
	}

	if (player->muted == 1){
		strcat(buff_out, "\t[Muted]");
	}
	strcat(buff_out, "\r\n");

	sprintf(temp, "Race: %s\r\nClass: %s\r\nGender: %s\r\nLVL: %d\r\nint: %d\r\nstr: %d\r\ndex: %d\r\ndmg: %d\r\ngold: %d\r\n",
		player->character->race, 
		player->character->class, 
		player->character->gender, 
		player->character->LVL, 
		player->character->intelligence, 
		player->character->strength, 
		player->character->dexterity, 
		player->character->damage, 
		player->character->gold);
	strcat(buff_out, temp);
	strcat(buff_out, "\r\n");

	server_to_the_player_self(buff_out, connfd);

}

void show_monster(int room_id, char *s){
	
	if (monster_array[room_id]->health <= 0){
	sprintf(s, "Monster: %s\tHP: %d\t[DEAD]\nLVL: %d\t\nDMG: %d",
	 monster_array[room_id]->name,
	  monster_array[room_id]->health,
	   monster_array[room_id]->LVL,
	    monster_array[room_id]->damage);
	}
	else{
	sprintf(s, "Monster: %s\tHP: %d\nLVL: %d\t\nDMG: %d",
	 monster_array[room_id]->name,
	  monster_array[room_id]->health,
	   monster_array[room_id]->LVL,
	    monster_array[room_id]->damage);
	}
	strcat(s, "\r\n\n");
}

void choose_class(player_st *player){
	char buff_in[BUFFER_SZ];
	int readlen;
	server_to_the_player_self("Choose Your Class:", player->connfd);
	do{
		server_to_the_player_self("\n\t[1] Warrior\n\t[2] Wizard\n\t[3] Ranger\n\t[4] Cleric\n\t[5] Thief\r\n\n", player->connfd);
		readlen = read(player->connfd, buff_in, sizeof(buff_in) - 1);
		buff_in[readlen] = '\0';          
		eliminate_r_and_n_at_the_end(buff_in);
	} while (strcmp(buff_in, "1") && strcmp(buff_in, "2") && strcmp(buff_in, "3") && strcmp(buff_in, "4") && strcmp(buff_in, "5"));
	if (!strcmp(buff_in, "1")){
		strcpy(player->character->class, "Warrior");
	}
	else if (!strcmp(buff_in, "2")){
		strcpy(player->character->class, "Wizard");
	}
	else if (!strcmp(buff_in, "3")){
		strcpy(player->character->class, "Ranger");
	}
	else if (!strcmp(buff_in, "4")){
		strcpy(player->character->class, "Cleric");
	}
	else if (!strcmp(buff_in, "5")){
		strcpy(player->character->class, "Thief");
	}
}

void choose_race(player_st *player){
	char buff_in[BUFFER_SZ];
	int readlen;
	server_to_the_player_self("Choose Your Race:", player->connfd);
	do{
		server_to_the_player_self("\n\t[1] Human\n\t[2] Elf\n\t[3] Orc\n\t[4] Dwarf\n\t[5] Halfling\r\n\n", player->connfd);
		readlen = read(player->connfd, buff_in, sizeof(buff_in) - 1);
		buff_in[readlen] = '\0';          
		eliminate_r_and_n_at_the_end(buff_in);
	} while (strcmp(buff_in, "1") && strcmp(buff_in, "2") && strcmp(buff_in, "3") && strcmp(buff_in, "4") && strcmp(buff_in, "5"));
	if (!strcmp(buff_in, "1")){
		strcpy(player->character->race, "Human");
		player->character->health = 100;
		player->character->intelligence = 5;
		player->character->strength = 5;
		player->character->dexterity = 5;
	}
	else if (!strcmp(buff_in, "2")){
		strcpy(player->character->race, "Elf");
		player->character->health = 90;
		player->character->intelligence = 7;
		player->character->strength = 4;
		player->character->dexterity = 6;
	}
	else if (!strcmp(buff_in, "3")){
		strcpy(player->character->race, "Orc");
		player->character->health = 100;
		player->character->intelligence = 3;
		player->character->strength = 7;
		player->character->dexterity = 4;
	}
	else if (!strcmp(buff_in, "4")){
		strcpy(player->character->race, "Dwarf");
		player->character->health = 110;
		player->character->intelligence = 3;
		player->character->strength = 6;
		player->character->dexterity = 5;
	}
	else if (!strcmp(buff_in, "5")){
		strcpy(player->character->race, "Halfling");
		player->character->health = 80;
		player->character->intelligence = 5;
		player->character->strength = 2;
		player->character->dexterity = 9;
	}
}

void choose_gender(player_st *player){
	char buff_in[BUFFER_SZ];
	int readlen;
	server_to_the_player_self("Choose Your Gender:", player->connfd);

	do{
		server_to_the_player_self("\n\t[1] Male\n\t[2] Female\r\n\n", player->connfd);
		readlen = read(player->connfd, buff_in, sizeof(buff_in) - 1);
		buff_in[readlen] = '\0';          
		eliminate_r_and_n_at_the_end(buff_in);
	} while (strcmp(buff_in, "1") && strcmp(buff_in, "2"));

	if (!strcmp(buff_in, "1")){
		strcpy(player->character->gender, "Male");
	}
	else if (!strcmp(buff_in, "2")){
		strcpy(player->character->gender, "Female");
	}
}

void lvlup(player_st *player){
	player->character->LVL += 1;
	player->character->health += 10;
	if (!strcmp(player->character->class, "Warrior")){
		player->character->strength += 3;
		player->character->intelligence += 1;
		player->character->dexterity += 2;
		player->character->damage = roundi(player->character->strength*1 + player->character->intelligence*0.5 + player->character->dexterity*0.75);
	}
	else if (!strcmp(player->character->class, "Wizard")){
		player->character->strength += 1;
		player->character->intelligence += 4;
		player->character->dexterity += 1;
		player->character->damage = roundi(player->character->strength*0.5 + player->character->intelligence*1.25 + player->character->dexterity*0.75);
	}
	else if (!strcmp(player->character->class, "Ranger")){
		player->character->strength += 2;
		player->character->intelligence += 2;
		player->character->dexterity += 2;
		player->character->damage = roundi(player->character->strength*0.75 + player->character->intelligence*0.5 + player->character->dexterity*1);
	}
	else if (!strcmp(player->character->class, "Cleric")){
		player->character->strength += 2;
		player->character->intelligence += 3;
		player->character->dexterity += 1;
		player->character->damage = roundi(player->character->strength*1 + player->character->intelligence*1 + player->character->dexterity*0.25);
	}
	else if (!strcmp(player->character->class, "Thief")){
		player->character->strength += 2;
		player->character->intelligence += 1;
		player->character->dexterity += 3;
		player->character->damage = roundi(player->character->strength*0.75 + player->character->intelligence*0.5 + player->character->dexterity*1);
	}
}

void create_character(player_st *player){
	char buff_in[BUFFER_SZ];
	int readlen;
	int i;
	int temp_connfd;
	int ready[3] = {0};
	player->character = malloc(sizeof(character_st));

	//deaf_player(&temp_connfd, player);
	server_to_the_player_self("Create you character:\r\n", player->connfd);
	do{
		server_to_the_player_self("Type number to select: \n\t[1] Race\n\t[2] Class\n\t[3] Gender\n\t[4] To finish\r\n", player->connfd);
		readlen = read(player->connfd, buff_in, sizeof(buff_in) - 1);
		buff_in[readlen] = '\0';          
		eliminate_r_and_n_at_the_end(buff_in);

		if (!strcmp(buff_in, "1")){
			choose_race(player);
			ready[0] = 1;
			//show_char(player);
		}
		else if (!strcmp(buff_in, "2")){
			choose_class(player);
			ready[1] = 1;
			//show_char(player);
		}
		else if (!strcmp(buff_in, "3")){
			choose_gender(player);
			ready[2] = 1;
			//show_char(player);
		}
		else if (!strcmp(buff_in, "4")){
			if (ready[0] == 0){
				server_to_the_player_self("You need to choose Race\r\n", player->connfd);
				buff_in[0] = '\0';
			}
			if (ready[1] == 0){
				server_to_the_player_self("You need to choose Class\r\n", player->connfd);
				buff_in[0] = '\0';
			}
			if (ready[2] == 0){
				server_to_the_player_self("You need to choose Gender\r\n", player->connfd);
				buff_in[0] = '\0';
			}
		}
	} while (strcmp(buff_in, "4"));
	player->character->LVL = 0;
	player->character->gold = 50;
	lvlup(player);


	show_player(player->connfd, player);
	player->ready = 1;
	
	//undeaf_player(&temp_connfd, player);
}

void check_start(player_st *player){
	if (player->room_id >= 0){          //If in a room
		if (player->GM == 0){           //If not a GM
			if (rooms[player->room_id][0]->ready == 1){     //GM started the game
				if (!(player->character)){ //If don't have a character that means, The game has just begun
					create_character(player);
				}
			}
		}
	}
}

void random_monster_generator(int room_id){
	monster_st *monster;
	int i,cnt = 0;
	int LVL_ave = 0;
	monster = malloc(sizeof(monster_st));

	for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){
			LVL_ave += rooms[room_id][i]->character->LVL;
			cnt++;
		}
	}
	LVL_ave = LVL_ave/cnt;
	
	strcpy(monster->name, "CANAVAR");
	monster->LVL =  LVL_ave;
	monster->health = 15 + (20 * LVL_ave) + (cnt * 5) + roll_a_dice();
	monster->damage = 5 + (10 * LVL_ave) + (cnt * 5) + roll_a_dice();
	monster->gold_drop = 20 + LVL_ave * roll_a_dice();
	monster->ready = 0;

	monster_array[room_id] = monster;
}

int random_event_generator(int room_id){
	float p;
	int i;
	char temp[BUFFER_SZ];
	srand(time(NULL));
	p = rand()%100;
	if (p < 50.0){           // 50 Monster
		random_monster_generator(room_id);
		sprintf(event[room_id], "You encountered a Monster. Prepare to fight\r\n\n");
		
		show_monster(room_id, temp);
		strcat(event[room_id], temp);
		server_to_room(event[room_id], room_id);

		unready_room(room_id);

		return 1;
	}
	else if(p < 60.0){      //2 yol ayrımı  10 ihtimal 
		sprintf(event[room_id], "The road was parting to two ways\r\n\t[1] Left path\r\n\t[2] Right path\r\n~It doesn't matter which way and I haven't add voting yet :/\n But Have Fun with the monsters\n");
		server_to_room(event[room_id], room_id);

	//	unready_room(room_id);

		return 2;
	}
	else if(p < 75.0){      //15 shop ihtimal? 
		sprintf(event[room_id], "You arrived at a shop. It was empty. \r\n\tYou've decided to continue your journey\r\n\n");
		server_to_room(event[room_id], room_id);

		return 3;
	}
	else if(p < 90.0){		//15 boş temple 
		sprintf(event[room_id], "You arrived at a ancient temple. The temple was plundered.\r\n\tYou've decided to continue your journey\r\n\n");
		server_to_room(event[room_id], room_id);

		return 4;
	}
	else{                   //10 boş arazi 
		for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
			if (rooms[room_id][i]){
				rooms[room_id][i]->character->health +=  7;
			}
		}
		sprintf(event[room_id], "Nothing here. You rested for a while. \r\n\tNow you are Ready to continue \r\n\n");
		server_to_room(event[room_id], room_id);

		return 5;
	}
}

int count_room(int room_id){
	int i;
	int counter = 0;
	for (i = 0; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){			//if player exist
			counter++;
		}			
	}
	return counter;
}

int ready_check(int room_id){
	int i;
	int ready = 1;
	for (i = 0; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){			//if player exist
			if (rooms[room_id][i]->ready == 0){
				ready = 0;
				return ready;
			}
		}			
	}
	return ready;
}

int alive_check(int room_id){
	int i;
	int alive = 0;
	for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){			//if player exist
			if (rooms[room_id][i]->character->health > 0){
				alive = 1;
				return alive;
			}
		}			
	}
	return alive;
}

void ready_room(int room_id){
	int i;
	for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){			//if player exist
			rooms[room_id][i]->ready = 1;
		}			
	}
}

void unready_room(int room_id){
	int i;
	for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){			//if player exist
			if (rooms[room_id][i]->character){	//if has a char (probably has)
				if (rooms[room_id][i]->character->health <= 0){	//if dead can't be unready
					continue;
				}
				else{
					rooms[room_id][i]->ready = 0;
				}
			}
			else{						//might be useless don't know, don't care RN
				rooms[room_id][i]->ready = 0;
			}
		}			
	}
}

void unroll_room(int room_id){
	int i;
	for (i = 0; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){			//if player exist
			rooms[room_id][i]->dice = 0;
		}			
	}
}

int roll_a_dice(){
	int dice;
	srand(time(NULL));
	dice = 1 + rand()%6;
	return dice;
}

int dice_buff(int dice){
	return (dice - 3);
}


int fight_handler(int room_id){
	char temp[BUFFER_SZ];
	int i;
	int monsters_turn = 1;
	int counter1 = 0;		//counts player in the room
	int counter2 = 0;
	int attack_to_who;

	if (!alive_check(room_id)){
		sprintf(event[room_id], "Everyone dies eventually\tGG\n\tYou can quit Game won't start again\r\n\n");
		server_to_room(event[room_id], room_id);
		return(1);
	}

	for(i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
		if (rooms[room_id][i]){					//if player exist
			if (rooms[room_id][i]->character->health > 0){	//if char is alive
				counter1++;
				if (rooms[room_id][i]->ready == 0){	//if not played yet
					monsters_turn = 0;
					if (rooms[room_id][i]->dice == 0){
						sprintf(temp, "The turn is on [%d] %s.\n\t Please Roll a dice %s\r\n\n",
							rooms[room_id][i]->id,
							rooms[room_id][i]->nick,
							rooms[room_id][i]->nick);
						
						show_monster(room_id, event[room_id]);
						strcat(event[room_id], temp);
						server_to_room(event[room_id], room_id);

						break;
					}
					else {
						int true_damage = rooms[room_id][i]->character->damage + dice_buff(rooms[room_id][i]->dice);
						monster_array[room_id]->health -= true_damage;
						
						rooms[room_id][i]->ready = 1;
						rooms[room_id][i]->dice = 0;

						sprintf(temp, "%s atacked.\n\t Hit %d\r\n\n",
							rooms[room_id][i]->nick,
							true_damage);
						
						show_monster(room_id, event[room_id]);
						strcat(temp, event[room_id]);
						strcpy(event[room_id], temp);
						server_to_room(event[room_id], room_id);

						break;
					}	
				}
			}
		}
	}

	if (monsters_turn == 1){
		
		if (rooms[room_id][0]->dice == 0){
			sprintf(temp, "The turn is on Monster.\n\tPlease roll a dice [GM] %s\r\n\n", rooms[room_id][0]->nick);

			show_monster(room_id, event[room_id]);
			strcat(event[room_id], temp);
			server_to_room(event[room_id], room_id);
		}
		else{
			srand(time(NULL));
			attack_to_who = 1 + rand()%counter1;
			for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
				if (rooms[room_id][i]){								//if player exist
					if (rooms[room_id][i]->character->health > 0){	//if char is alive
						counter2++;
						if (attack_to_who == counter2){
							int true_damage = monster_array[room_id]->damage + dice_buff(rooms[room_id][0]->dice);
							rooms[room_id][i]->character->health -= true_damage;

							sprintf(temp, "Monster attacked %s.\n\t Hit %d\r\n\t\t%s HP: %d",
								rooms[room_id][i]->nick,
								true_damage,
								rooms[room_id][i]->nick,
								rooms[room_id][i]->character->health);
							
							if (rooms[room_id][i]->character->health <= 0){
								strcat(temp, "\t[DEAD]");
								strcat(temp, "\r\n\n");
								strcpy(event[room_id], temp);
								server_to_room(event[room_id], room_id);
								server_to_the_player_self("\t\t~~~~~YOU DIED~~~~~\n", rooms[room_id][i]->connfd);
								rooms[room_id][i]->ready = 1;
								rooms[room_id][i]->dice = 0;
							}
							else {
								strcat(temp, "\r\n\n");
								strcpy(event[room_id], temp);
								server_to_room(event[room_id], room_id);
							}


							monster_array[room_id]->ready = 1;
							rooms[room_id][0]->dice = 0;	
						}
					}
				}
			}
		}
	}
	
	if (monster_array[room_id]->health > 0){
		if (monster_array[room_id]->ready == 1){
			unready_room(room_id);
			monster_array[room_id]->ready = 0;
		}
	}
	else{
		sprintf(event[room_id], "Congratulations, You killed the Monster\n\tThe Path is open. You can continue\r\n\n");
		server_to_room(event[room_id], room_id);
		
		for(i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
			if (rooms[room_id][i]){					//if player exist
				if (rooms[room_id][i]->character->health > 0){	//if char is alive
					lvlup(rooms[room_id][i]);
					rooms[room_id][i]->character->gold += monster_array[room_id]->gold_drop/counter1;					
				}
			}
		}
		server_to_room("\t~Your LVL has increased~\n", room_id);
		ready_room(room_id);
		unroll_room(room_id);
		monster_array[room_id] = NULL;
	}
	return 0;
}


void *player_handler(void *arg){
	char buff_out[BUFFER_SZ];
	char buff_in[BUFFER_SZ/2];
	int readlen;
	int i;
	player_st *player = (player_st *)arg;

	player_counter++;

	server_to_the_player_self("\r\nWellcome To DND server\n Please Enter your Nick name (you can change later with /nick)\r\n", player->connfd);
	readlen = read(player->connfd, buff_in, sizeof(buff_in) - 1);
	buff_in[readlen] = '\0';          
	readlen = eliminate_r_and_n_at_the_end(buff_in);

	player->nick = malloc(sizeof(char)*readlen);
	strcpy(player->nick, buff_in);

	player->id = g_id++;
	player->room_id = -1;
	player->GM = 0;
	player->muted = 0;
	player->ready = 0;
	player->dice = 0;

	printf("A new player Entered the server ");
	printf(" referenced by [%d] %s\r\n", player->id, player->nick);

	server_to_the_player_self("\r\nTo see rooms /rooms\r\n", player->connfd);
	server_to_the_player_self("To create room as a GM /create\r\n", player->connfd);
	server_to_the_player_self("To join a room as a player /join\r\n", player->connfd);
	server_to_the_player_self("To see other commands /help\r\n\n", player->connfd);

	while(1){
		readlen = read(player->connfd, buff_in, sizeof(buff_in) - 1);
		buff_in[readlen] = '\0';
		for (i = 0; buff_in[i] != '\0'; i++ ){          //eliminate "\r\n at the end"
			if (buff_in[i] == '\n' || buff_in[i] == '\r' ){
				buff_in[i] = '\0';
			}
		}

		if (buff_in[0] == '/') {
			char *command, *parameter;
			command = strtok(buff_in," ");
			if (!strcmp(command, "/create") || !strcmp(command, "/c")){
				int i;
				parameter = strtok(NULL, " ");      //Room's name
				if (parameter){
					char *room_name = parameter;
					parameter = strtok(NULL, "\0");
					char *password = parameter;
					dc(player->id, player->room_id);    //leave the room before create another;
					create_room(player, room_name, password);
					
				}
				else{
					server_to_the_player_self("Enter a name for the room please\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/join") || !strcmp(command, "/j")){
				parameter = strtok(NULL, " ");
				if (parameter){
					int room_id = atoi(parameter);
					parameter = strtok(NULL, "\0");
					char *password = parameter;
					if (player->room_id == room_id){
						sprintf(buff_out, "You are already in room [%d] %s\r\n\n", room_id, room_names[room_id]);
						server_to_the_player_self(buff_out, player->connfd);
					}
					else{
						join_room(player, room_id, password);
					}
				}
				else{
					server_to_the_player_self("Enter the Room ID to join the room please\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/dc")){
				if (player->room_id == -1){
					server_to_the_player_self("You can't dc from the loby\n To quit type /quit\r\n\n", player->connfd);
				}
				else{
					int temp = player->room_id;
					sprintf(buff_out, "[%d] %s has been disconnected from room\r\n\n", player->id, player->nick);
					server_to_room(buff_out, temp);
					dc(player->id, player->room_id);
					server_to_the_player_self("You have been dissconnected from the room\n You are in the Loby\r\n\n", player->connfd);    
				}
			}

			else if (!strcmp(command, "/rooms")){
				show_rooms(player->connfd);
			}
			
			else if (!strcmp(command, "/list")){
				list_room(player->room_id, player->connfd);
			}

			else if (!strcmp(command, "/quit") || !strcmp(command, "/q")){
				int temp = player->room_id; 
				dc(player->id, player->room_id);
				printf("[%d] %s has left from the server\n", player->id, player->nick);
				sprintf(buff_out, "Player [%d] %s has left from the server\n", player->id, player->nick);
				server_to_room(buff_out, temp);
				
				break;
			}

			else if (!strcmp(command, "/nick")){
				parameter = strtok(NULL, "\0");
				if (parameter){
					char *name = parameter;
					char *temp = malloc(sizeof(player->nick));
					strcpy(temp, player->nick);

					strcpy(player->nick, name);
					sprintf(buff_out, "[%d] %s change his/her name as %s\r\n\n", player->id, temp, name);
					server_to_room(buff_out, player->room_id);
				}
				else{
					server_to_the_player_self("You need a name to be called by your friends.\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/help_GM") || !strcmp(command, "/help_gm")){
				server_to_the_player_self("\r\n", player->connfd);
				server_to_the_player_self("/start \t\t\tStart the game\r\n", player->connfd);
				server_to_the_player_self("/next /ne\t\tContinue to next event\r\n", player->connfd);
				server_to_the_player_self("/now /n\t\t\tShows what happening now\r\n", player->connfd);
				server_to_the_player_self("/who <ID>\t\tShows player's info \r\n", player->connfd); 
				server_to_the_player_self("/monster /m\t\tShows monster's info \r\n", player->connfd); 
			//    server_to_the_player_self("/ask_vote \t\task vote from the players\r\n", player->connfd);
				server_to_the_player_self("/force_ready /fr\tForce everyone to be ready\r\n", player->connfd);
				server_to_the_player_self("/kick <ID>\t\tKick a player via ID \r\n", player->connfd); 
				server_to_the_player_self("/mute <ID>\t\tMute player via ID \r\n", player->connfd); 
				server_to_the_player_self("/unmute <ID>\t\tUnmute player via ID \r\n", player->connfd); 
				server_to_the_player_self("/mute_all\t\tMute All players\r\n", player->connfd); 
				server_to_the_player_self("/unmute_all\t\tUnmute All players\r\n", player->connfd);
				server_to_the_player_self("/rname <new name>\tRename of the Room\r\n", player->connfd); 
				server_to_the_player_self("/pwd <new password>\tChange Password of the Room\r\n", player->connfd);
				server_to_the_player_self("\r\n", player->connfd);
			}

			else if (!strcmp(command, "/start")){
				if (player->GM == 1){
					if (count_room(player->room_id)-1){
						if (player->ready == 0){
							server_to_the_player_self("Game started\r\n\n", player->connfd);
							strcpy(event[player->room_id], "Players are customizing their characters\r\n\n");
							send_message_to_room("[GM]: Game is ready to start\n\t Press Enter to customize your Character\r\n\n", player->id, player->room_id);
							player->ready = 1;
						}
						else{
							server_to_the_player_self("Game has already started\r\n\n", player->connfd);
						}
					}
					else{
						server_to_the_player_self("You need at least 1 player to start\r\n\n", player->connfd);
					}
				}
				else{
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/next") || !strcmp(command, "/ne")){
				if (player->GM == 1){					
					if (monster_array[player->room_id]){	//if there is monster fight
						fight_handler(player->room_id);
					}
					else if(ready_check(player->room_id)){	//if not monster wait until everyone ready
						random_event_generator(player->room_id);
					}
					else{									//is everyone ready ?
						server_to_the_player_self("Not everyone in the room is Ready\r\n\n", player->connfd);
					}
				}
				else{
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/force_ready") || !strcmp(command, "/fr")){				
				if (player->GM == 1){
					ready_room(player->room_id);
				}
				else {
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/mute")){
				if (player->GM == 1){
					parameter = strtok(NULL, " ");
					if (parameter){
						int id = atoi(parameter);
						if (id == player->id){
							server_to_the_player_self("You can't be muted Thus you are the GM\r\n\n",player->connfd);
						}
						else{
							int error = mute_player(player->room_id, id); 
							if (error){
								if (error == 1){
									sprintf(buff_out, "[%d] have been already muted\n To unmute type /unmute <ID>\r\n\n", id);
									server_to_the_player_self(buff_out, player->connfd);
								}
								else if(error == -1){
									sprintf(buff_out, "There is no player with ID [%d] in this room\n To list players /list \r\n\n", id);
									server_to_the_player_self(buff_out, player->connfd);
								}
							}
							else {
								sprintf(buff_out, "[%d] is muted now\n To unmute type /unmute <ID>\r\n\n", id);
								server_to_the_player_self(buff_out, player->connfd);
							}
						}
					}
					else{
						server_to_the_player_self("Enter ID of the player you want to mute\n Type /list to see players\r\n\n",player->connfd);
					}
				}
				else {
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/unmute")){
				if (player->GM == 1){
					parameter = strtok(NULL, " ");
					if (parameter){
						int id = atoi(parameter);
						if (id == player->id){
							server_to_the_player_self("Thus you are the GM, Thou can speak\r\n\n",player->connfd);
						}
						else{
							int error = unmute_player(player->room_id, id); 
							if (error){
								if (error == 1){
									sprintf(buff_out, "[%d] have been already unmuted\n To mute type /mute <ID>\r\n\n", id);
									server_to_the_player_self(buff_out, player->connfd);
								}
								else if(error == -1){
									sprintf(buff_out, "There is no player with ID [%d] in this room\n To list players /list \r\n\n", id);
									server_to_the_player_self(buff_out, player->connfd);
								}
								
							}
							else {
								sprintf(buff_out, "[%d] is unmuted now\r\n\n", id);
								server_to_the_player_self(buff_out, player->connfd);
							}
						}
					}
					else{
						server_to_the_player_self("Enter ID of the player you want to unmute\n Type /list to see players\r\n\n",player->connfd);
					}
				}
				else {
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}
			
			else if (!strcmp(command, "/mute_all")){
				if (player->GM == 1){
					for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
						if (rooms[player->room_id][i]){ //if player exist
							mute_player(player->room_id, rooms[player->room_id][i]->id);
						}
					}
					server_to_the_player_self("You muted everyone\r\n\n", player->connfd);
				}
				else {
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}
			
			else if (!strcmp(command, "/unmute_all")){
				if (player->GM == 1){
					for (i = 1; i < MAX_PLAYER_IN_ROOMS; i++){
						if (rooms[player->room_id][i]){ //if player exist
							unmute_player(player->room_id, rooms[player->room_id][i]->id);
						}
					}
					server_to_the_player_self("You unmuted everyone\r\n\n", player->connfd);
				}
				else {
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/kick")){
				if (player->GM == 1){
					parameter = strtok(NULL, " ");
					if (parameter){
						int id = atoi(parameter);
						if (id == player->id){
							server_to_the_player_self("You can't kick yourself\r\n\n",player->connfd);
						}
						else{
							if(kick_player(player->room_id, id)){       //returns 1 if successfuly kick
								sprintf(buff_out, "You successfuly kicked the player with ID [%d]\r\n\n", id);
								server_to_the_player_self(buff_out, player->connfd);
							}
							else {                                      //returns 0 if not
								sprintf(buff_out, "There is no player with ID [%d] in this room\n To list players /list \r\n\n", id);
								server_to_the_player_self(buff_out, player->connfd);
							}                         
						}
					}
					else{
						server_to_the_player_self("Enter ID of the player you want to kick\n Type /list to see players\r\n\n",player->connfd);
					}
				}
				else {
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/rname")){
				if (player->GM == 1){
					parameter = strtok(NULL, "\0");
					if (parameter){
					char *name = parameter;

					room_names[player->room_id] = realloc(room_names[player->room_id], sizeof(name));
					strcpy(room_names[player->room_id], name);
					
					server_to_room("Room's name has been changed by GM\r\n", player->room_id);
					sprintf(buff_out, "\tNew name is %s\r\n\n", name);
					server_to_room(buff_out, player->room_id);
					}
					else{
						server_to_the_player_self("You need a name for room.\r\n\n", player->connfd);
					}
				}
				else {
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/pwd")){
				if (player->GM == 1){
					parameter = strtok(NULL, "\0");
					if(parameter){
					char *password = parameter;

					room_passwords[player->room_id] = realloc(room_passwords[player->room_id], sizeof(password));
					strcpy(room_passwords[player->room_id], password);

					server_to_room("Rooms password has been changed by GM\r\n", player->room_id);
					sprintf(buff_out, "\tNew Password is %s\r\n\n", password);
					server_to_the_player_self(buff_out, player->connfd);
					}
					else{
						room_passwords[player->room_id] = NULL;
						server_to_room("Rooms password has been removed by GM\r\n", player->room_id);
					}
				}
				else {
					server_to_the_player_self("You are not GM\r\n\n", player->connfd);
				}
			}


			else if (!strcmp(command, "/help_p") || !strcmp(command, "/help_P")){
				server_to_the_player_self("\r\n", player->connfd);
				server_to_the_player_self("/now /n\t\tShows what happening now\r\n", player->connfd);
			//	server_to_the_player_self("/vote <number> \t\tUse vote when GM ask\r\n", player->connfd);
				server_to_the_player_self("/me \t\tShows who you are\r\n", player->connfd);
				server_to_the_player_self("/who <ID>\tShows player's info \r\n", player->connfd); 
				server_to_the_player_self("/monster /m\tShows monster's info \r\n", player->connfd); 
				server_to_the_player_self("\r\n", player->connfd);
			}

			else if (!strcmp(command, "/now") || !strcmp(command, "/n")){
				if (player->room_id != -1){
					server_to_the_player_self(event[player->room_id], player->connfd);
				}
				else{
					server_to_the_player_self("You are in loby. Nothing happens here\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/roll") || !strcmp(command, "/r")){
				if (player->room_id != -1){
					if (monster_array[player->room_id]){
						if (player->dice == 0){
							player->dice = roll_a_dice();

							sprintf(buff_out, "\n\t[%d] %s rolled a fight dice: %d\r\n",
								player->id,
								player->nick,
								player->dice);
							server_to_room(buff_out, player->room_id);
						}
						else{
							server_to_the_player_self("You have already rolled for this turn.\r\n\n", player->connfd);
						}
					}
					else{
						sprintf(buff_out, "You rolled a dice: %d\r\n\n", roll_a_dice());
						server_to_the_player_self(buff_out, player->connfd);
					}
				}
				else{
					sprintf(buff_out, "You rolled a dice: %d\r\n\n", roll_a_dice());
					server_to_the_player_self(buff_out, player->connfd);
				}
			}

			else if (!strcmp(command, "/me") || !strcmp(command, "/whoami")){
				if (player->room_id != -1){ //If not in lobby
					if (player->GM == 0){   //If not GM
						if (player->character){ //If game is started and a char is created
							show_player(player->connfd, player);
						}
						else{
							server_to_the_player_self("The game has not started yet \r\n\n", player->connfd);
						}
					}
					else{
						server_to_the_player_self("You [GM] can't have a character\r\n\n", player->connfd);
					}
				}
				else{
					server_to_the_player_self("You need to in a room to have a character\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/whois") || !strcmp(command, "/who")){
				if (player->room_id != -1){ //If not in lobby
					if (player->character || player->GM == 1){ //If game is started and a char is created
						parameter = strtok(NULL, " ");
						int flag = 0;
						if (parameter){                                         //if [ID] entered
							int id = atoi(parameter);
							for (i = 0; i < MAX_PLAYER_IN_ROOMS; i++){
								if (rooms[player->room_id][i]){                 //if player exist
									if (rooms[player->room_id][i]->id == id){   //if ID match
										flag = 1;                               //flag to check if player exist
										if(rooms[player->room_id][i]->GM == 0){ //if not GM
											show_player(player->connfd, rooms[player->room_id][i]);
										}
										else{                                   //if GM
											server_to_the_player_self("GM doesn't have a character\r\n\n", player->connfd);
										}
									}
								}
							}
							if (flag == 0){
								sprintf(buff_out, "There is no player with ID [%d] in this room\r\n\n", id);
								server_to_the_player_self(buff_out, player->connfd);
							}
						}
						else{
							server_to_the_player_self("You need to enter [ID] of the player\n To see players type /list\r\n\n", player->connfd);
						}
					}
					else{
						server_to_the_player_self("The game has not started yet \r\n\n", player->connfd);
					}
				}
				else{
					server_to_the_player_self("You can only see the characters in your room and You are in Loby\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/monster") || !strcmp(command, "/m")){
				if (player->room_id != -1){ //If not in lobby
					if (player->character || player->GM == 1){	//game begun
						if (monster_array[player->room_id]){	//if monster exist
							show_monster(player->room_id, buff_out);
							server_to_the_player_self(buff_out, player->connfd);
						}
						else{
							server_to_the_player_self("There is no Monster. Don't Worry\r\n\n", player->connfd);
						}
					}
					else{
						server_to_the_player_self("The game has not started yet \r\n\n", player->connfd);
					}
				}
				else{
					server_to_the_player_self("There are no Monster in Loby. Dont Worry\r\n\n", player->connfd);
				}
			}

			else if (!strcmp(command, "/help") || !strcmp(command, "/h")){
				server_to_the_player_self("\r\n",player->connfd);
				server_to_the_player_self("/create or /c\t\t<Room_Name> <Password(opt)>\r\n", player->connfd);
				server_to_the_player_self("/join or /j\t\t<Room_id> <Password(opt)>\r\n", player->connfd);
				server_to_the_player_self("/rooms\t\t\tTo see the rooms\r\n", player->connfd);
				server_to_the_player_self("/list\t\t\tTo see the in your room\r\n", player->connfd);
				server_to_the_player_self("/dc\t\t\tTo see the in your room\r\n", player->connfd);
				server_to_the_player_self("/roll or /r\t\troll a 6 sided dice\r\n", player->connfd);
				server_to_the_player_self("/nick\t\t\tRename yourself\r\n", player->connfd);
				server_to_the_player_self("/quit or /q\t\tTo quit\r\n", player->connfd);
				server_to_the_player_self("/help_GM\t\tGM commands \r\n", player->connfd); 
				server_to_the_player_self("/help_p\t\t\tplayer commands \r\n", player->connfd); 
				server_to_the_player_self("/help\t\t\tYou know it already\r\n\n", player->connfd);
			}
			else {
				server_to_the_player_self("Unknown command type /help to seek help\r\n\n", player->connfd);
			}
		}
		else{
			char temp[BUFFER_SZ];
			strcpy(temp,buff_in);
			eliminate_r_and_n_at_the_end(temp);
			if (strlen(temp)>0){             //To not send empty message
				if(player->muted == 0){                /* Send message to room*/
					snprintf(buff_out, sizeof(buff_out), "[%s] %s\r\n", player->nick, buff_in);
					send_message_to_room(buff_out, player->id, player->room_id);
				}
				else if(player->muted == 1){
					snprintf(buff_out, sizeof(buff_out), "You are Muted by [GM] %s\r\n\n", rooms[player->room_id][0]->nick);
					server_to_the_player_self(buff_out, player->connfd);
				}
			}
			else {
				check_start(player);
				continue;
			}
			
		}
	}

	delete_players_from_array(player->id);	//delete player from the array
	close(player->connfd);
	free(player);
	pthread_detach(pthread_self());

	player_counter--;
	return NULL;
}


int main(int argc, char *argv[]){
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in player_addr;
	pthread_t thread;  

	if(argc != 2){
		printf("\n Usage: %s <port> \n",argv[0]);
		return -1;
	} 

	/* Socket */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("Socket binding failed");
		return EXIT_FAILURE;
	}

	if (listen(listenfd, 50) < 0) {
		perror("Socket listening failed");
		return EXIT_FAILURE;
	}

	printf("~~~~~~~~~~SERVER STARTED~~~~~~~~~~\n");

	while (1) {         // New Players 
		socklen_t plyr_size = sizeof(player_addr);
		player_st *player = (player_st *)malloc(sizeof(player_st));
		connfd = accept(listenfd, (struct sockaddr*)&player_addr, &plyr_size);

		if ((player_counter + 1) == MAX_PLAYER) { // Check if max playerents is reached
			printf("Max player reached\n");
			close(connfd);
			continue;
		}

		player->connfd = connfd;
		add_players_to_array(player); //add player to array
		pthread_create(&thread, NULL, &player_handler, (void*)player); //fork thread

		sleep(1); // Reduce CPU usage
	}

	return 0;
}

