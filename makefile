CC = gcc
CFLAGS = -Wall
TARGETS = libreria.o battle_server battle_client
# Tutti i programmi da voler creare devono essere specificati in "all"
all: $(TARGETS)

#libreria.o: libreria.o
#	$(CC) $(CFLAGS) -c libreria.c

battle_server.o: battle_server.c
	$(CC) $(CFLAGS) -c battle_server.c

battle_server: battle_server.o
	$(CC) $(CFLAGS) -o battle_server battle_server.o libreria.o
	
battle_client.o: battle_client.c
	$(CC) $(CFLAGS) -c battle_client.c
	
battle_client: battle_client.o
	$(CC) $(CFLAGS) -o battle_client battle_client.o libreria.o
	
clean:
	$(RM) *.o
	
clobber:
	$(RM) $(TARGETS)
