#include <Client.hpp>
#include <Message.hpp>

#include <iostream>
#include <random>

#define DEFAULT_PORT 12347

// 127.0.0.1, but after applying htonl
#define LOCALHOST_ADDRESS 0x0100007F

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

Client::Client(uint16_t serverPort, unsigned int id, bool isBlueTeam)
: m_socket(INVALID_SOCKET)
, m_idAndTeam(0)
, m_key(DEFAULT_KEY)
, m_isConnected(false)
{
	m_idAndTeam =  (uint8_t)id << 1;
	m_idAndTeam |= isBlueTeam ? 0x01 : 0x00; // make sure is blue team is only 0x1 and 0x0

	ZeroMemory(&m_serverAddress, sizeof(sockaddr_in));
	m_serverAddress.sin_family      = AF_INET;
	m_serverAddress.sin_port        = htons(serverPort);
	m_serverAddress.sin_addr.s_addr = LOCALHOST_ADDRESS;

	if (!init(serverPort + static_cast<uint16_t>(id) + 1)) // +1 in case id = 0
	{
		std::cout << "[COMMS CLIENT] Failed to intialize client socket." << std::endl;
	}
}

Client::~Client()
{
	close();
}

void Client::close()
{
	std::cout << "[COMMS CLIENT] Closing..." << std::endl;

	Message disconnectMessage;
	disconnectMessage.playerIDAndTeam = m_idAndTeam;
	disconnectMessage.key             = m_key;
	disconnectMessage.type            = MSG_DISCONNECT;
	disconnectMessage.parameters      = MSG_ALL;
	if (!sendMessage(&disconnectMessage, true))
	{
		std::cout << "[COMMS CLIENT] Could not send disconnect message." << std::endl;
	}

	if (m_socket != INVALID_SOCKET)
	{
		int result = closesocket(m_socket);
		if (result != 0)
		{
			std::cout << "Error at closesocket() (" << WSAGetLastError() << "). Could not stop comms client." << std::endl;
			WSACleanup();
			return;
		}
		m_socket = INVALID_SOCKET;
		std::cout << "[COMMS CLIENT] Client successfully stopped." << std::endl;
	}
	WSACleanup();
}

bool Client::init(uint16_t clientPort)
{
#ifdef _WIN32

	// https://pastebin.com/JkGnQyPX

	clientPort = ((clientPort <= 450) ? DEFAULT_PORT : clientPort);

	std::cout << "[COMMS CLIENT] Binding to localhost on port : " << clientPort << std::endl;

	sockaddr_in bindAddress;

	// initialize socket address
	ZeroMemory(&bindAddress, sizeof(bindAddress)); // initialize to zero
	bindAddress.sin_family      = AF_INET;
	// big-endian conversion of port 16 bit value
	bindAddress.sin_port        = htons(clientPort);
	bindAddress.sin_addr.s_addr = INADDR_ANY;

	// initialize client socket
	WSADATA wsaData;
	int     result;

	// Initialize Winsock
	result = WSAStartup(MAKEWORD(2, 2), &wsaData); // initialize version 2.2
	if (result != 0)
	{
		std::cout << "WSAStartup failed (" << result << "). Could not start comms client." << std::endl;
		return false;
	}

	// Create a socket for the Client to listen for client connections
	//m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// Check for errors to ensure the socket is valid
	if (m_socket == INVALID_SOCKET)
	{
		std::cout << "Error at socket() (" << WSAGetLastError() << "). Could not start comms client." << std::endl;
		WSACleanup();
		return false;
	}

	// 1 to set non-blocking, 0 to set blocking
	unsigned long nonBlocking = 1;
	if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
	{
		std::cout << "Error at ioctlsocket() (" << WSAGetLastError() << "). Could not start comms client." << std::endl;
		WSACleanup();
		m_socket = INVALID_SOCKET;
		return false;
	}

	// Setup the UDP socket
	result = bind(m_socket, reinterpret_cast<sockaddr*>(&bindAddress), sizeof(bindAddress));
	if (result == SOCKET_ERROR)
	{
		std::cout << "Could not bind socket (" << WSAGetLastError() << "). Could not start comms sever." << std::endl;
		closesocket(m_socket);
		WSACleanup();
		m_socket = INVALID_SOCKET;
		return false;
	}

	std::cout << "[COMMS CLIENT] Started client on port: " << clientPort << std::endl;

#endif // _WIN32

	return attemptConnection();
}

void Client::update(float dt)
{
	// handle message as soon as we receive them
	// add them to queue if it is any other type of message
	// if the message is unknown and > 127 (not user-defined),
	// ignore it here.

	receiveMessages();

	if (!m_isConnected)
	{
		attemptConnection();
	}
}

bool Client::sendMessage(Message* message, bool force)
{
	if (!force && !m_isConnected)
	{
		std::cout << "[COMMS CLIENT] Client not connected. Will not send message." << std::endl;
		return false;
	}

	if (m_socket == INVALID_SOCKET)
	{
		return false;
	}

	int sendResult = sendto(m_socket, reinterpret_cast<char*>(message),
		static_cast<int>(sizeof(Message)), 0, reinterpret_cast<sockaddr*>(&m_serverAddress),
		sizeof(m_serverAddress));

	if (sendResult < 0)
	{
		std::cout << "[COMMS CLIENT] sendto() failed with error: " << WSAGetLastError() << "." << std::endl;
		return false;
	}
	
	return true;
}

bool Client::getMessage(Message& message)
{
	if (m_messageQueue.empty())
		return false;

	message = m_messageQueue.front();
	m_messageQueue.pop();

	return true;
}

bool Client::receiveMessages()
{
	bool done = false;

	while (!done)
	{
		
	}
}

void Client::generateRandomData(uint8_t* buffer) const
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint16_t> distrib(0x00, 0xFF);

	// TODO: Find a more efficient way to do this
	for (int i = 0; i < 60; i++)
	{
		*(buffer + i) |= distrib(gen);
	}
}

bool Client::attemptConnection()
{
	Message connectionMessage;
	connectionMessage.type            = MSG_CONNECT;
	connectionMessage.playerIDAndTeam = m_idAndTeam;
	connectionMessage.parameters      = MSG_ALL;
	generateRandomData(connectionMessage.data);
	if (!sendMessage(&connectionMessage, true))
	{
		std::cout << "[COMMS CLIENT] Could not send connect message." << std::endl;
		return false;
	}

	return true;
}

} // cl
