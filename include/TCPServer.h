//
//  TCPServer.h
//  ClientServer
//
//  Created by Sylvain Vriens on 01/03/2013.
//
//

#pragma once

#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "TCP.h"
#include "TCPClient.h"

#include "cinder/Cinder.h"
#include "cinder/Url.h"
#include "cinder/Utilities.h"
#include "cinder/Thread.h"

#include "cinder/App/AppBasic.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <queue>
#include <algorithm>

using namespace boost::asio;

namespace tcp {
    
    typedef std::shared_ptr<ip::tcp::socket> socket_ptr;
    typedef std::shared_ptr<ip::tcp::acceptor> acceptor_ptr;

    class TCPServer {
        
        /*****************************************/
        /** CONSTRUCTORS *************/
        /*****************************************/
        
    public:
        TCPServer(){
            
        }
        
        TCPServer(unsigned short port){
            mObj = std::shared_ptr<Obj>(new Obj(port));
        }
        
        ~TCPServer(){
            
        }
        
        
        /*****************************************/
        /** PUBLIC FUNCTIONS *********************/
        /*****************************************/
        
        bool isConnected(){
            if(!mObj) return false;
            
            return true;
        }
        
        bool hasClients(){
            if(!mObj) return false;
            
            return mObj->hasClients();
        }
        
        void setDelimiter(const std::string & delimiter){
            if(!mObj) return;
            
            mObj->setDelimiter(delimiter);
        }
        
        void sendAll(const ci::Buffer & buffer){
            if(!mObj) return;
            mObj->sendAll(buffer);
        }
        
        void sendAll(const std::string & data){
            if(!mObj) return;
            mObj->sendAll(data);
        }
       
        
    private:
        struct Obj : public boost::enable_shared_from_this< Obj > {
            /*****************************************/
            /** CONSTRUCTORS FUNCTIONS ***************/
            /*****************************************/
            
            Obj(short port)
            {
                init(port);
            }
            ~Obj(){
                mAcceptor->close();
                stopThread();
            }
            
            //this function will always return because it is always a shared object
            boost::shared_ptr<Obj> getPtr(){
                return boost::shared_ptr<Obj>(shared_from_this());
            }
            
            /*****************************************/
            /** INITIALIZATION ***********************/
            /*****************************************/
            
            void init(short port){
                mDelimiter = "\0";
                
                mAcceptor = acceptor_ptr(
                                         new ip::tcp::acceptor(
                                                               tcp::io_service(),
                                                               ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)
                                                               )
                                         );
                
                startThread();
            }
            
            /*****************************************/
            /** PUBLIC FUNCTION **********************/
            /*****************************************/
            
            bool hasClients(){
                std::lock_guard<std::mutex> lock(mDataMutex);
                return !mClients.empty();
            }
            
            void setDelimiter(const std::string & delimiter){
                std::lock_guard<std::mutex> lock(mDataMutex);
                
                mDelimiter = delimiter;
                
                for(std::vector<TCPClientRef>::iterator itr=mClients.begin(); itr!=mClients.end();){
                    (*itr)->setDelimiter(delimiter);
                }
            }
            
            void sendAll(const ci::Buffer & buffer){
                std::lock_guard<std::mutex> lock(mDataMutex);
                for(std::vector<TCPClientRef>::iterator itr=mClients.begin(); itr<mClients.end();++itr){
                    (*itr)->send(buffer);
                }
            }
            
            void sendAll(const std::string & data){
                ci::Buffer buffer(data.size());
                memcpy(buffer.getData(), data.data(), data.size());
                sendAll(buffer);
            }
            
            
            /*****************************************/
            /** ACTION FUNCTIONS *********************/
            /*****************************************/
            
            void accept(){
                {
                    std::lock_guard<std::mutex> lock(mRunMutex);
                    if(!mThreadRunning){
                        return;
                    }
                }
                
                //this creates an empty client
                tcp::TCPClientRef client(new tcp::TCPClient());
                
                //but by asking for a socket here it gets initilized
                mAcceptor->async_accept(
                                        *(client->getSocket()),
                                        boost::bind(&Obj::accept_handler, this, client, boost::asio::placeholders::error)
                                        );
            }
            
            //this function will only be called if not using signals
            //otherwise client_disconnect will be called
            void checkConnections(){
                
                std::lock_guard<std::mutex> lock(mDataMutex);
                
                mClients.erase(
                               std::remove_if(
                                              mClients.begin(),
                                              mClients.end(),
                                              [](TCPClientRef client) {
                                                  return !client->isConnected();
                                              }
                               )
                );
                
            }
            
            /*****************************************/
            /** HANDLER FUNCTIONS ********************/
            /*****************************************/
#if defined(TCP_USE_SIGNALS)

            void client_disconnect(endpoint_ptr endpoint){
                std::lock_guard<std::mutex> lock(mDataMutex);
                
                for(auto& client : mClients){
                    if(client->getEndpoint()==endpoint){
                        mDisconnectedClients.push(client);
                    }
                }
            }
            
            
            void cleanupDisconnectedClients(){
                //remove all disconnected clients
                std::lock_guard<std::mutex> lock(mDataMutex);
                
                if(mDisconnectedClients.empty()) return;
                
                while(!mDisconnectedClients.empty()){
                    mClients.erase(std::find(mClients.begin(), mClients.end(), mDisconnectedClients.front()));
                    mDisconnectedClients.pop();
                }
                
                
            }
#endif
            
            void accept_handler(const TCPClientRef & client, const boost::system::error_code& error){
                if(!error){
                    
                    {
                        std::lock_guard<std::mutex> lock(mDataMutex);
                        //now that we are locked we set the delimiter
                        client->setDelimiter(mDelimiter);
                        //add it to the list
                        mClients.push_back(client);
                    }
                    
                    //connect the disconnect listener
                    client->getSignalDisconnect().connect(std::bind(&Obj::client_disconnect, this, std::_1));
                    
                    client->setState(TCP_CONNECTED);
                
                }else{
                    std::cout << "Error: " << error << std::endl;
                }
                
                //accept another connection
                accept();
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
                    mThread->join();
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
                
                //call this once, but from the thread
                accept();
                
                bool running = true;
                
                //the loop
                while(running){
                    
                    tcp::io_service().poll();
                    
#if defined(TCP_USE_SIGNALS)
                    cleanupDisconnectedClients();
#else
                    checkConnections();
#endif
                    
                    //sleep and update the running variable
                    ci::sleep(1);
                    {
                        std::lock_guard<std::mutex> lock(mRunMutex);
                        running = mThreadRunning;
                    }
                }
                
            }
            
            
            
            /*****************************************/
            /** MEMBERS ******************************/
            /*****************************************/
            
            //thread
            bool                                mThreadRunning;
            std::shared_ptr<std::thread>        mThread;
            std::mutex                          mRunMutex;
            
            //asio stuff
            acceptor_ptr                        mAcceptor;
            
            //data
            std::mutex                          mDataMutex;
            std::vector<TCPClientRef>           mClients;
            std::string                         mDelimiter;
#if defined(TCP_USE_SIGNALS)
            std::queue<TCPClientRef>           mDisconnectedClients;
#endif
            
            
        };
    protected:
        std::shared_ptr<Obj> mObj;
        
    public:
        //@{
        //! Emulates shared_ptr-like behavior
        typedef std::shared_ptr<Obj> TCPServer::*unspecified_bool_type;
        operator unspecified_bool_type() const { return ( mObj.get() == 0 ) ? 0 : &TCPServer::mObj; }
        void reset() { mObj.reset(); }
        //@}
    
    };
};