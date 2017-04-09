/*
*
• distributedRouting.c : Distance Vector routing implementation
• @author Abhishek Pimple
• @created 4th November 2015
• @email: apimple@buffalo.edu
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>  
#include <netdb.h>
#include <arpa/inet.h>
#include <limits.h>

//For setting cost to Infinity
#define INF USHRT_MAX
#define FDESC_STDIN 0

//Global Declarations Start

//Data structure for holding given list of servers in topology file
struct server_list {
    unsigned int id;
    char ip_address[16];
    unsigned int port_no;

};

//Data Structure for holding routing table of current server and neighbor server.
struct neighbour_list {
    unsigned int idTo;
    unsigned int nextHop;
    unsigned int cost;
};

struct neighbour_list neighBourList_thisServer[6];
struct server_list serverList_thisServer[6];

int immediateNeighboutList[6];
int intervalWithoutUpdates[6];

char * routingInterval;
int intervalSeconds = 0;
int intervalMicroseconds = 0;
int my_server_id;
int my_port_number;
int numberOfservers;
int numberOfNeighbours;
char * server_ip_address;
char * fileName;

int listener_socket, client, len, retVal, socket_index;
int isCrashed = 0;
static int fdmax;
int newfd = -1; //Newly accepted file descriptor
struct sockaddr_in server, other_server;
fd_set master;
fd_set readfds;
//Global Declarations End

//Initialize routing table of current server
void initialize_neighbour_list() {
    //printf("Server id is : %d \n", my_server_id);
    int i = 1;
    for (i; i < 6; i++) {
        neighBourList_thisServer[i].idTo = i;

        if (i == my_server_id) {
            //printf("Setting cost to 0");
            neighBourList_thisServer[i].cost = 0;
            neighBourList_thisServer[i].nextHop = my_server_id;
            immediateNeighboutList[i] = 0;
        } else {
            neighBourList_thisServer[i].cost = INF;
            neighBourList_thisServer[i].nextHop = 0;
            immediateNeighboutList[i] = -1;
        }

    }
}

//Initialize routing table of neighbor table
void initialize_neighbour_table(struct neighbour_list * neighbor_table) {
    int i = 1;
    for (i; i < 6; i++) {
        neighbor_table[i].idTo = i;
        neighbor_table[i].nextHop = 0;
        neighbor_table[i].cost = INF;
    }
}

//Success message for command
void showSuccessMessage(char command[20]) {
    printf("%s SUCCESS.\n", command);
    fflush(stdout);
}

//Error message for command
void showFailureMessage(char command[20], char errorMessage[200]) {
    printf("%s Error. %s \n", command, errorMessage);
    fflush(stdout);
}

int getNumberOfImmediateNeighbors() {
    int i = 0;
    int numberOfImmediateNeighbors;
    for (i; i < 6; i++) {
        if (immediateNeighboutList[i] == 1) {
            numberOfImmediateNeighbors += 1;
        }
    }

    return numberOfImmediateNeighbors;
}

int getConnIdFromServerIpAddress(char * server_ip_address) {
    int i = 1;
    int connId = 0;
    for (i; i < 6; i++) {
        if (!strcasecmp(serverList_thisServer[i].ip_address, server_ip_address)) {
            connId = serverList_thisServer[i].id;
            break;
        }
    }
    return connId;
}

int getPortFromConnId(int connId) {
    int i = 1;
    int port_no = 0;
    for (i; i < 6; i++) {
        if (serverList_thisServer[i].id == connId) {
            port_no = serverList_thisServer[i].port_no;
            break;
        }
    }
    return port_no;
}

char * getIPaddressFromConnId(int connId) {
    int i = 1;
    char * ip_address;
    for (i; i < 6; i++) {
        if (serverList_thisServer[i].id == connId) {
            ip_address = serverList_thisServer[i].ip_address;
            break;
        }
    }
    return ip_address;
}
void displayServerList(struct server_list serverList_thisServer[6]) {
    int i = 1;

    printf("\n%-5s%-20s%-8s\n", "ID", "IP ADDRESS", "PORT");
    for (i; i < 6; i++) {
        printf("\n%-5d%-20s%-8d", serverList_thisServer[i].id, serverList_thisServer[i].ip_address, serverList_thisServer[i].port_no);
    }
    printf("\n");
    fflush(stdout);

}

void displayImmediateNeighborList() {
    printf("Immediate neighbor list : \n");
    int i = 1;
    for (i; i < 6; i++) {
        printf(" %d", immediateNeighboutList[i]);
    }
    printf("\n");
    fflush(stdout);
}
void displayRoutingTable(struct neighbour_list neighBourList_thisServer[6]) {
    int i = 1;
    printf("Routing table of server ID: %d\n", my_server_id);
    printf("\n%-12s%-8s%-15s\n", "Destination", "NextHop", "COST");
    for (i; i < 6; i++) {
        printf("\n%-12u%-8u%-15u", neighBourList_thisServer[i].idTo, neighBourList_thisServer[i].nextHop, neighBourList_thisServer[i].cost);
    }
    printf("\n");
    fflush(stdout);

}

//Create routing table string to send to neighboring nodes.
char * createRoutTableString() {
    char * routeString = (char * ) calloc(3000, sizeof(char));
    long pos = 0;
    int i;
    int updateField = 0;
    updateField = getNumberOfImmediateNeighbors();

    pos += sprintf(routeString, "route#%s#", server_ip_address);
    pos += sprintf( & routeString[pos], "%d#", my_port_number);
    pos += sprintf( & routeString[pos], "%d#", updateField);
    //printf("Pos : %lu\n",pos);
    int count = 0;
    for (i = 0; i < 6; i++) {
        if (immediateNeighboutList[i] == 1) {
            pos += sprintf( & routeString[pos], "%s", getIPaddressFromConnId(i));
            pos += sprintf( & routeString[pos], ",%u", getPortFromConnId(i));
            pos += sprintf( & routeString[pos], ",%u", i);
            if (count == updateField - 1) {
                pos += sprintf( & routeString[pos], ",%u", neighBourList_thisServer[i].cost);
            } else {
                pos += sprintf( & routeString[pos], ",%u|", neighBourList_thisServer[i].cost);
            }

            count++;
        }
    }
    //printf("routeString : %s\n",routeString);
    return routeString;
}

//Create routing table from costMatrix i.e. routing table obtained from neighbors
void formTable(struct neighbour_list * neighbor_table, char * costMatrix, int updateField, int neighborConnId) {

    char * arg;
    char * saveMain, * saveInside;
    arg = (char * ) calloc(strlen(costMatrix) + 1, sizeof(char));
    arg = strtok_r(costMatrix, "|", & saveMain);
    int paramIndex = 0;
    /*128.205.36.8,10002,1,7#128.205.36.36,10003,3,4*/
    /*  route#128.205.36.33#10001#2#128.205.36.8,10002,1,7|128.205.36.36,10003,3,4|   */
    //For its own entry
    neighbor_table[neighborConnId].idTo = neighborConnId;
    neighbor_table[neighborConnId].nextHop = neighborConnId;
    neighbor_table[neighborConnId].cost = 0;
    while (arg != NULL) {

        fflush(stdout);

        char * newarg;
        newarg = (char * ) calloc(strlen(arg) + 1, sizeof(char));
        char * tempString = (char * ) calloc(strlen(arg) + 1, sizeof(char));
        strcpy(tempString, arg);
        newarg = strtok_r(tempString, ",", & saveInside);
        int newparamIndex = 0;
        unsigned int connId;
        while (newarg != NULL) {

            fflush(stdout);
            if (newparamIndex == 0) {
                //Ip address
            }
            if (newparamIndex == 1) {
                //Port number
            }
            if (newparamIndex == 2) {
                connId = atoi(newarg);

                fflush(stdout);
                neighbor_table[connId].idTo = connId;
                neighbor_table[connId].nextHop = connId;
            }
            if (newparamIndex == 3) {
                unsigned int cost = atoi(newarg);
                neighbor_table[connId].cost = cost;
                if (connId == my_server_id && neighBourList_thisServer[neighborConnId].cost != cost) {
                    //Update my cost to this neighbor if it is changed on neighbor side
                    neighBourList_thisServer[neighborConnId].cost = cost;
                }

            }
            newparamIndex += 1;
            newarg = strtok_r(NULL, ",", & saveInside);

        }
        arg = strtok_r(NULL, "|", & saveMain);

    }

}

