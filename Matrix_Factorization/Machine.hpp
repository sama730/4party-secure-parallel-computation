#ifndef MACHINE_H__
#define MACHINE_H__

#pragma once

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "Network.hpp"
#include "IPManager.hpp"

// #ifdef UNIX_PLATFORM

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>

#include "Utility/CryptoUtility.h"
#include "Utility/Communicator.h"

#define MAX_PACKAGE_SIZE 1073741824

using namespace std;
using namespace Utility;

class Machine 
{ 
public:
	int totalMachines;  // number of processor
	int totalParties;   // number of parties 
	int machineId;		// 
	int party;
	int numberOfOutgoingParties;
	int numberOfIncomingParties;
	int numberOfOutgoingPeers;
	int numberOfIncomingPeers;
	int layerBasePort; 	// solve the problem of parties trying to connect to processor in other layers
	
	vector <Network *> partiesDown;
	vector <Network *> partiesUp;
	vector <Network *> peersDown;
	vector <Network *> peersUp;

	Machine (int totalMachines, int machineId, int party) 
	{
		this->totalMachines = totalMachines;
		this->totalParties = 4;
		this->machineId = machineId;
		this->party = party;
		this->numberOfIncomingParties = (this->totalParties - this->party - 1);
		this->numberOfOutgoingParties = (this->party);
		this->numberOfIncomingPeers = (this->totalMachines - this->machineId - 1);
		this->numberOfOutgoingPeers = this->machineId;
		layerBasePort = totalMachines * 2;

		partiesDown.resize(this->numberOfIncomingParties);
		partiesUp.resize(this->numberOfOutgoingParties);
		peersDown.resize(this->numberOfIncomingPeers);
		peersUp.resize(this->numberOfOutgoingPeers);
	}

	~Machine()
	{
		for(int idx = 0; idx < this->numberOfIncomingParties; idx++) delete partiesDown[idx];
		for(int idx = 0; idx < this->numberOfOutgoingParties; idx++) delete partiesUp[idx];
		for(int idx = 0; idx < this->numberOfIncomingPeers; idx++) delete peersDown[idx];
		for(int idx = 0; idx < this->numberOfOutgoingPeers; idx++) delete peersUp[idx];
	}
	

	void connecting(int peerBasePort, int partyBasePort) 
	{
		connectToOtherPeers(peerBasePort, peersDown, peersUp, numberOfIncomingPeers, numberOfOutgoingPeers);
		cout << " all peers are connected!" << endl;
		usleep(1000);
		connectToOtherParties(partyBasePort, partiesDown, partiesUp, numberOfIncomingParties, numberOfOutgoingParties);
		cout << " all parties are connected!" << endl;
	}

	void connectToOtherParties(int partyBasePort, vector <Network *> &partiesDown, vector <Network *> &partiesUp, int numberOfIncomingParties, int numberOfOutgoingParties) 
	{
		listeningServers(party, partyBasePort + machineId*layerBasePort + party, partiesDown, numberOfIncomingParties);
        connectingClients(party, partyBasePort + machineId*layerBasePort, partiesUp, numberOfOutgoingParties, true);
	}

	void connectToOtherPeers(int peerBasePort, vector <Network *> &peersDown, vector <Network *> &peersUp, int numberOfIncomingPeers, int numberOfOutgoingPeers)
	{
		listeningServers(machineId, peerBasePort + party*layerBasePort + machineId, peersDown, numberOfIncomingPeers);
        connectingClients(machineId, peerBasePort + party*layerBasePort, peersUp, numberOfOutgoingPeers, false);
	}


