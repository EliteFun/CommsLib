#ifndef COMMSLIB_MESSAGE_HPP
#define COMMSLIB_MESSAGE_HPP

#include <cstdint>

// default values
#define DEFAULT_KEY 0xF0 //!< Default key for connection packet

// to setup TMCP version
#define MSG_TMCP_VERSION 0x80 //!< Used to setup tmcp version. Only 1.0 is available

// TMCP 1.0 message types
#define MSG_TMCP1_BALL   0x81
#define MSG_TMCP1_BOOST  0x82
#define MSG_TMCP1_DEMO   0x83
#define MSG_TMCP1_READY  0x84
#define MSG_TMCP1_DEFEND 0x85

// COMMS LIB message types
#define MSG_CONNECT     0xC0 //!< Connection request and response
#define MSG_DISCONNECT  0xC1 //!< Disconnection request and broadcast
#define MSG_PING        0xC2 //!< Test latency between client and server
#define MSG_HEARTBEAT   0xC3 //!< Make sure the client is still connected
#define MSG_STATE       0xC4 //!< State of all currently connected clients
#define MSG_SERVER_STOP 0xC5 //!< Signal all clients that server is shutting down
#define MSG_ERROR       0xC6 //!< Signal an error. The error code is in data
#define MSG_INVALID     0xFF //!< Invalid message

// COMMS LIB message param masks
#define MSG_ORANGE  0b00000001            //!< Message orange team
#define MSG_BLUE    0b00000010            //!< Message blue team
#define MSG_PRIVATE 0b00000000            //!< Message server or single client only
#define MSG_ALL     MSG_ORANGE | MSG_BLUE //!< Message all bots

// COMMS LIB error codes
#define MSG_ERR_NO_ERR      0x00 //!< No error
#define MSG_ERR_TOO_MANY    0x01 //!< Too many clients are already connected
#define MSG_ERR_ALREADY_CON 0x02 //!< Client is already connected to the server
#define MSG_ERR_INVALID_CON 0x03 //!< Invalid connection packet

// COMMS LIB disconnect sources
#define MSG_DISCONNECT_SRC_CLIENT 0x01 //!< The client sent a disconnect message
#define MSG_DISCONNECT_SRC_SERVER 0x02 //!< The client was disconnected by the server (i.e. because of an error)

/**
 * @brief Common struct of bytes for client and server
 * 
 * Contains all the information required for a message.
 * It has a fixed size of 64 bytes to avoid any padding
 * issues.
 */
struct Message
{
	uint8_t playerIDAndTeam = 0x00;        //!< ID and team of the bot sending the message
	uint8_t key             = DEFAULT_KEY; //!< Unique client key
	uint8_t parameters      = 0x00;        //!< Bitset of parameters (who to message, etc.)
	uint8_t type            = MSG_INVALID; //!< Message type
	uint8_t data[60];                      //!< Message-specific data
};

/**
 * @struct Message
 * 
 * Common struct to transmit data from clients to server and vice-
 * versa.
 * 
 * ~~~~ MESSAGE TYPES ~~~~
 * * 0-127 ------ User defined
 * * 128-191 ---- TMCP
 * * 192-255 ---- Comms lib specific
 */

#endif // COMMSLIB_MESSAGE_HPP