//Perform bellman ford algorithms using neighbor routing table and current server routing table
void performBellmanFordAlgorithm(struct neighbour_list * neighbor_table, int neighborConnId) {

    int i = 1;

    for (i; i < 6; i++) {
        int viaNeighborCost = neighbor_table[my_server_id].cost + neighbor_table[i].cost;

        int directCost = neighBourList_thisServer[i].cost;

        if (viaNeighborCost > INF && neighBourList_thisServer[i].nextHop == neighborConnId) {
            neighBourList_thisServer[i].nextHop = 0;
            neighBourList_thisServer[i].cost = INF;
        }
        if (neighBourList_thisServer[i].nextHop == neighborConnId) {
            if (neighBourList_thisServer[i].cost != viaNeighborCost) {
                neighBourList_thisServer[i].cost = viaNeighborCost;
            }
        }
        if (viaNeighborCost < directCost) {
            //printf("Found New Path \n");
            neighBourList_thisServer[i].cost = viaNeighborCost;
            neighBourList_thisServer[i].nextHop = neighborConnId;
        }
    }

}

void createTableFromRouteString(char * routeString) {
    struct neighbour_list * neighbor_table;
    neighbor_table = malloc(6 * sizeof(struct neighbour_list));
    initialize_neighbour_table(neighbor_table);
    /*  routeString = route#10.0.51.133#10001#2#128.205.36.8,10002,1,7|128.205.36.36,10003,3,4|  */
    //printf("Route string is : %s \n", routeString);
    char * arg;
    arg = (char * ) malloc(strlen(routeString) * sizeof(char));
    arg = strtok(routeString, "#");
    int paramIndex = 0;
    int isRoute = 0;
    int updateField = 0;
    int neighborConnId = 0;
    while (arg != NULL) {
        //printf( "main Arg is %s\n", arg );
        fflush(stdout);
        if (paramIndex == 0) {
            if (!strcasecmp(arg, "route")) {
                //printf("It's route string\n");
                isRoute = 1;
            }
        }
        if (isRoute && paramIndex == 1) {
            char * neighbour_server_address = arg;
            neighborConnId = getConnIdFromServerIpAddress(neighbour_server_address);

        }
        if (isRoute && paramIndex == 2) {
            unsigned int port_no;
            port_no = atoi(arg);

        }
        if (isRoute && paramIndex == 3) {
            updateField = atoi(arg);

        }
        if (isRoute && paramIndex == 4) {

            formTable(neighbor_table, arg, updateField, neighborConnId);
        }
        paramIndex += 1;
        arg = strtok(NULL, "#");
    }

    performBellmanFordAlgorithm(neighbor_table, neighborConnId);
}

