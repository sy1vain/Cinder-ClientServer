//
//  TCPClient.h
//  ClientServer
//
//  Created by Sylvain Vriens on 28/02/2013.
//
//

/**
 Current problems
 * the server doesn;t check the reconnects yet
 * server misses a lot of implementations
 
 
 **/

#pragma once

// defines the value of _WIN32_WINNT needed by boost asio (WINDOWS ONLY)
#ifdef WIN32
    #include <sdkddkver.h>
#endif

#include "TCP.h"

#include "cinder/Cinder.h"
#include "cinder/Utilities.h"
#include "cinder/Thread.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <queue>

namespace tcp {
    
    using namespace boost::asio;
    
    typedef std::shared_ptr<ip::tcp::socket> socket_ptr;
    typedef std::shared_ptr<ip::tcp::endpoint> endpoint_ptr;
    typedef std::shared_ptr<streambuf> streambuffer_ptr;
    
    class TCPClient {
        
        friend class TCPServer;
        
        /*****************************************/
        /** CONSTRUCTORS *************/
        /*****************************************/
    public:
        TCPClient(){
        }
        
        TCPClient(const std::string &ip, unsigned short port){
            mObj = std::shared_ptr<Obj>(new Obj(ip, port));
        }
        
    public:
        ~TCPClient(){
            mObj.reset();
        }
        
        
        /*****************************************/
        /** PUBLIC FUNCTIONS *********************/
        /*****************************************/
        
        void connect(){
            if(!mObj) return;
            mObj->connect();
        }
        
        void disconnect(){
            if(!mObj) return;
            mObj->disconnect();
        }
        
        bool isConnected(){
            if(!mObj) return false;
            return mObj->isConnected();
        }
        
        void setDelimiter(const std::string & delimiter){
            if(!mObj) return;
            mObj->setDelimiter(delimiter);
        }
        
        bool hasBuffer(){
            if(!mObj) return false;
            return mObj->hasBuffer();
        }
        
        ci::Buffer getBuffer(){
            ci::Buffer buffer;
            if(!mObj) return buffer;
            
            return mObj->getBuffer();
        }
        
        void send(const ci::Buffer & buffer){
            if(!mObj) return;
            mObj->send(buffer);
        }
        
        void send(const std::string & data){
            if(!mObj) return;
            mObj->send(data);
        }
        
        void sendRaw(const ci::Buffer & buffer){
            if(!mObj) return;
            mObj->send(buffer, true);
        }
        
        void sendRaw(const std::string & data){
            if(!mObj) return;
            mObj->send(data, true);
        }
        
        endpoint_ptr getEndpoint(){
            if(!mObj) return endpoint_ptr();
            return mObj->getEndpoint();
        }
        
        std::string getAddress(){
            if(!mObj) return "0.0.0.0";
            return mObj->getAddress();
        }
        
        unsigned short getPort(){
            if(!mObj) return 0;
            return mObj->getPort();
        }
        
        std::string getAddressWithPort(){
            return getAddress() + ":" + ci::toString(getPort());
        }
        
        
        
        /*****************************************/
        /** SIGNALS ******************************/
        /*****************************************/
        
#if defined(TCP_USE_SIGNALS)
        
        typedef cinder::signals::signal<void(endpoint_ptr)> connection_signal_type;
        typedef cinder::signals::signal<void(endpoint_ptr, const ci::Buffer&)> data_signal_type;
        
        connection_signal_type& getSignalConnect(){
            return mObj->mConnectSignal;
        }
        
        connection_signal_type& getSignalDisconnect(){
            return mObj->mDisconnectSignal;
        }
        
        data_signal_type& getSignalData(){
            return mObj->mDataSignal;
        }
        
        
#endif
        
        /*****************************************/
        /** PROTECTED  FUNCTIONS *****************/
        /*****************************************/
    protected:
        
        //return the socket and if needed initilizes
        socket_ptr getSocket(){
            if(!mObj){
                mObj = std::shared_ptr<Obj>(new Obj());
            }
            
            return mObj->getSocket();
        }
        
        void setState(tcp::TCPState state){
            if(!mObj) return;
            mObj->setState(state);
        }
        
    private:
        
        class Buffer : public cinder::Buffer {
        public:
            Buffer():cinder::Buffer(){}
            Buffer(void *aBuffer, size_t aSize): cinder::Buffer(aBuffer, aSize){}
            Buffer(size_t size) : cinder::Buffer(size){}
            //Buffer(std::shared_ptr<class DataSource> dataSource) : cinder::Buffer(dataSource){}
            
