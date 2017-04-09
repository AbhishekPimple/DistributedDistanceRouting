1. To Compile
	gcc distributedRouting.c -o distributedRouting

	
2. To Run

	./distributedRouting -t topology.txt -i 10
	Here topology.txt is initial topology for given server and 10 is an update interval in seconds after which given 
	server sends it distance vector to its immediate neighbors.


3. Commands and their descriptions

Sr.No	Commands 									Description

1		./distributedRouting -t topology.txt -i 10				To start server with given topology file and update interval.

2 		update <serverId1> <serverId2> <Linkcost> 	Updates cost from server ID 1 to server ID 2 when run on Server ID 1.

3		step										Send distance vector to neighbors immediately. (User trigger).

4		packets										Displays number of packets (distance vector) this server has received since
													last request of this command.
													
5		display										Display current routing table of server.

6		disable <serverId>							Disable link between current server and given server ID.

7 		crash										Close all connections. This is to simulate server crash.

4. Server Responses

<COMMAND> SUCCESS.
	It means the given command is successfully executed on given server.
	
<COMMAND> ERRORR. <ERROR MESSAGE>
	It means given command could not execute. Please read ERROR MESSAGE carefully to analyze error.