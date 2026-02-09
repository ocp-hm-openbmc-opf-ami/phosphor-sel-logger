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
#else
constexpr const char* informationalLevel =
    "xyz.openbmc_project.Logging.Entry.Level.Informational";
constexpr const char* warningLevel =
    "xyz.openbmc_project.Logging.Entry.Level.Warning";
constexpr const char* errorLevel =
    "xyz.openbmc_project.Logging.Entry.Level.Critical";
constexpr const char* naLevel =
    "xyz.openbmc_project.Logging.Entry.Level.NotApplicable";

enum class eventReading : uint8_t
{
    lowerNonCritGoingLow = 0x00,
    lowerCritGoingLow = 0x02,
    lowerNonRecoverableGoingLow = 0x04,
    upperNonCritGoingHigh = 0x07,
    upperCritGoingHigh = 0x09,
    upperNonRecoverableGoingHigh = 0x0b,
};
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

static void toHexStr(const std::vector<uint8_t>& data, std::string& hexStr)
{
    std::stringstream stream;
    stream << std::hex << std::uppercase << std::setfill('0');
    for (int v : data)
    {
        stream << std::setw(2) << v;
    }
    hexStr = stream.str();
}

static void doPefTask(
    std::shared_ptr<sdbusplus::asio::connection> conn, const std::string& path,
    bool assert, const uint16_t& recordId, const std::vector<uint8_t>& selData,
    const std::string& message, const std::optional<uint8_t> addSenType)
{
    // Assign default values if none are provided
    uint8_t senNum = 0xff;
    uint8_t evtype = 0xff;
    uint8_t sentype = 0xff;

    if (!path.empty())
    {
        senNum = getSensorNumberFromPath(path);
        evtype = getSensorEventTypeFromPath(path);
        sentype = getSensorTypeFromPath(path);
    }
    else if (addSenType.has_value())
    {
        sentype = addSenType.value();
        evtype = getEventType(sentype);
    }
    evtype |= assert ? 0x00 : 0x80;
    std::chrono::microseconds timeout = DBUS_TIMEOUT;
    auto startPefTask =
        conn->new_method_call(pefService, pefObjPath, pefIface, pefTaskMethod);
    startPefTask.append(static_cast<uint16_t>(recordId), sentype, senNum,
                        evtype, selData[0], selData[1], selData[2], selBMCGenID,
                        message.c_str());
    try
    {
        conn->call(startPefTask, timeout.count());
    }
    catch (sdbusplus::exception_t&)
    {
        std::cerr << "Failed to call doPefTask\n";
    }
}

#ifdef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
using AdditionalData = std::map<std::string, std::string>;
static void selAddSystemRecord(
    std::shared_ptr<sdbusplus::asio::connection> conn,
    const std::string& message, const std::string& path,
    const std::vector<uint8_t>& selData, const bool& assert,
    const uint16_t& genId, const std::optional<AdditionalData>& addData);
#else

template <typename... T>
static uint16_t
    selAddSystemRecord(std::shared_ptr<sdbusplus::asio::connection> conn,
                       const std::string& message, const std::string& path,
                       const std::vector<uint8_t>& selData, const bool& assert,
                       const uint16_t& genId, T&&... metadata);
#endif