/*  Get Public IP address of current server by creating UDP socket with google DNS server.
Returns Public IP address of current server.
Ref: Used my project1 code.
*/
char * get_my_ip() {

    char * google_dns_server = "8.8.8.8";
    char * google_dns_port = "53";

    /* get peer server */
    struct addrinfo hints;
    memset( & hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo * info;
    int ret = 0;
    if ((ret = getaddrinfo(google_dns_server, google_dns_port, & hints, & info)) != 0) {
        printf("[ERROR]: getaddrinfo error: %s\n", gai_strerror(ret));
        return "NULL";
    }

    if (info - > ai_family == AF_INET6) {
        printf("[ERROR]: do not support IPv6 yet.\n");
        return "NULL";
    }

    /* create socket */
    int sock = socket(info - > ai_family, info - > ai_socktype, info - > ai_protocol);
    if (sock <= 0) {
        perror("socket");
        return "NULL";
    }

    /* connect to server */
    if (connect(sock, info - > ai_addr, info - > ai_addrlen) < 0) {
        perror("connect");
        close(sock);
        return "NULL";
    }

    /* get local socket info */
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr * ) & local_addr, & addr_len) < 0) {
        perror("getsockname");
        close(sock);
        return "NULL";
    }

    /* get peer ip addr */
    char * myip, myip1[INET_ADDRSTRLEN];
    myip = (char * ) malloc(INET_ADDRSTRLEN * sizeof(char));
    if (inet_ntop(local_addr.sin_family, & (local_addr.sin_addr), myip1, sizeof(myip1)) == NULL) {
        perror("inet_ntop");
        return "NULL";
    }
    strcpy(myip, myip1);
    fflush(stdout);
    return myip;

}