            bool raw;
        };
        
        struct Obj {
            /*****************************************/
            /** CONSTRUCTORS FUNCTIONS ***************/
            /*****************************************/
            Obj(const std::string &ip, unsigned short port){
                init(ip,port);
            }
            
            Obj(){
                init();
            }
            
            ~Obj(){
                stopThread();
            }
            
            /*****************************************/
            /** INITIALIZATION ***********************/
            /*****************************************/
            void init(){
                
                //set up a socket
                mSocket = socket_ptr(
                                     new ip::tcp::socket(
                                                         tcp::io_service()
                                                         )
                                     );
                
                //defaul delimiter
                mDelimiter = "\0";
                
                //clear the last activity
                mLastActivity = 0;

				//thread is obviously not running
				mThreadRunning = false;
                
                //initial state (thread not running yet)
                mState = TCP_INACTIVE;
                
            }
            
            
            void init(const std::string &ip, unsigned short port){
                //set the endpoint
                mEndpoint = endpoint_ptr(new ip::tcp::endpoint(ip::address::from_string(ip), port));
                
                //initialize the rest
                init();
            }
            
            /*****************************************/
            /** CONNECTION ***************************/
            /*****************************************/
            void connect(){

                TCPState state = getState();
                
                //already connected or connecting
                if(state>=TCP_CONNECTING) return;
                
                //set it to request a connect
                setState(TCP_CONNECT);
            }
            
            void disconnect(){
                setState(TCP_CLOSE);
            }
            
            bool isConnected(){
                TCPState state = getState();
                return (state>=TCP_CONNECTED && state<TCP_CLOSED);
            }
            
            socket_ptr getSocket(){
                return mSocket;
            }
            
            endpoint_ptr getEndpoint(){
                /** TBI check if set and create it otherwise from socket **/
                if(!mEndpoint){
                    try{
                        ip::tcp::endpoint endpoint = mSocket->remote_endpoint();
                        mEndpoint = endpoint_ptr(new ip::tcp::endpoint(endpoint.address(), endpoint.port()));
                    }catch(...){
                    }
                }
                
                return mEndpoint;
            }
            
            std::string getAddress(){
                return getEndpoint()->address().to_string();
            }
            
            unsigned short getPort(){
                return getEndpoint()->port();
            }
            
            /*****************************************/
            /** DATA FUNCTIONS ***********************/
            /*****************************************/
            
            void setDelimiter(const std::string & delimiter){
                std::lock_guard<std::mutex> lock(mDataMutex);
                mDelimiter = delimiter;
            }
            
            bool hasBuffer(){
                std::lock_guard<std::mutex> lock(mDataMutex);
                return !mIn.empty();
            }
            
            ci::Buffer getBuffer(){
                std::lock_guard<std::mutex> lock(mDataMutex);
                
                ci::Buffer buffer;
                if(!mIn.empty()){
                    buffer = mIn.front();
                    mIn.pop();
                }
                
                return buffer;
            }
            
            void send(const ci::Buffer & buffer, bool raw = false){
                TCPClient::Buffer tcpbuffer(buffer.getDataSize());
                memcpy(tcpbuffer.getData(), buffer.getData(), buffer.getDataSize());
                
                tcpbuffer.raw = raw;
                send(tcpbuffer);
                
            }
            
            void send(const TCPClient::Buffer & buffer){
                if(!isConnected()) return;
                
                {
                    std::lock_guard<std::mutex> lock(mDataMutex);
                    mOut.push(buffer);
                }
                
                TCPState state = getState();
                if(state<TCP_WRITE || state>TCP_WRITING){
                    //request a write
                    setState(TCP_WRITE);
                }
            }
            
            void send(const std::string & data, bool raw = false){
                //ci::Buffer buffer(data.size());
                TCPClient::Buffer buffer(data.size());
                memcpy(buffer.getData(), data.data(), data.size());
                buffer.raw = raw;
                send(buffer);
            }
            
            
            /*****************************************/
            /** STATE FUNCTIONS **********************/
            /*****************************************/
            
            void setState(tcp::TCPState state){
                
                {
                    std::lock_guard<std::mutex> lock(mStateMutex);
                    
                    //equals state
                    if(state==mState) return;
                    
                    mState = state;
                }
                                
				
                if(!isThreadRunning() && state!=TCP_INACTIVE){
					startThread();
                }
                
            }
            
            TCPState getState(){
                std::lock_guard<std::mutex> lock(mStateMutex);
                return mState;
            }
            
            /*****************************************/
            /** ACTION FUNCTIONS *********************/
            /*****************************************/
            