	void listeningServers(int myId, int serverPort, vector <Network *> &DownList, int numberOfIncomingConnections) 
	{
		int opt = 1, activity;
		int * index;
		char buffer[100];
		struct sockaddr_in serverSocket, clientSock;
		fd_set working_set;
		const char * msg = "Accept"; 
		struct timeval timeout;
		timeout.tv_sec  = 20;
   		timeout.tv_usec = 0;
		socklen_t clientSocksize = sizeof(clientSock);

		int server_fd, client_fd;
		if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) { perror("socket failed"); exit(1); }
		if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) { perror("setsockopt"); exit(1); } //| SO_REUSEPORT
		// if ((ioctl(server_fd, FIONBIO, (char *)&opt)) < 0) { perror("socket nonblocking failed"); exit(1); }
		memset(&serverSocket, 0, sizeof(serverSocket));
		serverSocket.sin_family = AF_INET;
		serverSocket.sin_addr.s_addr = htonl(INADDR_ANY); // set our address to any interface
		serverSocket.sin_port = htons(serverPort);        // set the server port number 

		if (bind(server_fd, (struct sockaddr *)&serverSocket, sizeof(serverSocket)) < 0) { cout<< "server_fd: " << server_fd; perror("error: bind"); exit(1); }

		if (listen(server_fd, 1024) < 0) { perror("error: listen"); exit(1); }
		cout << "Listeting on port: " << serverPort << endl;

		FD_ZERO(&working_set); //clear the socket set 
		FD_SET(server_fd, &working_set); //add master socket to set

		// cout << myId << " numberOfIncomingConnections: " << numberOfIncomingConnections << endl;
		for (int k = 0; k < numberOfIncomingConnections; k++) 
		{
			while(1)
			{
				// cout << " C ***********" << endl;
				activity = select(server_fd+1 , &working_set , NULL , NULL , NULL);   //&timeout
				// cout << " A ***********" << endl;
				if ((activity < 0) && (errno!=EINTR))  { perror("select failed"); exit(1); }
				// cout << " B ***********" << endl;
				if (FD_ISSET(server_fd, &working_set)) 
				{
					if ((client_fd = accept(server_fd, (struct sockaddr *)&clientSock, &clientSocksize)) < 0) { perror("error: accept"); exit(1); }
					// cout << "server_fd: " << server_fd << " client_fd: " << client_fd << endl;
					if (send(client_fd, msg, strlen(msg), 0) < 0) { perror("error: send"); exit(1); };
					Network * channel = new Network();
		        	channel->establishedChannel(client_fd);
		        	
		        	int bytes_received = recv(client_fd, buffer ,sizeof(buffer) , 0);
		        	int machineIndex = ((int *)buffer)[0];
		        	// channel->recv_data(&index, 1);
		        	// cout << myId << " accepted connection from " << machineIndex << " stored in its DownList " << (machineIndex - myId - 1) << endl;
					machineIndex = (machineIndex - myId - 1);
					DownList[machineIndex] = channel;
					// DownList[machineIndex]->recv_data(&machineIndex, 1);
					break;
				}
			}
		}
	}

	void connectingClients(int myId, int serverPort, vector <Network *> &UpList, int numberOfOutgoingConnections, bool partyFlag) 
	{ 
		IPManager * ipManager = new IPManager(totalMachines);
		string serverIp;
		int client_fd;
		int accepted_flag = -1;
		char buffer[10];
		for (int i = 0; i < numberOfOutgoingConnections; i++) 
		{			
			if (partyFlag)
				serverIp = ipManager->P_Ip[i][machineId];
			else
				serverIp = ipManager->P_Ip[party][i];
			// cout << "Client: server " << i << " is at ip: " << serverIp << " port: " << serverPort + i << endl;			
			struct sockaddr_in serverSocket;
			memset(&serverSocket, 0, sizeof(serverSocket));
			serverSocket.sin_family = AF_INET;
			serverSocket.sin_addr.s_addr = inet_addr(serverIp.c_str());
			serverSocket.sin_port = htons(serverPort + i);
			if ((client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == 0) { perror("socket failed"); exit(1); }

			do {
				usleep(1000);
				// cout << "My client_fd: " << client_fd << endl;
				if (connect(client_fd, (struct sockaddr *)&serverSocket, sizeof(serverSocket)) < 0) { perror("connection failed"); close(client_fd); exit(1); };
				accepted_flag = read(client_fd, buffer, 10);
			} while (accepted_flag < 0);
			
			Network * channel = new Network();
			channel->establishedChannel(client_fd);
        	UpList[i] = channel;
        	// cout << myId << " requested to connect to " << i << " stored in its UpList " << i <<  endl;
        	if (send(client_fd, &myId, sizeof(myId), 0) < 0) { perror("error: send"); exit(1); };
        	// UpList[i]->send_data(&myId, 1);
		}
	}
	
	void sendToParty(int party1, int party2, unsigned char *msg, uint64_t size) // party1 sends to party2
	{
		// std::cout << "send to party: " << party1 << " --> " << party2 << std::endl;
		Network * channel;
		if (party1 < party2)
			channel = partiesDown[party2-party1-1];
		else if (party1 > party2)
			channel = partiesUp[party2];

		if(size > 1024*1024) std::cout << "sending " << size << " bytes" << std::endl;
		
		uint64_t count = 0;
		uint64_t bufferSize;
		
		while(count < size)
		{
			if (count + MAX_PACKAGE_SIZE < size)
			{
				bufferSize = MAX_PACKAGE_SIZE;
			}
			else
			{
				bufferSize = size - count;
			}
			
			uint64_t bytes_sent = 0;

			while(bytes_sent < bufferSize)
			{
				uint64_t bs = send(channel->sock_fd, msg + count, bufferSize - bytes_sent, 0);

				if(bs < 0)
				{
					perror("error: send");
					exit(1);
				}

				bytes_sent += bs;
				count += bs;
			}
		}
		
		// if (send(channel->sock_fd, msg, size, 0) < 0) { perror("error: send"); exit(1); };
	}

	uint64_t receiveFromParty(int party1, int party2, unsigned char *msg, uint64_t size) // party1 receives from party2
	{
		// std::cout << "receiver from party: " << party1 << " <-- " << party2 << std::endl;
		Network * channel;
		if (party1 < party2)
			channel = partiesDown[party2-party1-1];
		else if (party1 > party2)
			channel = partiesUp[party2];

		int sd = channel->sock_fd;
		fd_set reading_set;
		
		while (true)
		{
			FD_ZERO(&reading_set); //clear the socket set 
			FD_SET(sd, &reading_set); //add master socket to set
			int activity = select(sd+1, &reading_set , NULL , NULL , NULL);
			if ((activity < 0) && (errno!=EINTR))  { perror("select failed"); exit(1); }
			if (activity > 0)
			{
				// if (FD_ISSET(sd, &reading_set)){
				// 	int bytes_received = recv(sd, msg, bufferSize, 0);

				// 	return bytes_received;
				// }
				if (FD_ISSET(sd, &reading_set)){
					uint64_t count = 0;
					uint64_t bufferSize;
					uint64_t bytes_received = 0;
					
					while(count < size)
					{
						if (count + MAX_PACKAGE_SIZE < size)
						{
							bufferSize = MAX_PACKAGE_SIZE;
						}
						else
						{
							bufferSize = size - count;
						}
						
						uint64_t arrived = 0;

						while (arrived < bufferSize){
							uint64_t br = recv(sd, msg + count, bufferSize - arrived, 0);
							arrived += br;
							count += br;
						}

						bytes_received += arrived;

						// count += MAX_PACKAGE_SIZE;
					}
					assert(bytes_received == size);
					return bytes_received;
				}
			}
			else if (activity == 0){
				return 0;
			}
		}
	}

	void sendToPeer(int peer1, int peer2, unsigned char *msg, uint64_t size)
	{
		// std::cout << "send to peer: " << peer1 << " --> " << peer2 << std::endl;

		Network * channel;
		if (peer1 < peer2)
			channel = peersDown[peer2-peer1-1];
		else if (peer1 > peer2)
			channel = peersUp[peer2];

		if(size > 1024*1024) std::cout << peer1 << " --> " << peer2 << " " << size << " bytes" << std::endl;
		
		uint64_t count = 0;
		uint64_t bufferSize;
		
		while(count < size)
		{
			if (count + MAX_PACKAGE_SIZE < size)
			{
				bufferSize = MAX_PACKAGE_SIZE;
			}
			else
			{
				bufferSize = size - count;
			}
			
			uint64_t bytes_sent = 0;

			while(bytes_sent < bufferSize)
			{
				uint64_t bs = send(channel->sock_fd, msg + count, bufferSize - bytes_sent, 0);

				if(bs < 0)
				{
					perror("error: send");
					exit(1);
				}

				bytes_sent += bs;
				count += bs;
			}
		}
		
		// char * buf = (char *)&msg;
		// if (send(channel->sock_fd, msg, size, 0) < 0) { perror("error: send"); exit(1); };
	}

	uint64_t receiveFromPeer(int peer1, int peer2, unsigned char *msg, uint64_t size) // party1 receives from party2
	{
		// std::cout << "receiver from peer: " << peer1 << " <-- " << peer2 << std::endl;
		// if(size > 1024*1024) std::cout << peer1 << " <-- " << peer2 << " " << size << " bytes" << std::endl;

		struct timeval timeout;
		timeout.tv_sec  = 1;
   		timeout.tv_usec = 0;

		Network * channel;
		if (peer1 < peer2)
			channel = peersDown[peer2-peer1-1];
		else if (peer1 > peer2)
			channel = peersUp[peer2];

		int sd = channel->sock_fd;
		fd_set reading_set;
		// char buffer[256];
		while (true)
		{
			FD_ZERO(&reading_set); //clear the socket set 
			FD_SET(sd, &reading_set); //add master socket to set
			int activity = select(sd+1, &reading_set , NULL , NULL , NULL);   //&timeout
			if ((activity < 0) && (errno!=EINTR))  { perror("select failed"); exit(1); }
			// cout << "activity: " << activity << endl;
			if (activity > 0)
			{
				// if (FD_ISSET(sd, &reading_set))
				// {
				// 	// int n = recv(sd, buf, sizeof(buf), 0);
				// 	// int res = fread((char*)buf, sizeof(char), 1, machine->partiesDown[party2-party1-1]->stream);
				// 	int bytes_received = recv(sd, msg, bufferSize, 0);

				// 	return bytes_received;
				// }
				if (FD_ISSET(sd, &reading_set)){
					uint64_t count = 0;
					uint64_t bufferSize;
					uint64_t bytes_received = 0;
					
					while(count < size)
					{
						if (count + MAX_PACKAGE_SIZE < size)
						{
							bufferSize = MAX_PACKAGE_SIZE;
						}
						else
						{
							bufferSize = size - count;
						}
						
						uint64_t arrived = 0;

						while (arrived < bufferSize){
							uint64_t br = recv(sd, msg + count, bufferSize - arrived, 0);
							arrived += br;
							count += br;
						}

						bytes_received += arrived;

						// count += MAX_PACKAGE_SIZE;
					}
					assert(bytes_received == size);
					return bytes_received;
				}
			}
			else if (activity == 0)
				return 0;
		}
	}
	
	// AES SEEDSIZE
	std::vector<unsigned char> getSharedSeedForPeerAndParty(int party, int partner, Communicator *com)
	{
		std::vector<unsigned char> seed(32), seed1, seed2;
		
		// std::cout << "getSharedSeedForPeerAndParty: " << std::endl;
		if (machineId == 0)
		{
			seed1 = CryptoUtility::SampleByteArray(32);
			seed2.resize(32);
			
			if(party % 2)
			{
				com->SendEvaluationPartner(seed1.data(), seed1.size());
				com->AwaitEvaluationPartner(seed2.data(), seed2.size());
			}
			else
			{
				com->AwaitEvaluationPartner(seed2.data(), seed2.size());
				com->SendEvaluationPartner(seed1.data(), seed1.size());
			}
			
			for(int idx = 0; idx < 32; idx++)
			{
				seed[idx] = seed1[idx] + seed2[idx];
			}

			// TestUtility::PrintByteArray(seed, "************ seed: ");
			
			/* machine 0 distribute it to all other peer processors */
			for (int i = 1; i < totalMachines; i++)
			{
				// std::cout << machineId << " Sending to machine: " << i << std::endl;
				sendToPeer(0, i, seed.data(),  seed.size());
				// std::cout << machineId << " Done sending to machine: " << i << std::endl;
			}
		}
		else
		{
			// std::cout << machineId << " receiving from machine: 0" << std::endl;
			receiveFromPeer(machineId, 0, seed.data(), seed.size());
			// std::cout << machineId << " Done receiving from machine: 0" << std::endl;
			// TestUtility::PrintByteArray(seed, "************ seed: ");
		}
		
		return seed;
	}
	
	std::vector<unsigned char> getSharedRandomSeed(int party, int partner, Communicator *com)
	{
		std::vector<unsigned char> seed(32), seed1, seed2;
		
		// std::cout << "getSharedRandomSeed: " << std::endl;
		seed1 = CryptoUtility::SampleByteArray(32);
		seed2.resize(32);
		
		if(party % 2)
		{
			com->SendEvaluationPartner(seed1.data(), seed1.size());
			com->AwaitEvaluationPartner(seed2.data(), seed2.size());
		}
		else
		{
			com->AwaitEvaluationPartner(seed2.data(), seed2.size());
			com->SendEvaluationPartner(seed1.data(), seed1.size());
		}
		
		for(int idx = 0; idx < 32; idx++)
		{
			seed[idx] = seed1[idx] + seed2[idx];
		}
		
		return seed;
	}
};


#endif  //UNIX_PLATFORM
