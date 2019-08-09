OBJ:=timerService.o
CFLAGS:=-fPIC -c -Wall -O -g
TARGET=libtimerService.so
LIB=-lpthread

all:$(TARGET)

$(TARGET):$(OBJ)
	$(CC) -shared -o $@ $(OBJ) $(LIB) 

$(OBJ):%.o:%.c
	$(CC) $(CFLAGS) $< -o $@

.PHONY:clean
clean:
	rm -rf $(OBJ) $(TARGET)

