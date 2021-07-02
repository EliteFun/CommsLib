#ifndef COMMSLIB_CLIENT_HPP
#define COMMSLIB_CLIENT_HPP

#include <mutex>
#include <queue>
#include <thread>

#ifdef _WIN32

	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif

	#ifndef NOMINMAX
	#define NOMINMAX
	#endif

	#include <WinSock2.h>

#endif // _WIN32

struct Message;

namespace cl
{

class Client
{
public:
	Client(uint16_t serverPort, unsigned int id, bool isBlueTeam);
	// clientPort = serverPort + id + 1
	~Client();

	void close();
	void update(float dt);

	bool sendMessage(Message* message, bool force = false);

	/**
	 * @brief Get all messages of interest to the user
	 * 
	 * @param message The next message in the queue
	 * @return true There are messages left in the queue
	 * @return false This is the last message
	 * 
	 * @code
	 * Message msg;
	 * while (getMessage(msg))
	 * {
	 *     // handle the message
	 * }
	 * @endcode
	 * 
	 * This method will not return a message that is used for
	 * COMMS LIB to work properly like connect, ping, etc.
	 */
	bool getMessage(Message& message);

private:

	bool receiveMessages();

	bool init(uint16_t clientPort);

	void generateRandomData(uint8_t* buffer) const;

	bool attemptConnection();


	SOCKET      m_socket;
	sockaddr_in m_serverAddress;

	uint8_t m_idAndTeam;
	uint8_t m_key;

	bool    m_isConnected;

	std::queue<Message> m_messageQueue;

	//float m_lastHeartbeatTimer;
};

} // cl

#endif // COMMSLIB_CLIENT_HPP