//Read current topology file specified at server startup
int readFileandPopulateVariables() {
    FILE * file = fopen(fileName, "r"); //Read topology file in ReadOnly mode 

    if (file) {
        char line[256];

        int count = 0;
        int serverlistIndex = 1;
        int neighbourlistIndex = 1;
        int currentServer = 0;
        while (fgets(line, sizeof(line), file)) {

            currentServer = 0;
            char * pos;
            //remove trailing new line \n character
            if ((pos = strchr(line, '\n')) != NULL) { * pos = '\0';
            }

            if (count == 0) {
                numberOfservers = atoi(line);

            }
            if (count == 1) {
                numberOfNeighbours = atoi(line);

            }
            if (count >= 2 && count < 2 + numberOfservers) {

                char * arg;
                arg = (char * ) malloc(strlen(line) * sizeof(char));
                arg = strtok(line, " ");
                int paramIndex = 0;
                while (arg != NULL) {
                    //printf( "Arg is %s\n", arg );
                    fflush(stdout);
                    if (paramIndex == 0) {
                        serverlistIndex = atoi(arg);
                        serverList_thisServer[serverlistIndex].id = atoi(arg);
                    }
                    if (paramIndex == 1) {
                        if (!strcasecmp(server_ip_address, arg)) {
                            currentServer = 1;
                            my_server_id = serverList_thisServer[serverlistIndex].id;
                            initialize_neighbour_list(neighBourList_thisServer);

                        }
                        strcpy(serverList_thisServer[serverlistIndex].ip_address, arg);
                    }
                    if (paramIndex == 2) {
                        serverList_thisServer[serverlistIndex].port_no = atoi(arg);
                        if (currentServer) {
                            my_port_number = serverList_thisServer[serverlistIndex].port_no;
                            currentServer = 0;
                        }

                    }
                    paramIndex += 1;
                    arg = strtok(NULL, " ");
                }

            }
            if (count >= 7 && count < 7 + numberOfNeighbours) {
                char * arg;
                arg = (char * ) malloc(strlen(line) * sizeof(char));
                arg = strtok(line, " ");
                int paramIndex = 0, from, to;
                int thiscost;
                while (arg != NULL) {
                    //printf( "Arg is %s\n", arg );
                    fflush(stdout);
                    if (paramIndex == 0) {
                        from = atoi(arg);

                    }
                    if (paramIndex == 1) {
                        to = atoi(arg);

                    }
                    if (paramIndex == 2) {

                        thiscost = atoi(arg);

                    }
                    paramIndex += 1;

                    arg = strtok(NULL, " ");
                }
                neighBourList_thisServer[to].nextHop = to;
                neighBourList_thisServer[to].idTo = to;
                immediateNeighboutList[to] = 1;
                neighBourList_thisServer[to].cost = thiscost;
                neighbourlistIndex += 1;
            }
            count += 1;
        }

        //Close file after reading it
        fclose(file);
        return 1;
    } else {
        return 0;
    }

}

