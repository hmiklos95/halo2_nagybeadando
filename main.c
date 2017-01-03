#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define PORT "1111"

#define SIZE 15

#define CLIENTNUM 2
#define HORIZONTAL 0
#define VERTICAL 1
#define DIAGONAL_LEFT 2
#define DIAGONAL_RIGHT 3

typedef struct client {
    int fd;
    int score;

    int newGameChoice;
} client;

typedef struct position {
    int reserved;
    int fired;

    client* hitBy;
} position;

typedef struct game {
    int round;
    int hitCount;
    int reservedCount;

    position** table;

    client* current;

    client* first;
    client* second;

    int waitingAnswerCount;

} game;

int setupListenerSocket() {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    hints.ai_flags = AF_UNSPEC;

    getaddrinfo(NULL, PORT, &hints, &res);

    int listener = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    bind(listener, res->ai_addr, INET_ADDRSTRLEN);

    freeaddrinfo(res);

    listen(listener, 5);

    return listener;
}

void sendGameEndedMessage(game* currentGame) {
    char gameEndedMessage[100];
    sprintf(gameEndedMessage, "A jatek veget ert.: P1: %d pont, P2: %d pont \n", currentGame->first->score, currentGame->second->score);


    send(currentGame->first->fd, gameEndedMessage, strlen(gameEndedMessage), 0);
    send(currentGame->second->fd, gameEndedMessage, strlen(gameEndedMessage), 0);
}

void sendQuestion(game* currentGame) {
    char gameEndedMessage[100];
    sprintf(gameEndedMessage, "Kivan uj jatekot kezdeni?\n");

    send(currentGame->current->fd, gameEndedMessage, strlen(gameEndedMessage), 0);
}

int fire(game* currentGame, char* positionChars) {
    int column = positionChars[0] - 65; //letter
    int row = atoi(positionChars + 1) - 1;

    /*if(column >= SIZE || row >= SIZE || row <= 0 || column <= 0) {
        return -1;
    }*/

    int wasAlreadyHit = currentGame->table[row][column].fired; //count shots only once
    int hitNow = 0;

    currentGame->table[row][column].fired = 1;
    currentGame->table[row][column].hitBy = currentGame->current;

    hitNow = currentGame->table[row][column].reserved && !wasAlreadyHit;

    currentGame->current->score += hitNow;

    char successMessage[10];

    if(hitNow) {
        sprintf(successMessage, "sikeres loves!\n");
    } else {
        sprintf(successMessage, "sikertelen loves!\n");
    }

    send(currentGame->current->fd, successMessage, strlen(successMessage), 0);

    return hitNow;
}

void printTableForGamer(game* currentGame, int clientfd)  {
    char tableString[SIZE*SIZE*2 + SIZE + 1];

    int i, j;

    char* curr = tableString;
    for(i = 0; i<SIZE; i++) {
        for(j = 0; j<SIZE; j++) {

            position current = currentGame->table[i][j];

            if(current.fired) {
                curr += sprintf(curr, "%s", current.reserved ? "x|" : "o|");
            } else {
                curr += sprintf(curr, "%s", " |");
            }

        }
        curr += sprintf(curr, "\n");
    }

    send(clientfd, tableString, strlen(tableString), 0);
}

void abortGame(game* currentGame) {
    char message[80];
    sprintf(message, "A jatek megszakitva. A jelenlegi allas: P1: %d pont, P2: %d pont \nKoszonjuk a jatekot.\n", currentGame->first->score, currentGame->second->score);


    send(currentGame->first->fd, message, strlen(message), 0);
    send(currentGame->second->fd, message, strlen(message), 0);

    close(currentGame->first->fd);
    close(currentGame->second->fd);
}

void exitGame(game* currentGame) {
    char yesMessage[100];
    sprintf(yesMessage, "Csak akkor indulhat uj jatek, ha a masik fel is ugy akarja. Koszonjuk a jatekot.\n");

    char noMessage[80];
    sprintf(noMessage, "Rendben, koszonjuk a jatekot.\n");

    if(currentGame->first->newGameChoice) {
        send(currentGame->first->fd, yesMessage, strlen(yesMessage), 0);
    } else {
        send(currentGame->first->fd, noMessage, strlen(noMessage), 0);
    }

    if(currentGame->second->newGameChoice) {
        send(currentGame->second->fd, yesMessage, strlen(yesMessage), 0);
    } else {
        send(currentGame->second->fd, noMessage, strlen(noMessage), 0);
    }

    close(currentGame->first->fd);
    close(currentGame->second->fd);
}


