all:
	g++ -o Peer0 peer0.cpp STUNExternalIP.cpp -I. -lpthread -lmosquitto -lrt