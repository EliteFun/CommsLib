#ifndef COMMS_LIB_RECEIVE_STATUS_HPP
#define COMMS_LIB_RECEIVE_STATUS_HPP

#include <stdint.h>

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

#endif // COMMS_LIB_RECEIVE_STATUS_HPP
