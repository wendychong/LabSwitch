# Make file

net367: host.o packet.o man.o main.o net.o switch.o
	gcc -o net367 host.o man.o main.o net.o packet.o switch.o

main.o: main.c
	gcc -c main.c

host.o: host.c
	gcc -c host.c

man.o:  man.c
	gcc -c man.c

net.o:  net.c
	gcc -c net.c

packet.o:  packet.c
	gcc -c packet.c

switch.o: switch.c
	gcc -c switch.c

clean:
	rm *.o

