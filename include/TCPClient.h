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

#include "TCP.h"

#include "cinder/Cinder.h"
#include "cinder/Utilities.h"
#include "cinder/Thread.h"

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
        
    protected:
        void clearSignals(){
            //here we disconnect all and invoke immediatly to clean them up
            mObj->mConnectSignal.disconnect_all_slots();
            mObj->mConnectSignal(endpoint_ptr());
            mObj->mDisconnectSignal.disconnect_all_slots();
            mObj->mDisconnectSignal(endpoint_ptr());
            mObj->mDataSignal.disconnect_all_slots();
            mObj->mDataSignal(endpoint_ptr(),ci::Buffer());
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
            
            //this copies from a cinder buffer and creats a checksum buffer
            Buffer(const cinder::Buffer& source): cinder::Buffer(source.getDataSize() + 2){
                copyFrom(source.getData(), source.getDataSize());
                
                mContents = ci::Buffer(getData(), source.getDataSize());

                //genereate checksum (done on request!)
                generateChecksum();
            }
            //this copies from another sources and checks the checksum
            Buffer(const void *aBuffer, size_t aSize): cinder::Buffer(aSize){
                //copy the actual data
                copyFrom(aBuffer,aSize);
                
                //get the data
                uint8_t * data = (uint8_t*)getData();
                mContents = ci::Buffer(data, aSize-2);
                
                                //get the checksum
                uint8_t * cdata = data + mContents.getDataSize();
                mChecksum = ci::Buffer(cdata, 2);
                
                //check the checksum
                mChecked = verifyChecksum();
            }
            
            ci::Buffer getBuffer(){
                ci::Buffer b(getData(), getDataSize()-2);
                return ci::Buffer(b);
            }
            
            ci::Buffer getContents(){
                ci::Buffer c(mContents.getDataSize());
                c.copyFrom(mContents.getData(), mContents.getDataSize());
                return c;
            }
            
            const ci::Buffer& getChecksum(){
                return mChecksum;
            }
            
            bool check(){
                return mChecked;
            }
            
        protected:
            void generateChecksum(){
                
                //generate
                uint8_t * cdata = (uint8_t*)getData() + mContents.getDataSize();
                mChecksum = ci::Buffer(cdata, 2);
                
                //calculate
                uint8_t cksm = checksum();
                uint8_t ccksm = ~cksm;
                
                //set
                *cdata = cksm;  //set the checksum
                cdata++;
                *cdata = ccksm; //and set the inverse checksum
                
                mChecked = true;
            }
            
            bool verifyChecksum(){
                
                uint8_t * cdata = (uint8_t*)mChecksum.getData();
                uint8_t cksm = *cdata;
                cdata++;
                uint8_t ccksm = *cdata;
                uint8_t cccksm = ~ccksm;
                
                if(cksm!=cccksm) return false;
                
                //check the checksum
                return checksum()==cksm;
            }
            
            uint8_t checksum(){
                uint8_t * data = (uint8_t*)mContents.getData();
                uint8_t chsm = 0;
                for(size_t i=0; i<mContents.getDataSize(); i++){
                    chsm = chsm^*(data+i);
                }
                
                return chsm;
            }
            
            bool mChecked;
            //these are actually just point to the data of this one
            ci::Buffer mContents;
            ci::Buffer mChecksum;
            
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
                if(isConnected()){
                    closeConnection();
                }
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
            
            void send(const ci::Buffer & buffer){
                TCPClient::Buffer tcpbuffer(buffer);
                send(tcpbuffer);
                
            }
            
            void send(const TCPClient::Buffer & buffer){
                if(!isConnected()) return;
                
                {
                    std::lock_guard<std::mutex> lock(mDataMutex);
                    mOut.push(buffer);
                }
                
                TCPState state = getState();
                //we are not ready yet since reading hasn't started yet
                if(state<TCP_READING) return;
                if(state<TCP_WRITE || state>TCP_WRITING){
                    
                    //request a write
                    setState(TCP_WRITE);
                }
            }
            
            void send(const std::string & data){
                ci::Buffer buffer(data.size());
                memcpy(buffer.getData(), data.data(), data.size());
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
                                              boost::bind(&Obj::handle_read, this, boost::asio::placeholders::error, buffer, boost::asio::placeholders::bytes_transferred));
                
                //if the state is read, bump to reading
                if(state==TCP_READ){
                    if(mOut.empty()){
                        setState(TCP_READING);
                    }else{
                        //there was a packet waiting for writing before we started reading
                        setState(TCP_WRITE);
                    }
                }
            }
            
            void write(){

                TCPState state = getState();
                //return if not yet reading, but also when we are already writing is disconnecting
                if(state<TCP_READING || state>=TCP_WRITING) return;
                
                setState(TCP_WRITING);
                
                requestWrite();
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
                if(mSocket){
                    mSocket->close();
                }
      
                setState(TCP_CLOSED);
                
#if defined(TCP_USE_SIGNALS)
                mDisconnectSignal(getEndpoint());
#endif
            }
            
            void handle_read(const boost::system::error_code& error, streambuffer_ptr &buffer, std::size_t size){
                
                if(!error){
                    time(&mLastActivity);
                    
                    std::string delimiter;
                    {
                        std::lock_guard<std::mutex> lock(mDataMutex);
                        delimiter = mDelimiter;
                    }
                    
                    size_t delimitSize = delimiter.size();
                    
                    boost::asio::const_buffer b = buffer->data();
                    size_t bufferSize = size; //amount of data including first delimiter
                    size_t dataSize = bufferSize - delimitSize; //amount of data untill first delimiter
                    const void *data = buffer_cast<const void *>(b);
                    
                    TCPClient::Buffer tcpBuffer(data, dataSize);
                    if(tcpBuffer.check()){
                        ci::Buffer contents = tcpBuffer.getContents();
                        {
                            std::lock_guard<std::mutex> lock(mDataMutex);
                            mIn.push(contents);
                            //enforce only last 20 messages
                            while(mIn.size()>20){
                                mIn.pop();
                            }
                        }
                    
#if defined(TCP_USE_SIGNALS)
                        mDataSignal(getEndpoint(), contents);
#endif
                    }else{
                        ci::app::console() << "possible corrupted packet" << std::endl;
                    }

                    //clear the buffer by reading including our delimiter
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
            
            void requestWrite(){
                tcp::io_service().post(
                                       boost::bind(&Obj::do_write, this)
                                       );
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
                    //delimiter
                    bufs.push_back(buffer(mDelimiter.c_str(), mDelimiter.size()));
                    
                    //write it, sending the buffer with it so it gets retained
                    boost::asio::async_write(*mSocket,
                                             bufs,
                                             boost::bind(
                                                         &Obj::handle_write,
                                                         this,
                                                         outBuffer,
                                                         boost::asio::placeholders::error,
                                                         boost::asio::placeholders::bytes_transferred
                                                         ));
                }else{
                    setState(TCP_READING);
                }
                
            }
            
            //the out buffer is actuall never used, but in this way the memory gets retained untill this block
            void handle_write(TCPClient::Buffer, const boost::system::error_code& error,  std::size_t bytes_transferred){
                
                if(!error){
                    //success, do it again
                    requestWrite();
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