void drawTable(position** table) {
    int i, j;
    for(i = 0; i<SIZE; i++) {
        for(j = 0; j<SIZE; j++) {
            printf("%d ", table[i][j].reserved);
        }
        printf("\n");
    }
}

int isPlaceFree(position** table, int start, int size, int direction) {


    int y = start/SIZE;
    int x = start%SIZE;

    if(x<0 || x >= SIZE || y<0 || y>= SIZE) {
        return 0;
    }

    int endX, endY;

    if(start<0) {
        return 0;
    }

    switch (direction) {
    case HORIZONTAL:

        endX = x + size - 1;
        if(endX>=SIZE) {
            return 0;
        }


        int currX, currY;
        for(currX = x; currX<=endX; currX++) {
            if(table[y][currX].reserved) {
                return 0;
            }
        }

        return 1;
    case VERTICAL:


        endY = y + size - 1;
        if(endY>=SIZE) {
            return 0;
        }


        for(currY = y; currY<=endY; currY++) {
            if(table[currY][x].reserved) {
                return 0;
            }
        }

        return 1;

    case DIAGONAL_LEFT:


        endX = x - size + 1;
        endY = y + size - 1;

        if(endX< 0 || endY>=SIZE) {
            return 0;
        }


        for(currX = x, currY = y; currX>=endX && currY<=endY; currY++, currX--) {
            if(table[currY][currX].reserved) {
                return 0;
            }
        }

        return 1;

    case DIAGONAL_RIGHT:
        endX = x + size - 1;
        endY = y + size - 1;

        if(endY>=SIZE || endX>=SIZE) {
            return 0;
        }


        for(currX = x, currY = y; currX<=endX && currY<=endY; currY++, currX++) {
            if(table[currY][currX].reserved) {
                return 0;
            }
        }

        return 1;

        break;


    default:
        return 0;

    }
}

void markAsAllocated(position** table, int start, int size, int direction) {
    int y = start/SIZE;
    int x = start%SIZE;


    int endX, endY;

    switch (direction) {
    case HORIZONTAL:

        endX = x + size - 1;

        int currX, currY;
        for(currX = x; currX<=endX; currX++) {
            table[y][currX].reserved = 1;
        }

        break;
    case VERTICAL:

        endY = y + size - 1;

        for(currY = y; currY<=endY; currY++) {
            table[currY][x].reserved = 1;
        }

        break;

    case DIAGONAL_LEFT:


        endX = x - size + 1;
        endY = y + size - 1;

        for(currX = x, currY = y; currX>=endX && currY<=endY; currY++, currX--) {
            table[currY][currX].reserved = 1;
        }

        break;

    case DIAGONAL_RIGHT:
        endX = x + size - 1;
        endY = y + size - 1;

        for(currX = x, currY = y; currX<=endX && currY<=endY; currY++, currX++) {
            table[currY][currX].reserved = 1;
        }

        break;

        break;


    default:
        break;
    }
}

int allocShip(position** table, int size) {

    //horizontal, vertical, diagonalLeft, diagonalRight
    int direction = rand()%4;


    //random position
    int start = rand() % (SIZE*SIZE);

    int isFree = isPlaceFree(table, start, size, direction);


    int seek = start;

    //start seekind downwards until find a place to fit
    if(!isFree) {
        do {
            isFree = isPlaceFree(table, ++seek, size, direction);
        }while(!isFree && seek<SIZE*SIZE);
    }

    //start seekind upwards if not found
    if(!isFree) {
        seek = start;
        do {
            isFree = isPlaceFree(table, --seek, size, direction);
        }while(!isFree && seek>0);
    }


    //if position found, allocate it
    if(isFree) {
        markAsAllocated(table, seek, size, direction);
        return size;
    } else {
        return 0;
    }
}

