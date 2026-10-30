/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SYSTEM_CONNECTIVITY_WIFICOND_QSAP_CLIENT_H_
#define SYSTEM_CONNECTIVITY_WIFICOND_QSAP_CLIENT_H_

#include <string>

namespace android {
namespace wificond {

// CAF/QTI ships the soft AP control protocol as a proprietary helper,
// libqsap_sdk.so.  The original vendor wificond linked against it directly and
// exposed the whole thing to the Wi-Fi framework through
// IWificond.setHostapdParam(byte[]), which simply forwards an unauthenticated
// string to a privileged netlink/hostapd helper.
//
// This class re-implements that glue in AOSP sources.  The QSAP wire protocol
// itself is not reimplemented (it talks to vendor netlink attributes and the
// vendor hostapd fork), so the entry points are resolved at runtime from the
// device's libqsap_sdk.so instead of being hard-linked into the build.
//
// The trust model here is questionable and is preserved only for device
// behaviour compatibility: any caller that can reach IWificond can ask a
// privileged process to reconfigure the soft AP behind the framework's back.
class QsapClient {
 public:
  static QsapClient& GetInstance();

  // Parses a "softap <subcommand> ..." command and dispatches it to the vendor
  // library.  Returns true when the vendor helper reported success.
  bool RunCommand(const std::string& command);

 private:
  QsapClient();
  ~QsapClient();
  QsapClient(const QsapClient&) = delete;
  QsapClient& operator=(const QsapClient&) = delete;

  bool EnsureLoaded();
  bool Dispatch(int argc, char** argv);

  void* handle_;
  int (*set_softap_)(int argc, char** argv);
  int (*hostd_exec_)(int argc, char** argv);
  int (*add_or_remove_interface_)(const char* ifname, int add);
  int (*control_bridge_)(int argc, char** argv);
  int (*hostd_exec_cmd_)(const char* cmdline, char* reply, int* reply_len);
};

}  // namespace wificond
}  // namespace android

#endif  // SYSTEM_CONNECTIVITY_WIFICOND_QSAP_CLIENT_H_
