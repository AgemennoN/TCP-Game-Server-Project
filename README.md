# TCP-Game-Server-Project

Players connect to the server, create a room or join an already opened room, can talk with other players in the rooms. Creater of the room become the GM (Game Master), he can kick and mute players. GM starts the game and manages the room. I will explain in more detail but first how to compile and start the server (Figure 1).

	gcc -o Game server.c –lpthread
 
	./Game 8888

![image](https://user-images.githubusercontent.com/81033171/159159949-b6b732cf-bcbd-4a0d-b62f-6f72770e5827.png)
Figure 1: Compiling and Starting the Server

After the server has started players (clients) can connect with:

	telnet 127.0.0.1 8888
 
 ![image](https://user-images.githubusercontent.com/81033171/159160680-b0220af1-38de-40bc-87e5-dc384729b388.png)
Figure 2: Entering the Server


After the players connect to the server, they use some commands to create and join the rooms. But there are more commands and to see it type 

/help.
 
 
 