//Helps in creating socket, binding and listening
void setupThisServer() {

    //Create server socket
    if ((listener_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Error in creating socket:");
        exit(-1);
    }
    bzero( & server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(my_port_number);
    server.sin_addr.s_addr = INADDR_ANY; /* Get public IP */
    bzero( & server.sin_zero, 8);

    int len = sizeof(struct sockaddr_in);
    //Bind IP and Port to server 
    if ((bind(listener_socket, (struct sockaddr * ) & server, len)) == -1) {
        perror("Error in Bind: ");
        exit(-1);
    }
    //printf("Successfully bound to this port : %d\n", my_port_number);

    printf("Server IP address : %s\n", server_ip_address);
    printf("Server Port : %d \n", my_port_number);
    printf("Server Connection Id : %d \n", my_server_id);
    printf("Update Interval : %d seconds %d microseconds \n", intervalSeconds, intervalMicroseconds);
    fflush(stdout);
    //Clear all bits in readfds and master
    FD_ZERO( & readfds);
    FD_ZERO( & master);

    //Set descriptors for STDIN data input and Listerning socket which we have just created.
    FD_SET(FDESC_STDIN, & master);
    FD_SET(listener_socket, & master);

    //Keeping track of biggest file descriptor
    fdmax = listener_socket;
}

//Update cost in current routing table
void updateCost(char splitCommand[100][100]) {
    unsigned int source = atoi(splitCommand[1]);
    unsigned int dest = atoi(splitCommand[2]);
    unsigned int newCost;
    if (!strcasecmp(splitCommand[3], "inf")) {
        newCost = INF;
        int i = 0;
        for (i; i < 6; i++) {
            if (neighBourList_thisServer[i].nextHop == dest) {
                neighBourList_thisServer[i].cost = INF;
                neighBourList_thisServer[i].nextHop = 0;
            }
        }
    } else {
        newCost = atoi(splitCommand[3]);
        neighBourList_thisServer[dest].nextHop = dest;
    }

    neighBourList_thisServer[dest].cost = newCost;
}

//Send updated cost to neighbor
void sendUpdatetoNeighbor(char splitCommand[100][100]) {

    int i = 1;
    struct sockaddr_in other_router;
    int other_sockefd, len = sizeof(other_router);
    int destinationId = atoi(splitCommand[2]);
    char * routeString = (char * ) calloc(1000, sizeof(char));
    //printf("routeString update : %s \n", routeString);
    unsigned int costtosend;
    if (!strcasecmp(splitCommand[3], "inf")) {
        costtosend = INF;
    } else {
        costtosend = atoi(splitCommand[3]);
    }

    int retVal = sprintf(routeString, "update#%s#%s#%u", splitCommand[1], splitCommand[2], costtosend);

    if ((other_sockefd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Error in creating socket:");
        exit(-1);
    }
    memset((char * ) & other_router, 0, len);
    other_router.sin_family = AF_INET;
    other_router.sin_port = htons(getPortFromConnId(destinationId));

    if (inet_aton(getIPaddressFromConnId(destinationId), & other_router.sin_addr) == 0) {
        perror("inet_aton failed");
    }

    if (sendto(other_sockefd, routeString, strlen(routeString), 0, (struct sockaddr * ) & other_router, len) == -1) {
        perror("Error in sending routing table");
    }

    //Close the socket of neighbor after sending DV
    close(other_sockefd);

}

//Send distance routing updates to neighboring nodes.
void sendRoutingUpdates() {
    char * routeString = createRoutTableString();
    int i = 1;
    struct sockaddr_in other_router;
    int other_sockefd, len = sizeof(other_router);
    for (i; i < 6; i++) {
        if (neighBourList_thisServer[i].cost != INF && immediateNeighboutList[i] == 1) {
            if ((other_sockefd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
                perror("Error in creating socket:");
                exit(-1);
            }
            memset((char * ) & other_router, 0, len);
            other_router.sin_family = AF_INET;
            other_router.sin_port = htons(getPortFromConnId(i));

            if (inet_aton(getIPaddressFromConnId(i), & other_router.sin_addr) == 0) {
                perror("inet_aton failed");
            }

            if (sendto(other_sockefd, routeString, strlen(routeString), 0, (struct sockaddr * ) & other_router, len) == -1) {
                perror("Error in sending routing table");
            }

            //Close the socket of neighbor after sending DV
            close(other_sockefd);
        }
    }
}

//Listen to packets from neighbors. Listen to user commands too.
void listenAndSendPackets() {

    int argc_1, lastChar;
    int numReceived;
    char cmd[100];
    char data[4000];
    argc_1 = 0;
    int isPacketCommandRequested = 0;
    int numberOfPackets = 0;
    int updateInterval = 0;
    //updateInterval = atoi(routingInterval);

    FD_ZERO( & master);
    FD_SET(FDESC_STDIN, & master);
    FD_SET(listener_socket, & master);

    int k = 0;
    for (k; k < 6; k++) {
        intervalWithoutUpdates[k] = 0;
    }

    struct timeval timeout;
    //Reset timeout and FD_SET
    timeout.tv_sec = intervalSeconds;
    timeout.tv_usec = intervalMicroseconds;
    int isTimeoutOccurred = 0;
    while (1) {
        //printf("Starting infinite while loop \n");
        fflush(stdout);
        if (isTimeoutOccurred != 1) {
            printf("[PA2]$ ");
            fflush(stdout);
        }
        isTimeoutOccurred = 0;

        if (isPacketCommandRequested == 1) {
            isPacketCommandRequested = 0;
            numberOfPackets = 0;
        }

        // printf("Copying master to readfds \n");
        readfds = master;

        retVal = select(fdmax + 1, & readfds, NULL, NULL, & timeout);
        /*retVal = select(fdmax+1, &readfds, NULL, NULL, NULL);*/
        if (retVal == -1) {
            perror("Error in select: ");
            exit(-1);
        } else if (retVal == 0) {
            //Send distance vector after specific updateInterval

            sendRoutingUpdates();
            //Reset timeout and FD_SET
            timeout.tv_sec = intervalSeconds;
            timeout.tv_usec = intervalMicroseconds;
            fflush(stdout);
            isTimeoutOccurred = 1;

            int k = 1;

            for (k = 1; k < 6; k++) {
                if (immediateNeighboutList[k] == 1 && neighBourList_thisServer[k].cost != INF && intervalWithoutUpdates[k] == 3) {
                    fflush(stdout);
                    printf("\n Didn't receive routing update for 3 consecutive intervals from server id : %d \n", k);
                    printf("Updating cost to infinity for server ID : %d\n", k);
                    fflush(stdout);
                    neighBourList_thisServer[k].cost = INF;
                    neighBourList_thisServer[k].nextHop = 0;
                    intervalWithoutUpdates[k] = 0;
                    int x = 1;
                    for (x; x < 6; x++) {
                        if (neighBourList_thisServer[x].nextHop == k) {
                            neighBourList_thisServer[x].cost = INF;
                            neighBourList_thisServer[x].nextHop = 0;
                        }
                    }
                } else {
                    intervalWithoutUpdates[k] += 1;
                }

            }

        } else {
            argc_1 = 0;
            for (socket_index = 0; socket_index <= fdmax; socket_index++) {

                if (FD_ISSET(socket_index, & readfds)) {

                    if (socket_index == FDESC_STDIN) {
                        //Ref: From recitation PDF
                        fgets(cmd, 100, stdin);
                        fflush(stdout);
                        char splitCommand[100][100];
                        char * arg;
                        arg = (char * ) malloc(strlen(cmd) * sizeof(char));
                        arg = strtok(cmd, " ");
                        while (arg != NULL) {
                            //printf( "Arg is %s\n", arg );
                            fflush(stdout);
                            strcpy(splitCommand[argc_1], arg);
                            argc_1 += 1;
                            arg = strtok(NULL, " ");
                        }
                        //Last character of last string has \n
                        lastChar = strlen(splitCommand[argc_1 - 1]) - 1;
                        splitCommand[argc_1 - 1][lastChar] = '\0';

                        //printf("Current command is: %s", splitCommand[0]);
                        if (!strcasecmp(splitCommand[0], "UPDATE")) {
                            //printf("argc : %d \n",argc_1);
                            if (argc_1 != 4) {
                                showFailureMessage("UPDATE", "Invalid number of arguments for UPDATE Command. Please read README File");
                            } else {
                                int updateTo = atoi(splitCommand[2]);
                                int updateFrom = atoi(splitCommand[1]);
                                if (updateTo < 1 || updateTo > 5 || updateFrom < 1 || updateFrom > 5) {
                                    showFailureMessage("UPDATE", "Please use Valid Server Ids");
                                } else if (immediateNeighboutList[updateTo] != 1) {
                                    showFailureMessage("UPDATE", "Not a neighbor. You can only update Cost to neighbor");
                                } else {
                                    sendUpdatetoNeighbor(splitCommand);
                                    updateCost(splitCommand);
                                    //sendRoutingUpdates();
                                    showSuccessMessage("UPDATE");
                                }

                            }
                        } else
                        if (!strcasecmp(splitCommand[0], "STEP")) {
                            //Send routing updates to neighbours immediately
                            if (argc_1 != 1) {
                                showFailureMessage("STEP", "Invalid number of arguments for STEP Command. Please read README File");
                            } else {
                                sendRoutingUpdates();
                                showSuccessMessage("STEP");
                            }
                        } else
                        if (!strcasecmp(splitCommand[0], "PACKETS")) {
                            /*Display the number of distance vector packets this server has received since the last instance when this
                            information was requested*/
                            if (argc_1 != 1) {
                                showFailureMessage("PACKETS", "Invalid number of arguments for PACKETS Command. Please read README File");
                            } else {
                                isPacketCommandRequested = 1;
                                printf("Number of distance vector packets received : %d\n", numberOfPackets);
                                showSuccessMessage("PACKETS");
                            }

                        } else
                        if (!strcasecmp(splitCommand[0], "DISABLE")) {
                            /*Disable the link to given server. Here you need to check if the given server is its neighbor.*/

                            if (argc_1 != 2) {
                                showFailureMessage("DISABLE", "Invalid number of arguments for DISABLE Command. Please read README File");
                            } else {
                                unsigned int neighbourId = atoi(splitCommand[1]);
                                //Check if server is neighbour
                                if (immediateNeighboutList[neighbourId] == 1) {
                                    //Update cost to its neighbour to infinity to disable link
                                    neighBourList_thisServer[neighbourId].cost = INF;
                                    neighBourList_thisServer[neighbourId].nextHop = 0;
                                    immediateNeighboutList[neighbourId] = -1;
                                    int k = 1;
                                    for (k; k < 6; k++) {
                                        if (neighBourList_thisServer[k].nextHop == neighbourId) {
                                            neighBourList_thisServer[k].nextHop = 0;
                                            neighBourList_thisServer[k].cost = INF;
                                        }
                                    }
                                    showSuccessMessage("DISABLE");
                                } else {
                                    //Not a neighbour so can't update cost
                                    showFailureMessage("DISABLE", "Given server ID is NOT a neighbour");
                                }
                            }
                        } else
                        if (!strcasecmp(splitCommand[0], "CRASH")) {
                            if (argc_1 != 1) {
                                showFailureMessage("CRASH", "Invalid number of arguments for CRASH Command. Please read README File");
                            } else {
                                isCrashed = 1;
                                close(listener_socket);
                                fdmax = fdmax - 1;
                                FD_CLR(listener_socket, & master);
                                int k = 1;
                                for (k; k < 6; k++) {
                                    if (immediateNeighboutList[k] != 0) {
                                        immediateNeighboutList[k] = -1;
                                    }
                                    if (k != my_server_id) {
                                        neighBourList_thisServer[k].cost = INF;
                                        neighBourList_thisServer[k].nextHop = 0;
                                    }
                                }
                                showSuccessMessage("CRASH");
                            }

                        } else
                        if (!strcasecmp(splitCommand[0], "DISPLAY")) {
                            if (argc_1 != 1) {
                                showFailureMessage("DISPLAY", "Invalid number of arguments for DISPLAY Command. Please read README File");
                            } else if (isCrashed) {
                                //Do nothing
                            } else {
                                displayRoutingTable(neighBourList_thisServer);
                                //displayImmediateNeighborList();
                                showSuccessMessage("DISPLAY");
                            }
                        } else {
                            printf("Inavlid Command. Please read README file\n");
                            fflush(stdout);
                        }
                    } else if (socket_index == listener_socket) {
                        //Handle data from other routers
                        struct sockaddr_in other_router;
                        int len = sizeof(other_router);
                        char * buff = (char * ) calloc(1000, sizeof(char));

                        int numReceived = recvfrom(socket_index, buff, 1000, 0, (struct sockaddr * ) & other_router, & len);
                        //printf("Number of bytes received : %d\n", numReceived);
                        //printf("Received String is : %s \n", buff);
                        if (numReceived == -1) {
                            perror("Error in receiving packet");
                        } else {

                            //printf("Received from %s:%d\nData: %s\n\n",inet_ntoa(other_router.sin_addr), ntohs(other_router.sin_port), buff);
                            int receivedFromId = getConnIdFromServerIpAddress(inet_ntoa(other_router.sin_addr));
                            /*if(neighBourList_thisServer[receivedFromId].cost != INF){*/
                            intervalWithoutUpdates[receivedFromId] = 0;
                            printf("\nReceived distance vector from Server ID: %d \n", receivedFromId);
                            fflush(stdout);
                            //createTableFromRouteString(buff);
                            struct neighbour_list * neighbor_table;
                            neighbor_table = malloc(6 * sizeof(struct neighbour_list));
                            initialize_neighbour_table(neighbor_table);
                            /*  routeString = route#10.0.51.133#10001#2#128.205.36.8,10002,1,7|128.205.36.36,10003,3,4|  */
                            //printf("Route string is : %s \n", routeString);
                            char * routeString = buff;
                            char * arg;
                            arg = (char * ) malloc(strlen(routeString) * sizeof(char));
                            arg = strtok(routeString, "#");
                            int paramIndex = 0;
                            int isRoute = 0;
                            int isUpdate = 0;
                            int updateField = 0;
                            int neighborConnId = 0;
                            unsigned int from, to, updatedCost;
                            while (arg != NULL) {
                                //printf( "main Arg is %s\n", arg );
                                fflush(stdout);
                                if (paramIndex == 0) {
                                    if (!strcasecmp(arg, "route")) {
                                        //printf("It's route string\n");
                                        numberOfPackets += 1;
                                        isRoute = 1;
                                    }
                                    if (!strcasecmp(arg, "update")) {
                                        isUpdate = 1;
                                    }
                                }
                                if (isRoute && paramIndex == 1) {
                                    char * neighbour_server_address = arg;
                                    neighborConnId = getConnIdFromServerIpAddress(neighbour_server_address);
                                    //printf("neighborConnId : %d\n", neighborConnId);
                                    //printf("neighbour_server_address : %s\n", neighbour_server_address);
                                }
                                if (isRoute && paramIndex == 2) {
                                    unsigned int port_no;
                                    port_no = atoi(arg);
                                    //printf("Neighbour server port : %d\n", port_no);
                                }
                                if (isRoute && paramIndex == 3) {
                                    updateField = atoi(arg);
                                    //printf("updateField : %d\n", updateField);
                                }
                                if (isRoute && paramIndex == 4) {
                                    //printf("Rest of the string is : %s\n", arg);
                                    formTable(neighbor_table, arg, updateField, neighborConnId);
                                }
                                if (isUpdate && paramIndex == 1) {
                                    from = atoi(arg);
                                }
                                if (isUpdate && paramIndex == 2) {
                                    to = atoi(arg);
                                }
                                if (isUpdate && paramIndex == 3) {
                                    updatedCost = atoi(arg);
                                }
                                paramIndex += 1;
                                arg = strtok(NULL, "#");
                            }

                            //displayRoutingTable(neighbor_table);
                            if (isRoute) {
                                performBellmanFordAlgorithm(neighbor_table, neighborConnId);
                            } else if (isUpdate) {
                                printf("Update cost received : %u from server ID:%u\n", updatedCost, from);
                                if (updatedCost == INF) {
                                    //printf("Received innfinity. From: %u \n", from);
                                    neighBourList_thisServer[from].nextHop = 0;
                                    //immediateNeighboutList[from] = -1;
                                    neighBourList_thisServer[from].cost = updatedCost;
                                    //displayRoutingTable(neighBourList_thisServer);
                                    int r = 1;
                                    for (r; r < 6; r++) {
                                        if (neighBourList_thisServer[r].nextHop == from) {
                                            neighBourList_thisServer[r].nextHop = 0;
                                            neighBourList_thisServer[r].cost = INF;
                                        }
                                    }
                                } else {
                                    neighBourList_thisServer[from].nextHop = from;
                                    neighBourList_thisServer[from].cost = updatedCost;
                                }

                            }
                            /* }*/

                        }

                    }
                }

            }
        }
    }
}

//Driver function
int main(int argc, char * * argv) {
    //printf("INF: %u\n", INF);
    server_ip_address = get_my_ip();
    //printf("server_ip_address : %s", server_ip_address);
    if (argc == 5 && !strcasecmp(argv[1], "-t") && !strcasecmp(argv[3], "-i")) {
        //printf("File name : %s\n", argv[2]);
        routingInterval = argv[4];
        //printf("Routing interval : %s \n", routingInterval);

        char * arg;
        arg = (char * ) calloc(strlen(routingInterval) + 1, sizeof(char));
        arg = strtok(routingInterval, ".");
        int paramIndex = 0;

        while (arg != NULL) {
            //printf( "main Arg is %s\n", arg );
            fflush(stdout);
            if (paramIndex == 0) {
                intervalSeconds = atoi(arg);
            }
            if (paramIndex == 1) {
                intervalMicroseconds = atoi(arg);
                if (intervalMicroseconds < 0) {
                    intervalMicroseconds = 0;
                }
            }
            paramIndex += 1;
            arg = strtok(NULL, ".");
        }
        /*printf("Seconds: %d, MicroSeconds: %d \n", intervalSeconds, intervalMicroseconds);*/

        //Reading topology file
        fileName = argv[2];
        int fileSuccess = 0;
        fileSuccess = readFileandPopulateVariables();
        if (!fileSuccess) {
            printf("Given file does not exist. \n");
            return 0;
        }
        setupThisServer();
        //createTableFromRouteString(createRoutTableString());
        //createTableFromRouteString("128.205.36.8,10002,1,7|128.205.36.36,10003,3,4");
        listenAndSendPackets();

    } else if (argc != 5) {
        printf("Inavalid number of arguments. Please read README file \n");
        return 0;
    }
    return 0;

}