            void openConnection(){
                
                //ignore if already connected
                if(getState()>TCP_CONNECT) return;
                
                mSocket->close();
                
                mSocket->async_connect(*mEndpoint,
                                       boost::bind(&Obj::handle_connect, this, boost::asio::placeholders::error));
                
                setState(TCP_CONNECTING);
            }
            
            void closeConnection(){
                
                if(!isConnected()) return;
                
                TCPState state = getState();
                if(state>TCP_CLOSE) return;
                
                tcp::io_service().post(
                     boost::bind(&Obj::do_close, this)
                );
                
                setState(TCP_CLOSING);
            }
            
            void read(streambuffer_ptr &buffer){
                
                //not connected, return
                if(!isConnected()) return;
                
                TCPState state = getState();
                if(state>=TCP_CLOSE) return;
                
                std::string delimiter;
                {
                    std::lock_guard<std::mutex> lock(mDataMutex);
                    delimiter = mDelimiter;
                }
                
                // wait for a message to arrive, then call handle_read
                boost::asio::async_read_until(*mSocket, *buffer, delimiter,
                                              boost::bind(&Obj::handle_read, this, boost::asio::placeholders::error, buffer));
                
                //if the state is read, bump to reading
                if(state==TCP_READ){
                    setState(TCP_READING);
                }
            }
            
            void write(){

                TCPState state = getState();
                //return if not yet reading, but also when we are already writing is disconnecting
                if(state<TCP_READING || state>=TCP_WRITING) return;
                
                tcp::io_service().post(
                                         boost::bind(&Obj::do_write, this)
                                         );
                
                setState(TCP_WRITING);
            }
            
            /*****************************************/
            /** HANDLER FUNCTIONS ********************/
            /*****************************************/
            
            void handle_connect(const boost::system::error_code& error){
                
                if(!error){
                    time(&mLastActivity);
                    
                    setState(TCP_CONNECTED);
#if defined(TCP_USE_SIGNALS)
                    mConnectSignal(getEndpoint());
#endif
                }else{
					ci::app::console() << "error connectiong to server" << std::endl;
                    do_close();
                }
            }
            
            void do_close(){
                mSocket->close();
      
                setState(TCP_CLOSED);
                
#if defined(TCP_USE_SIGNALS)
                mDisconnectSignal(getEndpoint());
#endif
            }
            
            void handle_read(const boost::system::error_code& error, streambuffer_ptr &buffer){
                
                if(!error){
                    time(&mLastActivity);
                    
                    std::string delimiter;
                    {
                        std::lock_guard<std::mutex> lock(mDataMutex);
                        delimiter = mDelimiter;
                    }
                    
                    size_t delimitSize = delimiter.size(); //we add1 1 because a string always ends in \0
                    
                    boost::asio::const_buffer b = buffer->data();
                    size_t bufferSize = buffer_size(b);
                    const void *data = buffer_cast<const void *>(b);
                    
                    ci::Buffer buff(bufferSize-delimitSize);
                    memcpy(buff.getData(), data, bufferSize-delimitSize);
                    {
                        std::lock_guard<std::mutex> lock(mDataMutex);
                        mIn.push(buff);
                        //enforce only last 20 messages
                        while(mIn.size()>20){
                            mIn.pop();
                        }
                    }
                    
#if defined(TCP_USE_SIGNALS)
                    mDataSignal(getEndpoint(), buff);
#endif

                    //clear the buffer
                    buffer->consume(bufferSize);
                    
                    read(buffer);
                    
                //end of file received, so we are now closed
                }else if(error == error::eof){
                    closeConnection();
                }else if(error == error::operation_aborted){
                    //we closed it ourselves!
                    return;
                }else{ //any other error: close it
                    closeConnection();
                }
            }
            
            void do_write(){
                
                TCPState state = getState();
                
                //only write when in writing state
                if(state!=TCP_WRITING) return;
                
                TCPClient::Buffer outBuffer;
                {
                    std::lock_guard<std::mutex> lock(mDataMutex);
                    
                    //get the next buffer if available
                    if(!mOut.empty()){
                        outBuffer = mOut.front();
                        mOut.pop();
                    }
                }
                
                if(outBuffer){
                    //get data reading in send format
                    std::vector<const_buffer> bufs;
                    bufs.push_back(buffer((char*)outBuffer.getData(), outBuffer.getDataSize()));
                    //if not raw data, append the delimiter
                    if(!outBuffer.raw){
                        bufs.push_back(buffer(mDelimiter.c_str(), mDelimiter.size()));
                    }
                    
                    //write it
                    boost::asio::async_write(*mSocket,
                                             bufs,
                                             boost::bind(&Obj::handle_write, this, boost::asio::placeholders::error));
                }else{
                    setState(TCP_READING);
                }
                
                
            }
            
