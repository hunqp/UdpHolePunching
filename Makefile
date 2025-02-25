PUBLISHER = Publisher
SUBSCRIBER = Subscriber

all:
	g++ -c ults.cpp -o ults.o
	g++ -c publisher.cpp -o publisher.o
	g++ -c subscriber.cpp -o subscriber.o
	gcc -c STUNExternalIP.c -o STUNExternalIP.o
	g++ -c peerconnection.cpp -o peerconnection.o
	g++ publisher.o STUNExternalIP.o peerconnection.o ults.o -o $(PUBLISHER) -I. -lpthread -lmosquitto -lrt
	g++ subscriber.o STUNExternalIP.o peerconnection.o ults.o -o $(SUBSCRIBER) -I. -lpthread -lmosquitto -lrt

clean:
	rm -rf *.o $(PUBLISHER) $(SUBSCRIBER)