int allocShips(position** table) {
    srand(time(NULL));

    int reservedCount = 0;

    int currentSize;

    int shipNumbers[5] = {3, 3, 2, 2, 1};

    for(currentSize = 1; currentSize<=5; currentSize++) {
        int j;
        for(j = 0; j<shipNumbers[currentSize - 1]; j++) {
            reservedCount += allocShip(table, currentSize);
        }
    }

    return reservedCount;
}

position** createTable() {

    position** table = (position**) malloc(SIZE*sizeof(position*));

    int i, j;
    for(i = 0; i<SIZE; i++) {
        table[i] = (position*) malloc(SIZE*sizeof(position));
        for(j = 0; j<SIZE; j++) {
            table[i][j].fired = 0;
            table[i][j].hitBy = 0;
            table[i][j].reserved = 0;
        }

    }


    return table;
}

client* createGamer(int clientfd) {
    client* newGamer = (client*)malloc(sizeof(client));
    newGamer->fd = clientfd;
    newGamer->score = 0;
    newGamer->newGameChoice = 0;

    return newGamer;
}

game* createGame(int firstGamerFd) {
    game* created = (game*)malloc(sizeof(game));
    client* createdGamer = createGamer(firstGamerFd);

    created->table = createTable();
    created->reservedCount = allocShips(created->table);
    created->round = 0;
    created->hitCount = 0;

    created->first = createdGamer;
    created->second = 0;
    created->current = createdGamer;

    created->waitingAnswerCount = 0;

    printf("new game created: \n");
    drawTable(created->table);

    return created;
}

void resetGame(game* currentGame) {
    currentGame->table = createTable();
    currentGame->reservedCount = allocShips(currentGame->table);
    currentGame->round = 0;
    currentGame->hitCount = 0;

    currentGame->current = currentGame->first;
    currentGame->first->score = 0;
    currentGame->first->newGameChoice = 0;

    currentGame->second->score = 0;
    currentGame->second->newGameChoice = 0;


    currentGame->waitingAnswerCount = 0;

    char *message = "Kerem a lovest\n";
    send(currentGame->current->fd, message, strlen(message), 0);

    printf("new game created: \n");
    drawTable(currentGame->table);
}

void sendRules(int clientfd) {

    /* declare a file pointer */
    FILE    *infile;
    char    *buffer;
    long    numbytes;

    /* open an existing file for reading */
    infile = fopen("rules", "r");

    /* quit if the file does not exist */
    if(infile == NULL)
        return;

    /* Get the number of bytes */
    fseek(infile, 0L, SEEK_END);
    numbytes = ftell(infile);

    /* reset the file position indicator to
    the beginning of the file */
    fseek(infile, 0L, SEEK_SET);

    /* grab sufficient memory for the
    buffer to hold the text */
    buffer = (char*)calloc(numbytes, sizeof(char));

    /* memory error */
    if(buffer == NULL)
        return;

    /* copy all the text into the buffer */
    fread(buffer, sizeof(char), numbytes, infile);
    fclose(infile);

    send(clientfd, buffer, numbytes, 0);

    /* free the memory we used for the buffer */
    free(buffer);
}

