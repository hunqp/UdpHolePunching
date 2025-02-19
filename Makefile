all:
	g++ -o Peer0 peer0.cpp STUNExternalIP.cpp -I. -lpthread -lmosquitto -lrt
	g++ -o Peer1 peer1.cpp STUNExternalIP.cpp -I. -lpthread -lmosquitto -lrt

clean:
	rm -rf Peer0
	rm -rf Peer1