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

#include "wificond/qsap_client.h"

#include <dlfcn.h>

#include <vector>

#include <android-base/logging.h>

namespace android {
namespace wificond {

namespace {

constexpr char kVendorLibrary[] = "libqsap_sdk.so";

std::vector<std::string> Tokenize(const std::string& command) {
  std::vector<std::string> tokens;
  std::string current;
  for (char c : command) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\0') {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return tokens;
}

}  // namespace

QsapClient::QsapClient()
    : handle_(nullptr),
      set_softap_(nullptr),
      hostd_exec_(nullptr),
      add_or_remove_interface_(nullptr),
      control_bridge_(nullptr) {}

QsapClient::~QsapClient() {
  if (handle_ != nullptr) {
    dlclose(handle_);
  }
}

QsapClient& QsapClient::GetInstance() {
  static QsapClient instance;
  return instance;
}

bool QsapClient::EnsureLoaded() {
  if (handle_ != nullptr) {
    return true;
  }
  handle_ = dlopen(kVendorLibrary, RTLD_NOW | RTLD_LOCAL);
  if (handle_ == nullptr) {
    LOG(ERROR) << "Failed to load " << kVendorLibrary << ": " << dlerror();
    return false;
  }
  set_softap_ = reinterpret_cast<int (*)(int, char**)>(
      dlsym(handle_, "qsapsetSoftap"));
  hostd_exec_ = reinterpret_cast<int (*)(int, char**)>(
      dlsym(handle_, "qsap_hostd_exec"));
  add_or_remove_interface_ = reinterpret_cast<int (*)(const char*, int)>(
      dlsym(handle_, "qsap_add_or_remove_interface"));
  control_bridge_ = reinterpret_cast<int (*)(int, char**)>(
      dlsym(handle_, "qsap_control_bridge"));
  if (set_softap_ == nullptr || hostd_exec_ == nullptr ||
      add_or_remove_interface_ == nullptr || control_bridge_ == nullptr) {
    LOG(ERROR) << kVendorLibrary << " is missing QSAP entry points";
    dlclose(handle_);
    handle_ = nullptr;
    return false;
  }
  return true;
}

bool QsapClient::Dispatch(int argc, char** argv) {
  // The framework always sends "softap <subcommand> ...".  Be tolerant of a
  // bare "<subcommand> ..." as well.
  int base = 0;
  std::string first(argv[0]);
  if (first == "softap") {
    base = 1;
  }
  if (argc <= base) {
    LOG(ERROR) << "QSAP command has no subcommand";
    return false;
  }
  const std::string subcommand(argv[base]);
  int rc;
  if (subcommand == "create" || subcommand == "remove") {
    if (argc <= base + 1) {
      LOG(ERROR) << "QSAP " << subcommand << " requires an interface name";
      return false;
    }
    rc = add_or_remove_interface_(argv[base + 1], subcommand == "create" ? 1 : 0);
  } else if (subcommand == "setsoftap") {
    rc = set_softap_(argc, argv);
  } else if (subcommand == "hostapd") {
    rc = hostd_exec_(argc, argv);
  } else if (subcommand == "bridge") {
    rc = control_bridge_(argc, argv);
  } else {
    // "qccmd" and vendor-only subcommands are not reimplemented yet: the
    // vendor hostd_exec_cmd entry point has an undocumented signature.
    LOG(ERROR) << "Unsupported QSAP subcommand: " << subcommand;
    return false;
  }
  LOG(INFO) << "QSAP " << subcommand << " returned " << rc;
  return rc == 0;
}

bool QsapClient::RunCommand(const std::string& command) {
  if (!EnsureLoaded()) {
    return false;
  }
  std::vector<std::string> tokens = Tokenize(command);
  if (tokens.empty()) {
    return false;
  }
  std::vector<char*> argv;
  argv.reserve(tokens.size() + 1);
  for (std::string& token : tokens) {
    argv.push_back(const_cast<char*>(token.c_str()));
  }
  argv.push_back(nullptr);
  return Dispatch(static_cast<int>(tokens.size()), argv.data());
}

}  // namespace wificond
}  // namespace android
