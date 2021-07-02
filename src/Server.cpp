#include <Server.hpp>
#include <Message.hpp>

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <queue>


#define DEFAULT_PORT 12346

// used by getStatus
#define STATUS_READ   0x1
#define STATUS_WRITE  0x2
#define STATUS_EXCEPT 0x4

#ifdef _WIN32

	#include <WS2tcpip.h>

	#pragma comment(lib, "Ws2_32.lib")

#endif // _WIN32

namespace cl
{

Server::Server(uint16_t port)
: m_socket(INVALID_SOCKET)
, m_continueExecution(true)
, m_thread(&Server::init, this, port)
{
}

Server::~Server()
{
	stop();
}

void Server::stop()
{
	std::cout << "[COMMS SERVER] Stopping..." << std::endl;
	m_continueExecution.store(false);
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	if (m_thread.joinable()) m_thread.join();

#ifdef _WIN32

	if (m_socket != INVALID_SOCKET)
	{
		int result = closesocket(m_socket); // close server; don't shut it down
		if (result != 0)
		{
			std::cout << "[COMMS SERVER] Error at closesocket() (" << WSAGetLastError() << "). Could not stop comms server." << std::endl;
			WSACleanup();
			return;
		}
		m_socket = INVALID_SOCKET;
		std::cout << "[COMMS SERVER] Server successfully stopped." << std::endl;
	}
	WSACleanup();

#endif // _WIN32
}

bool Server::isRunning() const
{
	return m_continueExecution.load();
}

void Server::printClients(bool showPing) const
{
	if (m_clients.empty())
	{
		std::cout << "No clients connected." << std::endl;
		return;
	}

	std::cout << "Index    RLBot ID      Team     Key            Address       Port    Last Message";
	if (showPing) std::cout << "     ping";
	std::cout << std::endl;

	for (unsigned int i = 0; i < m_clients.size(); i++)
	{
		std::cout << std::setw(5) << i << "    ";
		std::cout << std::setw(8) << (m_clients[i].idAndTeam >> 1) << "    ";
		std::cout << ((m_clients[i].idAndTeam & 0x01) ? "Orange" : "  Blue") << "    ";
		std::cout << "0x" << std::hex << std::setfill('0') << (unsigned int)(m_clients[i].key)
			      << std::setfill(' ') << std::dec << "    ";
		std::string addrString = std::to_string((int)( m_clients[i].address        & 0xFF)) + "."
							   + std::to_string((int)((m_clients[i].address >> 8)  & 0xFF)) + "."
							   + std::to_string((int)((m_clients[i].address >> 16) & 0xFF)) + "."
							   + std::to_string((int)((m_clients[i].address >> 24) & 0xFF));
		std::cout << std::setw(15) << addrString << "    ";
		std::cout << std::setw(7)  << ntohs(m_clients[i].port) << "    ";
		std::cout << std::endl;
	}

}

void Server::pingClients()
{

}

bool Server::init(uint16_t port)
{
#ifdef _WIN32

	// https://pastebin.com/JkGnQyPX
	// https://www.sfml-dev.org/tutorials/2.5/network-socket.php

	sockaddr_in bindAddress;

	// create server bind address
	ZeroMemory(&bindAddress, sizeof(bindAddress)); // initialize to zero
	bindAddress.sin_family      = AF_INET;
	bindAddress.sin_addr.s_addr = INADDR_ANY;

	port = (port <= 450) ? DEFAULT_PORT : port;
	std::cout << "[COMMS SERVER] Setting up on port : " << port << "." << std::endl;
	// big-endian conversion of port 16 bit value
	bindAddress.sin_port   = htons(port);

	// initialize server socket
	WSADATA wsaData;
	int     result;

	// Initialize Winsock
	result = WSAStartup(MAKEWORD(2, 2), &wsaData); // initialize version 2.2
	if (result != 0)
	{
		std::cout << "[COMMS SERVER] WSAStartup failed (" << result << "). Could not start comms server." << std::endl;
		m_continueExecution.store(false);
		return false;
	}

	// Create a socket for the server to listen for client connections
	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// Check for errors to ensure the socket is valid
	if (m_socket == INVALID_SOCKET)
	{
		std::cout << "[COMMS SERVER] Error at socket() (" << WSAGetLastError() << "). Could not start comms server." << std::endl;
		WSACleanup();
		m_continueExecution.store(false);
		return false;
	}

	// 1 to set non-blocking, 0 to set blocking
	unsigned long nonBlocking = 1;
	if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
	{
		std::cout << "[COMMS SERVER] Error at ioctlsocket() (" << WSAGetLastError() << "). Could not start comms server." << std::endl;
		WSACleanup();
		m_continueExecution.store(false);
		return false;
	}

	// Bind the UDP socket
	result = bind(m_socket, reinterpret_cast<sockaddr*>(&bindAddress), sizeof(bindAddress));
	if (result == SOCKET_ERROR)
	{
		std::cout << "[COMMS SERVER] Could not bind socket (" << WSAGetLastError() << "). Could not start comms sever." << std::endl;
		closesocket(m_socket);
		WSACleanup();
		m_continueExecution.store(false);
		m_socket = INVALID_SOCKET;
		return false;
	}

	std::cout << "[COMMS SERVER] Started server on port: " << ntohs(bindAddress.sin_port) << "." << std::endl;

#endif // _WIN32

	run();

	return true;
}

void Server::run()
{
	Message       receiveMessage;
	uint32_t      senderAddress;
	uint16_t      senderPort;
	ReceiveStatus receiveStatus;

	Message handledMessage;

	// TODO: Reduce CPU usage. The server doesn't have to run at much more than 120 updates per second...
	while (m_continueExecution.load())
	{
		// loop until there is no more data to be read
		while ((receiveStatus = receive(receiveMessage, senderAddress, senderPort)) != ReceiveStatus::NoData)
		{
			if (receiveStatus == ReceiveStatus::Error)      break;         // handled outside of loop
			if (receiveStatus == ReceiveStatus::Oversized)  continue;      // skip oversized packet (maybe throw away excess data)
			if (receiveStatus == ReceiveStatus::Undersized) continue;      // skip undersized packet (maybe replace missing data by 0)
			if (receiveStatus == ReceiveStatus::ConnReset)
			{
				pingClients(); // find which client disconnected
				continue;
			}

			handledMessage = handleMessage(receiveMessage, senderAddress, senderPort);
			if (handledMessage.type != MSG_INVALID)
				m_messageBuffer.push(handledMessage);
		}

		// there was an error while calling receive(). Stop the server.
		if (receiveStatus == ReceiveStatus::Error)
		{
			std::cout << "[COMMS SERVER] Error while receiving data. Stopping server." << std::endl;
			m_continueExecution.store(false);
			break;
		}

		if (m_messageBuffer.size() > 0)
		{
			std::cout << "[COMMS SERVER] Received " << m_messageBuffer.size() * sizeof(Message) << " bytes from clients." << std::endl;
			while (!m_messageBuffer.empty())
			{
				// tailor message for all clients
				// NOTE: for multi-byte values sent across the network
				// htonl or htons should be used to convert 4-byte and
				// 2-byte values respectively.
				Message currentMessage = m_messageBuffer.front();

				for (unsigned int i = 0; i < m_clients.size(); ++i)
				{
					if (m_clients[i].address == INADDR_ANY)
						continue;

					// check if client should receive the message
					// and tailor message for client
					Message clientMessage = currentMessage;
					clientMessage.key = m_clients[i].key;

					if (!send(&clientMessage, m_clients[i]))
					{
						std::cout << "[COMMS SERVER] Client #" << i << " with id: "
								  << (m_clients[i].idAndTeam >> 1) << " send error." << std::endl;

						m_clients[i].address = INADDR_ANY;
						m_clients[i].port    = 0;

						Message disconnectMessage;
						disconnectMessage.parameters = MSG_ALL;
						disconnectMessage.type       = MSG_DISCONNECT;
						disconnectMessage.data[0]    = m_clients[i].idAndTeam >> 1;
						m_messageBuffer.push(disconnectMessage);
					}
				}

				m_messageBuffer.pop();
			}
		}

		// remove released sockets from list, backwards to avoid skipping some clients
		if (m_clients.size() > 0)
		{
			for (int i = (signed)m_clients.size() - 1; i >= 0; --i)
			{
				if (m_clients[i].address == INADDR_ANY)
					m_clients.erase(m_clients.begin() + i);
			}
		}
	}

	std::cout << "[COMMS SERVER] Server loop stopped successfully." << std::endl;
}

bool Server::send(Message* message, const Client& recipient) const
{
#ifdef _WIN32

	if (m_socket == INVALID_SOCKET)
	{
		std::cout << "[COMMS SERVER] Invalid socket. Cannot send message." << std::endl;
		return false;
	}

	sockaddr_in recipientAddress;
	ZeroMemory(&recipientAddress, sizeof(recipientAddress)); // initialize to zero
	recipientAddress.sin_family      = AF_INET;
	recipientAddress.sin_addr.s_addr = recipient.address;
	recipientAddress.sin_port        = recipient.port;

	int sendResult = sendto(m_socket, reinterpret_cast<char*>(message),
		static_cast<int>(sizeof(Message)), 0, reinterpret_cast<sockaddr*>(&recipientAddress),
		sizeof(recipientAddress));

	if (sendResult < 0)
	{
		std::cout << "[COMMS SERVER] Error at sendto() (" << WSAGetLastError() << "). Message not sent." << std::endl;
		return false;
	}

#endif // _WIN32

	return true;
}

Server::ReceiveStatus Server::receive(Message& message, uint32_t& ipAddress, uint16_t& port)
{
#ifdef _WIN32

	if (m_socket == INVALID_SOCKET)
	{
		std::cout << "[COMMS SERVER] Invalid socket. Cannot receive message." << std::endl;
		return ReceiveStatus::Error;
	}

	std::size_t receivedSize = 0;
	ipAddress                = INADDR_ANY;
	port                     = 0;

	sockaddr_in senderAddress;
	ZeroMemory(&senderAddress, sizeof(sockaddr_in));
	senderAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	senderAddress.sin_family      = AF_INET;
	senderAddress.sin_port        = 0;

	int addressSize = static_cast<int>(sizeof(sockaddr_in));
	int sizeReceived = recvfrom(m_socket, reinterpret_cast<char*>(&message),
		static_cast<int>(sizeof(Message)), 0, reinterpret_cast<sockaddr*>(&senderAddress), &addressSize);
	
	if (sizeReceived == 0)
	{
		return Server::ReceiveStatus::NoData;
	}
	else if (sizeReceived < 0)
	{
		int errorCode = WSAGetLastError();
		if (errorCode == WSAEWOULDBLOCK)
		{
			// no data to be read
			return Server::ReceiveStatus::NoData;
		}
		else if (errorCode == WSAEMSGSIZE)
		{
			std::cout << "[COMMS SERVER] Warning: oversized datagram received. Data truncated or ignored." << std::endl;
			// too much data. Datagram truncated
			ipAddress = senderAddress.sin_addr.s_addr; // we'll keep the windows formatting (no ntohl, etc.)
			port      = senderAddress.sin_port;
			return Server::ReceiveStatus::Oversized;
		}
		else if (errorCode == WSAECONNRESET)
		{
			// there was an error when sending a packet to a client.
			ipAddress = senderAddress.sin_addr.s_addr;
			port      = senderAddress.sin_port;
			//std::cout << "[COMMS SERVER] Warning: WSACONNRESET. Disconnecting client." << std::endl;
			for (int i = 0; i < m_clients.size(); i++)
			{
				if (m_clients[i].port == port)
				{
					disconnectClient(i);

					Message disconnectMessage;
					disconnectMessage.playerIDAndTeam = m_clients[i].idAndTeam;
					disconnectMessage.key             = DEFAULT_KEY;
					disconnectMessage.type            = MSG_DISCONNECT;
					disconnectMessage.parameters      = MSG_ALL;
					disconnectMessage.data[0]         = MSG_DISCONNECT_SRC_SERVER;

					m_messageBuffer.push(disconnectMessage);

					// We only disconnect the first client in the list because
					// it is sorted by join time (from first to last) meaning
					// that if a client were to incorectly disconnect and then
					// reconnect later, we only delete the old invalid client.
					break;
				}
			}
			return Server::ReceiveStatus::ConnReset;
		}
		std::cout << "[COMMS SERVER] recvfrom() failed with error: " << errorCode << "." << std::endl;
		return Server::ReceiveStatus::Error;
	}
	else if (sizeReceived > static_cast<int>(sizeof(Message)))
	{
		std::cout << "[COMMS SERVER] Received oversized datagram." << std::endl;
		return Server::ReceiveStatus::Oversized;
	}
	else if (sizeReceived < static_cast<int>(sizeof(Message)))
	{
		std::cout << "[COMMS SERVER] Received undersized datagram." << std::endl;
		return Server::ReceiveStatus::Undersized;
	}

	// SUCCESS, proceed to filling in information
	// Message was already filled in recvfrom()
	ipAddress = senderAddress.sin_addr.s_addr; // we'll keep the windows formatting (no ntohl, etc.)
	port      = senderAddress.sin_port;

#endif // _WIN32
	
	return Server::ReceiveStatus::Success;
}

Message Server::handleMessage(const Message& receivedMessage, const uint32_t& senderAddress, const uint16_t& senderPort)
{
	Message t_message = receivedMessage;

	if (receivedMessage.type == MSG_CONNECT)
	{
		t_message = handleConnectionMessage(receivedMessage, senderAddress, senderPort);
	}
	else if (receivedMessage.type == MSG_DISCONNECT)
	{
		for (int i = 0; i < m_clients.size(); i++)
		{
			if (m_clients[i].idAndTeam == receivedMessage.playerIDAndTeam)
			{
				disconnectClient(i);

				t_message.parameters |= MSG_ALL;
				t_message.data[0]    =  MSG_DISCONNECT_SRC_CLIENT;
			}
		}
	}
	else
	{
		std::cout << "[COMMS SERVER] Message of type: " << (int)receivedMessage.type << "not handled." << std::endl;
	}

	t_message.key = DEFAULT_KEY;
	return t_message;
}

Message Server::handleConnectionMessage(const Message& connectionMessage, const uint32_t& senderAddress, const uint16_t& senderPort)
{
	std::cout << "[COMMS SERVER] Received connection message!" << std::endl;

	Message outputMessage;
	outputMessage.type = MSG_INVALID;
	
	Client newClient;
	newClient.address   = senderAddress;
	newClient.key       = connectionMessage.key;
	newClient.port      = senderPort;
	newClient.idAndTeam = connectionMessage.playerIDAndTeam;

	if (m_clients.size() >= 128)
	{
		std::cout << "[COMMS SERVER] Too many clients already connected. Ignoring connection request." << std::endl;

		sendErrorMessage(newClient, MSG_ERR_TOO_MANY);

		return outputMessage;
	}
	if (connectionMessage.key != DEFAULT_KEY)
	{
		std::cout << "[COMMS SERVER] Received invalid connection message. Ignoring connection request." << std::endl;

		sendErrorMessage(newClient, MSG_ERR_INVALID_CON);

		return outputMessage;
	}

	for (int i = 0; i < m_clients.size(); i++)
	{
		// no need to check for port/address, as we can't bind two UDP sockets to the same port on the same machine

		if (m_clients[i].idAndTeam == newClient.idAndTeam)
		{
			std::cout << "[COMMS SERVER] Received connection packet form already connect client with id: " << (m_clients[i].idAndTeam >> 1) << "." << std::endl;

			sendErrorMessage(newClient, MSG_ERR_ALREADY_CON);

			return outputMessage;
		}

		if (m_clients[i].address == newClient.address && m_clients[i].port == newClient.port)
		{
			// the client was not disconnected properly and reconnected later
			// we remove the old one because it is invalid (we can only bind
			// one UDP socket to the same port on the same machine)
			disconnectClient(i);

			Message disconnectMessage;
			disconnectMessage.playerIDAndTeam = m_clients[i].idAndTeam;
			disconnectMessage.key             = DEFAULT_KEY;
			disconnectMessage.type            = MSG_DISCONNECT;
			disconnectMessage.parameters      = MSG_ALL;
			disconnectMessage.data[0]         = MSG_DISCONNECT_SRC_SERVER;

			m_messageBuffer.push(disconnectMessage);
		}
	}

	// SETUP CLIENT

	// generate new key
	newClient.key = generateKey(reinterpret_cast<uint8_t const*>(&connectionMessage));

	m_clients.push_back(newClient);

	outputMessage.playerIDAndTeam = newClient.idAndTeam;
	outputMessage.type            = MSG_CONNECT;
	outputMessage.parameters      |= MSG_ALL;
	
	return outputMessage;
}

bool Server::sendErrorMessage(const Client& recipient, uint8_t errorCode) const
{
	Message errMessage;
	errMessage.playerIDAndTeam = recipient.idAndTeam;
	errMessage.key             = recipient.key;
	errMessage.parameters      = MSG_PRIVATE;
	errMessage.type            = MSG_ERROR;
	errMessage.data[0]         = errorCode;

	return send(&errMessage, recipient);
}

void Server::disconnectClient(const int& clientIndex)
{
	m_clients[clientIndex].address = INADDR_ANY;
	m_clients[clientIndex].port    = 0;
}

uint8_t Server::generateKey(uint8_t const* messageData) const
{
	uint8_t result = *messageData ^ *(messageData + 1);
	result ^= *(messageData + ((result ^ 0x3F) % 32));
	result ^= *(messageData + 32 + ((result ^ 0x11) % 32));

	return result;
}

} // cl
