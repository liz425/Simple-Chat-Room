all: Client Server
Client: Client.cpp header.hpp
	g++ -std=c++11 Client.cpp -o Client
Server: Server.cpp header.hpp
	g++ -std=c++11 Server.cpp -o Server
.PHONY: clean
clean:
	-rm -f Client Server