            void handle_write(const boost::system::error_code& error){
                
                if(!error){
                    //success, do it again
                    do_write();
                }else if(error == error::operation_aborted){
                    //we closed it outselves
                    return;
                }else{//error, we close it
                    closeConnection();
                }

            }
            
            
            /*****************************************/
            /** THREAD FUNCTIONS *********************/
            /*****************************************/
            void startThread(){
                stopThread();
                
                {
                    std::lock_guard<std::mutex> lock(mRunMutex);
                    mThreadRunning = true;
                }
                
                mThread = std::shared_ptr<std::thread>(new std::thread(boost::bind(&Obj::threadFunction, this)));
                
            }
            
            void stopThread(){

                if(mThread){
                    {
                        std::lock_guard<std::mutex> lock(mRunMutex);
                        mThreadRunning = false;
                    }
                    if(mThread->joinable()){
                        mThread->join();
                    }
                    
                    mThread = std::shared_ptr<std::thread>();
                }

            }
            
            bool isThreadRunning(){
                std::lock_guard<std::mutex> lock(mRunMutex);
                return mThreadRunning;
            }
            
            /*****************************************/
            /** THREAD *******************************/
            /*****************************************/
            
            void threadFunction(){
                
                //we set the lastactivity here, we start counting from thread start
                time(&mLastActivity);
                
                try{
                
                    //local variables
                    streambuffer_ptr buffer = streambuffer_ptr(new streambuf());
                    bool running = true;
                    
                    //the loop
                    while(running){
                        
                        //The main switch for actions
                        TCPState state = getState();
                        switch(state){
                                
                            //if we are closed, return it to the idle state
                            case TCP_CLOSED:
                                setState(TCP_IDLE);
                                break;
                                
                            //start a connect
                            case TCP_CONNECT:
                                openConnection();
                                break;
                                
                            //close the connection
                            case TCP_CLOSE:
                                closeConnection();
                                break;
                                
                            //start reading
                            case TCP_READ:
                                read(buffer);
                                break;
                                
                            case TCP_WRITE:
                                write();
                                break;
                                
                            default:
                                break;
                        }
                        
                        //poll the service
                        tcp::io_service().poll();
                        if(tcp::io_service().stopped()){
                            tcp::io_service().reset();
                        }
                        
                        //we check inactivity if connected
                        if(mLastActivity>0 && difftime(time(NULL), mLastActivity)>TCP_TIMEOUT){
                            closeConnection();
                        }
                        
                        //sleep and update the running variable
                        ci::sleep(1);
                        {
                            std::lock_guard<std::mutex> lock(mRunMutex);
                            running = mThreadRunning;
                        }
                    }
                    
                }catch(...){
                    ci::app::console() << "client thread had an exception" << std::endl;
                }
                
                mLastActivity = 0;
                
                setState(TCP_INACTIVE);
            }
            
            /*****************************************/
            /** SIGNALS ******************************/
            /*****************************************/
#if defined(TCP_USE_SIGNALS)
            
            connection_signal_type              mConnectSignal;
            connection_signal_type              mDisconnectSignal;
            data_signal_type                    mDataSignal;
            
#endif
            
            /*****************************************/
            /** MEMBERS ******************************/
            /*****************************************/
            
            //thread
            bool                                mThreadRunning;
            std::shared_ptr<std::thread>        mThread;
            std::mutex                          mRunMutex;
            
            //data
            std::string                         mDelimiter;
            std::mutex                          mDataMutex;
            std::queue<ci::Buffer>              mIn;
            std::queue<TCPClient::Buffer>      mOut;
            
            //asio stuff
            socket_ptr                          mSocket;
            endpoint_ptr                        mEndpoint;
            
            std::mutex                          mStateMutex;
            tcp::TCPState                       mState;
            
            time_t                              mLastActivity;
            
        };
        
    protected:
        std::shared_ptr<Obj> mObj;
        
    public:
        //@{
        //! Emulates shared_ptr-like behavior
        typedef std::shared_ptr<Obj> TCPClient::*unspecified_bool_type;
        operator unspecified_bool_type() const { return ( mObj.get() == 0 ) ? 0 : &TCPClient::mObj; }
        void reset() { mObj.reset(); }
        //@}
        
    };
    
    
};


