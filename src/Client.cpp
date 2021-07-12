#include <Client.hpp>

#include <iostream>
#include <random>

#define DEFAULT_PORT 12347

// 127.0.0.1, but after applying htonl
#define LOCALHOST_ADDRESS 0x0100007F

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

	disconnect();

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
	Message       message;
	ReceiveStatus receiveStatus;

	// handle message as soon as we receive them
	// add them to queue if it is any other type of message
	// if the message is unknown and > 127 (not user-defined),
	// ignore it here.

	while ((receiveStatus = receive(message)) != ReceiveStatus::NoData)
	{
		if (receiveStatus == ReceiveStatus::Error)      break;         // handled outside of loop
		if (receiveStatus == ReceiveStatus::Oversized)  continue;      // skip oversized packet (maybe throw away excess data)
		if (receiveStatus == ReceiveStatus::Undersized) continue;      // skip undersized packet (maybe replace missing data by 0)
		if (receiveStatus == ReceiveStatus::ConnReset)
		{
			std::cout << "[COMMS CLIENT] Error: connection forcibly closed by server." << std::endl;
			disconnect(false);
			return false;
		}

		if (message.key != m_key && m_isConnected)
		{
			std::cout << "[COMMS CLIENT] Received message with invalid key. Ignoring." << std::endl;
			continue;
		}

		if (!(message.type >= 192)) // NOT Comms Lib specific
		{
			if (m_isConnected)
			{
				m_messageQueue.push(message);
			}
			else
			{
				std::cout << "[COMMS CLIENT] Warning: non-connected client received a message. Skipping." << std::endl;
				continue; // we can continue because we know this is not a connection message
			}
		}

		if (message.type >= 128)
		{
			handleMessage(message);
		}
	}

	if (receiveStatus == ReceiveStatus::Error)
	{
		std::cout << "[COMMS CLIENT] There was an error while receiving messages." << std::endl;
		//disconnect();
		return false;
	}

	return true;
}

ReceiveStatus Client::receive(Message& message)
{
#ifdef _WIN32

	if (m_socket == INVALID_SOCKET)
	{
		std::cout << "[COMMS CLIENT] Invalid socket. Cannot receive message." << std::endl;
		return ReceiveStatus::Error;
	}

	std::size_t receivedSize = 0;

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
		return ReceiveStatus::NoData;
	}
	else if (sizeReceived < 0)
	{
		int errorCode = WSAGetLastError();
		if (errorCode == WSAEWOULDBLOCK)
		{
			// no data to be read
			return ReceiveStatus::NoData;
		}
		else if (errorCode == WSAECONNRESET)
		{
			std::cout << "[COMMS CLIENT] Error: could not send message to server. Stopping client..." << std::endl;
			return ReceiveStatus::ConnReset;
		}
		else if (errorCode == WSAEMSGSIZE)
		{
			if ((senderAddress.sin_addr.s_addr != m_serverAddress.sin_addr.s_addr) ||
	    		(senderAddress.sin_port        != m_serverAddress.sin_port))
			{
				message.playerIDAndTeam = 0x00;
				message.key             = DEFAULT_KEY;
				message.parameters      = 0x00;
				message.type            = MSG_INVALID;
				std::cout << "[COMMS CLIENT] Warning: Client received message from some other address than server." << std::endl;
				return ReceiveStatus::Warning;
			}
			// too much data. Datagram truncated
			std::cout << "[COMMS CLIENT] Warning: oversized datagram received. Data truncated or ignored." << std::endl;
			return ReceiveStatus::Oversized;
		}
		std::cout << "[COMMS CLIENT] recvfrom() failed with error: " << errorCode << "." << std::endl;
		return ReceiveStatus::Error;
	}
	else if ((senderAddress.sin_addr.s_addr != m_serverAddress.sin_addr.s_addr) ||
			 (senderAddress.sin_port        != m_serverAddress.sin_port))
	{
		message.playerIDAndTeam = 0x00;
		message.key             = DEFAULT_KEY;
		message.parameters      = 0x00;
		message.type            = MSG_INVALID;
		std::cout << "[COMMS CLIENT] Warning: Client received message from some other address than server." << std::endl;
		return ReceiveStatus::Warning;
	}
	else if (sizeReceived > static_cast<int>(sizeof(Message)))
	{
		// this should not happen, but it's here just in case
		std::cout << "[COMMS CLIENT] Received oversized datagram." << std::endl;
		return ReceiveStatus::Oversized;
	}
	else if (sizeReceived < static_cast<int>(sizeof(Message)))
	{
		std::cout << "[COMMS CLIENT] Received undersized datagram." << std::endl;
		return ReceiveStatus::Undersized;
	}

#endif // _WIN32
	
	return ReceiveStatus::Success;
}

bool Client::handleMessage(const Message& message)
{
	if (message.type == MSG_CONNECT)
	{
		if (!m_isConnected)
		{
			if (message.playerIDAndTeam == m_idAndTeam)
			{
				m_isConnected = true;
				m_key         = message.key;
			}
			else
			{
				std::cout << "[COMMS CLIENT] Warning: non-connected client received a message. Skipping..." << std::endl;
				return false;
			}
		}
		else
		{
			// other client connected!
		}

		return true;
	}
	
	if (!m_isConnected)
	{
		std::cout << "[COMMS CLIENT] Warning: non-connected client received a message. Skipping..." << std::endl;
		return false;
	}

	return true;
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

bool Client::disconnect(bool serverAlive)
{
	m_isConnected = false;
	m_key         = DEFAULT_KEY;

	if (serverAlive)
	{
		Message disconnectMessage;
		disconnectMessage.playerIDAndTeam = m_idAndTeam;
		disconnectMessage.key             = m_key;
		disconnectMessage.type            = MSG_DISCONNECT;
		disconnectMessage.parameters      = MSG_ALL;
		if (!sendMessage(&disconnectMessage, true))
		{
			std::cout << "[COMMS CLIENT] Failed to send disconnect message." << std::endl;
			return false;
		}
	}

	return true;
}

} // cl
