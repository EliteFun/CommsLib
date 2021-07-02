#ifndef COMMSLIB_SERVER_HPP
#define COMMSLIB_SERVER_HPP

#include <Message.hpp>

#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#ifdef _WIN32
	
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif

	#ifndef NOMINMAX
	#define NOMINMAX
	#endif

	#include <WinSock2.h>

#endif // _WIN32

namespace cl
{

/**
 * @brief Server class. Handles everything related to the server.
 * 
 * The object will automatically start the server using the given
 * port and close it when it's done.
 */
class Server
{
public:
	/**
	 * @brief Construct a new Server object and start it
	 * 
	 * @param port the port on which to bind the server
	 */
	Server(uint16_t port);

	/**
	 * @brief Destroy the Server object and close the socket
	 * 
	 * This stops the server loop safely. You may want to put
	 * the server in a scope to destroy it before the program
	 * ends.
	 */
	~Server();

	/**
	 * @brief Manually stop the server
	 * 
	 * This is the function called by ~Server().
	 * 
	 * @see ~Server
	 */
	void stop();

	/**
	 * @brief Determine if the server thread is running
	 * 
	 * @return true 
	 * @return false 
	 */
	bool isRunning() const;

	/**
	 * @brief Display a list of connected clients with relevent data
	 * 
	 * Formats a table to display the RLBot id, team, key, address, port,
	 * ping (if requested), and last heartbeat (if requested)
	 */
	void printClients(bool showPing = false) const;

	/**
	 * @brief Ping all connected clients
	 * 
	 * If the delay is superior to 1 second, we remove the client
	 * from the list of connected clients.
	 * 
	 * This method only sends a ping request to all clients and
	 * changes the isPinging member of Client for all of them.
	 */
	void pingClients();

private:
	/**
	 * @brief A struct containing client information
	 * 
	 * All values are set to zero by default
	 */
	struct Client
	{
		uint8_t  idAndTeam = 0x00;       //!< RLBot id and team of the client
		uint8_t  key       = 0x00;       //!< Private key for the client
		// INADDR_ANY will stand for an invalid/uninitialized address
		uint32_t address   = INADDR_ANY; //!< Address of the client
		uint16_t port      = 0;          //!< Port of the client

		//uint16_t ping            = 0;
		//bool     isPinging       = false;
		//float    lastMessageTime = -1.0f;
	};

	/**
	 * @brief Different status returned by receive()
	 * 
	 * @see receive
	 */
	enum class ReceiveStatus : uint8_t
	{
		Success,    //!< Available data was the size of a message and was read
		Error,      //!< An error occured
		NoData,     //!< No data received
		Oversized,  //!< Too much data received
		Undersized, //!< Not enough data received
		Warning,    //!< A non-fatal error occured
		ConnReset,  //!< A packet could not be sent to a client because it was disconnected
	};

	/**
	 * @brief Initialize the server
	 * 
	 * @param port port of the server
	 * @return true server successfully initialized
	 * @return false there was an error while initializing the server
	 * 
	 * Automatically runs the server at the end.
	 * This function is launched in a separate thread in the
	 * constructor
	 */
	bool init(uint16_t port);

	/**
	 * @brief Run the main loop of the server
	 * 
	 */
	void run();

	/**
	 * @brief Send a message to a client
	 * 
	 * @param message the actual message to be sent
	 * @param recipient the receiving client
	 * @return true message was successfully sent
	 * @return false there was an error and the message was not sent
	 */
	bool send(Message* message, const Client& recipient) const;

	/**
	 * @brief Receive a message from a client
	 * 
	 * @param message Reference to an empty message to be filled
	 * @param ipAddress Reference to an address to be filled
	 * @param port Reference to a port to be filled
	 * @return ReceiveStatus The status of the data
	 */
	ReceiveStatus receive(Message& message, uint32_t& ipAddress, uint16_t& port);

	/**
	 * @brief Handle a raw message from a client
	 * 
	 * @param receivedMessage The message received from receive()
	 * @param senderAddress The address of the sender
	 * @param senderPort The port of the sender
	 * @return Message output message to store in buffer
	 * 
	 * This function parses the messages, execute an appropriate
	 * function to handle it more precisely, if required and
	 * transforms it into a message that can be handled later when
	 * going through the message queue.
	 */
	Message handleMessage(const Message& receivedMessage, const uint32_t& senderAddress, const uint16_t& senderPort);

	/**
	 * @brief Handle a connection message
	 * 
	 * @param connectionMessage The message received from receive()
	 * @param senderAddress The address of the sender
	 * @param senderPort The port of the sender
	 * @return Message The output message to send to other clients
	 * 
	 * As handling a connection message is longer, it is separated
	 * in another function.
	 * 
	 * If the connection is invalid for some reason, the output
	 * message will be invalid and no change will be made to the
	 * list of connected clients.
	 */
	Message handleConnectionMessage(const Message& connectionMessage, const uint32_t& senderAddress,
		const uint16_t& senderPort);

	/**
	 * @brief Sends an error message to a client
	 * 
	 * @param recipient Client struct the recipient's info
	 * @param errorCode Code of the error
	 * @return true Message successfully sent
	 * @return false Message not successfully sent
	 * 
	 * Note: the recipient does not have to be in the list of
	 * clients.
	 */
	bool sendErrorMessage(const Client& recipient, uint8_t errorCode) const;

	/**
	 * @brief Disconnect a specific client
	 * 
	 * @param clientIndex The index of the client that was disconnected
	 */
	void disconnectClient(const int& clientIndex);

	/**
	 * @brief Generate a unique key for a client
	 * 
	 * @param messageData A pointer to the beginning of the message's data
	 * @return uint8_t The key itself
	 * 
	 * This function uses the data in the message in order
	 * to generate a key specific to each client.
	 */
	uint8_t generateKey(uint8_t const* messageData) const;

	std::vector<Client> m_clients; //!< The list of all connected clients
	
	std::queue<Message> m_messageBuffer; //!< A buffer for all the messages received in one update

	SOCKET m_socket; //!< The server's socket handle

	std::atomic<bool> m_continueExecution; //!< Used to safely stop the server
	std::thread       m_thread;            //!< The server's thread
};

} // cl

#endif // COMMSLIB_SERVER_HPP

/**
 * @class cl::Server
 * 
 * Creates a server on a separate thread used to receive and dispatch
 * messages sent by clients properly. 
 */
