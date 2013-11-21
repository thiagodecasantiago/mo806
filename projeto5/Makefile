.PHONY: clean

clientes=cliente0 cliente1 cliente2

all: $(clientes)

CC=gcc -g3
ZOOKEEPER_INC=-I$(PREFIX)/include/zookeeper
ZOOKEEPER_LIB=-L$(PREFIX)/lib -lzookeeper_mt -DTHREADED


cliente0: src/cliente0.c barrier.o
	$(CC) $(ZOOKEEPER_INC) $(ZOOKEEPER_LIB) $< barrier.o -o $@

cliente1: src/cliente1.c barrier.o
	$(CC) $(ZOOKEEPER_INC) $(ZOOKEEPER_LIB) $< barrier.o -o $@

cliente2: src/cliente1.c barrier.o
	$(CC) $(ZOOKEEPER_INC) $(ZOOKEEPER_LIB) $< barrier.o -o $@

barrier.o: src/barrier.c src/barrier.h
	$(CC) $(ZOOKEEPER_INC) $< -c -o $@

clean:
	-@rm cliente0 cliente1 cliente2 barrier.o
