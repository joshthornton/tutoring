CC = gcc
CFLAGS =-Wall -Werror -g --std=gnu99 -O0
SRC =
BIN =release/
LIB =../common/
INCLUDE=-I/usr/local/include/hiredis/
LIBRARIES=-lhiredis -lfcgi -lcrypto

login: $(SRC)login.c $(LIB)common.c $(LIB)json.h
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC)login.c $(LIB)common.c $(LIB)json.c -o $(BIN)login $(LIBRARIES) 

register: $(SRC)register.c $(LIB)common.c $(LIB)json.h
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC)register.c $(LIB)common.c $(LIB)json.c -o $(BIN)register $(LIBRARIES) 

save: $(SRC)save.c $(LIB)common.c $(LIB)json.h $(SRC)session.c
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC)save.c $(SRC)session.c $(LIB)common.c $(LIB)json.c -o $(BIN)save $(LIBRARIES) 

students: $(SRC)students.c $(LIB)common.c $(LIB)json.h $(SRC)session.c
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC)students.c $(SRC)session.c $(LIB)common.c $(LIB)json.c -o $(BIN)students $(LIBRARIES) 

tutorials: $(SRC)tutorials.c $(LIB)common.c $(LIB)json.h $(SRC)session.c
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC)tutorials.c $(SRC)session.c $(LIB)common.c $(LIB)json.c -o $(BIN)tutorials $(LIBRARIES) 

deploy: login register save students tutorials
	-killall -q login
	-killall -q register 
	-killall -q save 
	-killall -q students 
	-killall -q tutorials 
	spawn-fcgi -a127.0.0.1 -p9001 -n ./$(BIN)login &
	spawn-fcgi -a127.0.0.1 -p9002 -n ./$(BIN)register &
	spawn-fcgi -a127.0.0.1 -p9003 -n ./$(BIN)save &
	spawn-fcgi -a127.0.0.1 -p9004 -n ./$(BIN)students &
	spawn-fcgi -a127.0.0.1 -p9005 -n ./$(BIN)tutorials &
	cp -r www/* /usr/share/nginx/tutoring/www/


clean:
	rm -rf $(BIN)*
