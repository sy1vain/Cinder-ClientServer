//
//  TCP.h
//  ClientServer
//
//  Created by Sylvain Vriens on 04/03/2013.
//
//


#pragma once


const size_t TCP_MAX_PACKET_SIZE = 1024;

const time_t TCP_TIMEOUT = 60; //in seconds


//we check the boost version here, but we should actually check the cinder version
#include <boost/version.hpp>
#if BOOST_VERSION >= 105200
#define TCP_USE_SIGNALS
#endif

#include "cinder/app/App.h"


namespace tcp {
    
    class TCPClient;
    class TCPServer;
    
    typedef std::shared_ptr<tcp::TCPClient> TCPClientRef;
    typedef std::shared_ptr<tcp::TCPServer> TCPServerRef;
    
    
    static boost::asio::io_service& io_service(){
        return ci::app::App::get()->io_service();
    }
    
    enum TCPState {
        TCP_INACTIVE,           //  0   thread not running
        TCP_RUNNING,            //  1   thread running, only needing poll
        TCP_IDLE = TCP_RUNNING, //  1   idem
        TCP_CONNECT,            //  2   request for connect
        TCP_CONNECTING,         //  3   waiting for the connection
        TCP_CONNECTED,          //  4   is connected & start read
        TCP_READ = TCP_CONNECTED,
        TCP_READING,            //  5   is reading
        TCP_WRITE,              //  6   request write
        TCP_WRITING,            //  7   writing messages to the socket
        TCP_CLOSE,         //  8   disconnect request
        TCP_CLOSING,      //  9  we are waiting for disconnect
        TCP_CLOSED       //  10  we are disconnected
    };

};