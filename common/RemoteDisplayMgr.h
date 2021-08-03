/*
Copyright (C) 2021 Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions
and limitations under the License.


SPDX-License-Identifier: Apache-2.0

Author:
Xue Yifei (yifei.xue@intel.com)
Mao Marc (marc.mao@intel.com)
Date: 2021.06.09

*/

#ifndef __REMOTE_DISPLAY_MGR_H__
#define __REMOTE_DISPLAY_MGR_H__

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "IRemoteDevice.h"
#include "RemoteDisplay.h"

class RemoteDisplayMgr : public DisplayStatusListener {
 public:
  RemoteDisplayMgr();
  ~RemoteDisplayMgr();

  int init(IRemoteDevice* dev);
  // hwc as client, legacy to compatible mdc
  int connectToRemote();

  // DisplayStatusListener
  int onConnect(int fd) override;
  int onDisconnect(int fd) override;

 private:
  int addRemoteDisplay(int fd);
  int removeRemoteDisplay(int fd);
  void socketThreadProc();
  void workerThreadProc();

  int setNonblocking(int fd);
  int addEpollFd(int fd);
  int delEpollFd(int fd);

 private:
  const char* kClientSock = "/ipc/display-sock";
  const char* kServerSock = "/ipc/hwc-sock";

  std::unique_ptr<IRemoteDevice> mHwcDevice;
  int mClientFd = -1;
  std::mutex mConnectionMutex;
  std::condition_variable mClientConnected;

  std::unique_ptr<std::thread> mSocketThread;
  int mServerFd = -1;
  int mMaxConnections = 2;

  std::vector<int> mPendingRemoveDisplays;
  std::mutex mWorkerMutex;
  int mWorkerEventReadPipeFd = -1;
  int mWorkerEventWritePipeFd = -1;

  std::map<int, RemoteDisplay> mRemoteDisplays;
  static const int kMaxEvents = 10;
  int mEpollFd = -1;
};
#endif  //__REMOTE_DISPLAY_MGR_H__
