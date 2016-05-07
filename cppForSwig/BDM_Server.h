////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _BDM_SERVER_H
#define _BDM_SERVER_H

#include <vector>
#include <map>
#include <thread>
#include <condition_variable>
#include <future>

#include "BitcoinP2p.h"
#include "include\fcgiapp.h"

#include "BlockDataViewer.h"
#include "BDM_seder.h"
#include "EncryptionUtils.h"
#include "LedgerEntry.h"

#define MAX_CONTENT_LENGTH 1024*1024*1024

///////////////////////////////////////////////////////////////////////////////
class SocketCallback : public Callback
{
public:
   SocketCallback::SocketCallback(void) : Callback()
   {}

   void emit(void);
   Arguments respond(void);
};

///////////////////////////////////////////////////////////////////////////////
class BlockDataManagerThread
{
   struct BlockDataManagerThreadImpl;
   BlockDataManagerThreadImpl *pimpl;

   BlockHeader* topBH_ = nullptr;

public:
   BlockDataManagerThread(const BlockDataManagerConfig &config);
   ~BlockDataManagerThread();

   // start the BDM thread
   void start(BDM_INIT_MODE mode);

   BlockDataManager *bdm();

   void setConfig(const BlockDataManagerConfig &config);

   // stop the BDM thread 
   void shutdownAndWait();

   // return true if the caller should wait on callback notification
   bool requestShutdown();

   BlockHeader* topBH(void) const { return topBH_; }

private:
   static void* thrun(void *);
   void run();

private:
   BlockDataManagerThread(const BlockDataManagerThread&);
};

///////////////////////////////////////////////////////////////////////////////
class BDV_Server_Object : public BlockDataViewer
{
private:
   map<string, function<Arguments(
      const vector<string>&, Arguments&)>> methodMap_;
   
   thread tID_;
   SocketCallback cb_;

   string bdvID_;
   BlockDataManagerThread* bdmT_;

   map<string, LedgerDelegate> delegateMap_;

   struct walletRegStruct
   {
      vector<BinaryData> scrAddrVec;
      string IDstr;
      bool isNew;
   };

   mutex registerWalletMutex_;
   map<string, walletRegStruct> wltRegMap_;

public:

   BlockingStack<BDV_Action_Struct> notificationStack_;

private:
   BDV_Server_Object(BDV_Server_Object&) = delete; //no copies
   
   void registerCallback();
   
   void buildMethodMap(void);
   void startThreads(void);

   bool registerWallet(
      vector<BinaryData> const& scrAddrVec, string IDstr, bool wltIsNew);

public:
   BDV_Server_Object(BlockDataManagerThread *bdmT);

   const string& getID(void) const { return bdvID_; }
   void maintenanceThread(void);
   Arguments executeCommand(const string& method, 
                              const vector<string>& ids, 
                              Arguments& args);
};

class Clients;

///////////////////////////////////////////////////////////////////////////////
class Clients
{
private:
   TransactionalMap<string, shared_ptr<BDV_Server_Object>> BDVs_;
   BlockDataManagerThread* bdmT_;

private:
   void maintenanceThread(void) const;

public:

   Clients(BlockDataManagerThread* bdmT) :
      bdmT_(bdmT)
   {
      auto mainthread = [this](void)->void
      {
         maintenanceThread();
      };

      thread thr(mainthread);
      if (thr.joinable())
         thr.detach();
   }

   const shared_ptr<BDV_Server_Object>& get(const string& id) const;
   Arguments runCommand(const string& cmd);
   Arguments registerBDV(void);
};

///////////////////////////////////////////////////////////////////////////////
class FCGI_Server
{
   /***
   Figure if it should use a socket or a named pipe.
   Force it to listen only to localhost if we use a socket 
   (both in *nix and win32 code files)
   ***/

private:
   int sockfd_ = -1;
   mutex mu_;
   int run_ = true;
   atomic<uint32_t> liveThreads_;

   Clients clients_;

public:
   FCGI_Server(BlockDataManagerThread* bdmT) :
      clients_(bdmT)
   {
      liveThreads_.store(0, memory_order_relaxed);
   }

   void init(void);
   void enterLoop(void);
   void processRequest(FCGX_Request* req);
};

#endif