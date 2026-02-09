/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#pragma once
#include <chrono>
#include <filesystem>

using namespace std::literals::chrono_literals;
constexpr std::chrono::microseconds DBUS_TIMEOUT = 5s;

static constexpr const char* ipmiSelObject = "xyz.openbmc_project.Logging.IPMI";
static constexpr const char* ipmiSelPath = "/xyz/openbmc_project/Logging/IPMI";
static constexpr const char* ipmiSelAddInterface =
    "xyz.openbmc_project.Logging.IPMI";

static constexpr const char* pefService = "xyz.openbmc_project.pef.alerting";
static constexpr const char* pefObjPath = "/xyz/openbmc_project/pef/alerting";
static constexpr const char* pefIface = "xyz.openbmc_project.pef.pefTask";
static constexpr const char* pefTaskMethod = "doPefTask";
static constexpr const char* osService = "xyz.openbmc_project.OSSStatusSensor";
static constexpr const char* DiscreteIntf = "xyz.openbmc_project.Sensor.State";

#ifndef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
// SEL policy in dbus
static constexpr const char* selLogObj = "xyz.openbmc_project.Settings";
static constexpr const char* selLogPath =
    "/xyz/openbmc_project/logging/settings";
static constexpr const char* selLogIntf =
    "xyz.openbmc_project.Logging.Settings";
static constexpr int maxSELEntries = 2000;
static bool maxSELEntriesReached = false;
#endif

// ID string generated using journalctl to include in the MESSAGE_ID field for
// SEL entries.  Helps with filtering SEL entries in the journal.
static constexpr const char* selMessageId = "b370836ccf2f4850ac5bee185b77893a";
static constexpr int selPriority = 5; // notice
static constexpr uint8_t selSystemType = 0x02;
static constexpr uint16_t selBMCGenID = 0x0020;
static constexpr uint16_t selInvalidRecID =
    std::numeric_limits<uint16_t>::max();
static constexpr size_t selEvtDataMaxSize = 3;
static constexpr size_t selOemDataMaxSize = 13;
static constexpr uint8_t selEvtDataUnspecified = 0xFF;

static const std::filesystem::path selLogDir = "/var/log";
static const std::string selLogFilename = "ipmi_sel";
#ifdef SEL_LOGGER_ENABLE_SEL_DELETE
static const std::string nextRecordFilename = "next_records";
#endif

template <typename... T>
static uint16_t
    selAddSystemRecord(std::shared_ptr<sdbusplus::asio::connection> conn,
                       const std::string& message, const std::string& path,
                       const std::vector<uint8_t>& selData, const bool& assert,
                       const uint16_t& genId, T&&... metadata);
