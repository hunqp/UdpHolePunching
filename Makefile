all:
	g++ -c peer0.cpp -o peer0.o
	g++ -c peer1.cpp -o peer1.o
	gcc -c STUNExternalIP.c -o STUNExternalIP.o
	g++ peer0.o STUNExternalIP.o -o Peer0 -I. -lpthread -lmosquitto -lrt
	g++ peer1.o STUNExternalIP.o -o Peer1 -I. -lpthread -lmosquitto -lrt

clean:
	rm -rf *.o Peer1 Peer0