int processClientMessage(char *message, game* currentGame, int clientfd) {
    message[strlen(message) - 1] = '\0';
    char* firstPart = strtok(message, " ");

    if(firstPart != 0) {
        if(strcmp(firstPart, "TUZ") == 0 && currentGame->current->fd == clientfd && currentGame->second) { //make sure that only current gamer can fire, and there are two players present
            char* positionChars = strtok(NULL, " ");
            currentGame->hitCount += fire(currentGame, positionChars);

            //all places have been shot, so we are waiting for an answer from each players
            if(currentGame->hitCount == currentGame->reservedCount) {
                currentGame->waitingAnswerCount = 2;
                sendGameEndedMessage(currentGame);
                sendQuestion(currentGame);
            } else {
                //next round, next gamer
                currentGame->current = (currentGame->current == currentGame->first) ? currentGame->second : currentGame->first;
                currentGame->round++;

                char *message = "Kerem a lovest\n";
                send(currentGame->current->fd, message, strlen(message), 0);
            }
        } else if(strcmp(firstPart, "TABLA") == 0) {
            printTableForGamer(currentGame, clientfd);
        } else if(strcmp(firstPart, "IGEN") == 0 && currentGame->current->fd == clientfd && currentGame->waitingAnswerCount--) { //handle answers only of the current player
            currentGame->current->newGameChoice = 1;

            //next round, next gamer
            currentGame->current = (currentGame->current == currentGame->first) ? currentGame->second : currentGame->first;
            currentGame->round++;

            if(!currentGame->waitingAnswerCount) {
                if(currentGame->first->newGameChoice && currentGame->second->newGameChoice) {
                    resetGame(currentGame);
                } else {
                    exitGame(currentGame);
                    return 0;
                }
            } else {
                sendQuestion(currentGame);
            }
        } else if(strcmp(firstPart, "KILEP") == 0 ) {
            abortGame(currentGame);
            return 0;
        } else if(strcmp(firstPart, "NEM") == 0 && currentGame->current->fd == clientfd && currentGame->waitingAnswerCount--) {
            currentGame->current->newGameChoice = 0;

            //next round, next gamer
            currentGame->current = (currentGame->current == currentGame->first) ? currentGame->second : currentGame->first;
            currentGame->round++;

            if(!currentGame->waitingAnswerCount) {
                exitGame(currentGame);
                return 0;
            } else {
                sendQuestion(currentGame);
            }
        } else if(strcmp(firstPart, "SZABALY") == 0) {
            sendRules(clientfd);
        }
    }


    return 1;

}




int main(void) {
    int listener = setupListenerSocket();


    int cnum = 0;

    fd_set master, readfds;
    int fdmax = listener;
    int i, j;
    int newfd;


    game *currentGame;


    FD_ZERO(&master);
    FD_ZERO(&readfds);
    FD_SET(listener, &master);


    while(1) {
        readfds = master;

        select(fdmax + 1, &readfds, NULL, NULL, NULL);

        for(i = 0; i<=fdmax; i++) {
            if(FD_ISSET(i, &readfds)) {
                if(i == listener) {
                    newfd = accept(listener, 0, 0);

                    if(cnum==CLIENTNUM) {
                        pid_t pid = fork();
                        if(pid==0) {//child, two clients
                            close(listener);
                            FD_CLR(listener,&master);
                            close(newfd);
                        } else {//parent, remaining first client, wait for second client
                            for(j=0;j<=fdmax;j++) {
                                if((FD_ISSET(j,&master)) && (j!=listener)) {
                                    close(j);
                                }
                            }
                            FD_ZERO(&master);
                            FD_SET(listener,&master);
                            FD_SET(newfd,&master);
                            fdmax = (listener>newfd)?listener:newfd;

                            currentGame = createGame(newfd);    //create new game
                            cnum = 1;

                            //greet first player
                            char *message = "Udvozollek a jatekban. Jelenleg a masodik jatekosra varunk.\n";
                            send(newfd, message, strlen(message), 0);
                        }
                    } else {
                        FD_SET(newfd,&master);
                        if(fdmax<newfd) fdmax = newfd;

                        if(cnum == 0) {
                            currentGame = createGame(newfd);    //first game

                            //greet first player
                            char *message = "Udvozollek a jatekban. Jelenleg a masodik jatekosra varunk.\n";
                            send(newfd, message, strlen(message), 0);
                        } else {
                            currentGame->second = createGamer(newfd);   //second gamer joined

                            //wait for first shoot
                            char *message = "Kerem a lovest\n";
                            send(currentGame->current->fd, message, strlen(message), 0);
                        }

                        cnum++;
                    }

                } else {
                    char buff[20] = "";
                    int recvbytes = recv(i, buff, sizeof(buff), 0);

                    if(recvbytes == 0) {
                        abortGame(currentGame);
                        FD_CLR(i, &master);
                        FD_ZERO(&master);
                        FD_SET(listener, &master);
                        cnum = 0;
                        fdmax = listener;
                        currentGame = 0;
                    } else {
                        buff[recvbytes-1]='\0';
                        int result = processClientMessage(buff, currentGame, i);
                        if(!result) {
                            FD_ZERO(&master);
                            FD_SET(listener, &master);
                            cnum = 0;

                            fdmax = listener;
                            currentGame = 0;
                        }
                    }
                }
            }
        }
    }



    close(listener);

    return 0;
}
