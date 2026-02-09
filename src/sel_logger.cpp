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
#include <systemd/sd-journal.h>
#include <phosphor-ipmi-host/selutility.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <intel-ipmi-oem/sdrutils.hpp>
#include <pulse_event_monitor.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sel_logger.hpp>
#include <threshold_event_monitor.hpp>
#include <watchdog_event_monitor.hpp>
#ifdef SEL_LOGGER_MONITOR_THRESHOLD_ALARM_EVENTS
#include <threshold_alarm_event_monitor.hpp>
#endif
#ifdef SEL_LOGGER_MONITOR_HOST_ERROR_EVENTS
#include <host_error_event_monitor.hpp>
#endif
#ifdef SEL_LOGGER_LOG_CRASH
#include <crash_event_monitor.hpp>
#endif

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

struct DBusInternalError final : public sdbusplus::exception_t
{
    const char* name() const noexcept override
    {
        return "org.freedesktop.DBus.Error.Failed";
    }
    const char* description() const noexcept override
    {
        return "internal error";
    }
    const char* what() const noexcept override
    {
        return "org.freedesktop.DBus.Error.Failed: "
               "internal error";
    }

    int get_errno() const noexcept override
    {
        return EACCES;
    }
};

#ifndef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
static bool getSELLogFiles(std::vector<std::filesystem::path>& selLogFiles)
{
    // Loop through the directory looking for ipmi_sel log files
    for (const std::filesystem::directory_entry& dirEnt :
         std::filesystem::directory_iterator(selLogDir))
    {
        std::string filename = dirEnt.path().filename();
        if (boost::starts_with(filename, selLogFilename))
        {
            // If we find an ipmi_sel log file, save the path
            selLogFiles.emplace_back(selLogDir / filename);
        }
    }
    // As the log files rotate, they are appended with a ".#" that is higher for
    // the older logs. Since we don't expect more than 10 log files, we
    // can just sort the list to get them in order from newest to oldest
    std::sort(selLogFiles.begin(), selLogFiles.end());

    return !selLogFiles.empty();
}

static void saveClearSelTimestamp()
{
    int fd = open("/var/lib/ipmi/sel_erase_time",
                  O_WRONLY | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0)
    {
        std::cerr << "Failed to open file\n";
        return;
    }

    if (futimens(fd, NULL) < 0)
    {
        std::cerr << "Failed to update SEL cleared timestamp: "
                  << std::string(strerror(errno));
    }
    close(fd);
}

#ifdef SEL_LOGGER_ENABLE_SEL_DELETE
std::vector<uint16_t> nextRecordsCache;

static void backupCacheToFile()
{
    std::ofstream nextRecordStream(selLogDir / nextRecordFilename);
    for (auto recordIds : nextRecordsCache)
    {
        nextRecordStream << recordIds << '\n';
    }
}

static uint16_t getNewRecordId()
{
    uint16_t nextRecordId = nextRecordsCache.back();
    // Check if SEL is full
    if (nextRecordId == selInvalidRecID)
    {
        return nextRecordId;
    }
    nextRecordsCache.pop_back();
    if (nextRecordsCache.empty())
    {
        nextRecordsCache.push_back(nextRecordId + 1);
    }
    backupCacheToFile();
    return nextRecordId;
}

static void initializeRecordId()
{
    std::ifstream nextRecordStream(selLogDir / nextRecordFilename);
    if (!nextRecordStream.is_open())
    {
        std::ofstream newStream(selLogDir / nextRecordFilename);
        newStream << '1' << '\n';
        newStream.close();
        nextRecordStream.open(selLogDir / nextRecordFilename);
    }
    std::string line;
    while (std::getline(nextRecordStream, line))
    {
        nextRecordsCache.push_back(std::stoi(line));
    }
}

void clearSelLogFiles()
{
    saveClearSelTimestamp();

    // Clear the SEL by deleting the log files
    std::vector<std::filesystem::path> selLogFiles;
    if (getSELLogFiles(selLogFiles))
    {
        for (const std::filesystem::path& file : selLogFiles)
        {
            std::error_code ec;
            std::filesystem::remove(file, ec);
        }
    }
    // Reload rsyslog so it knows to start new log files
    boost::asio::io_context io;
    auto dbus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::message_t rsyslogReload = dbus->new_method_call(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "ReloadUnit");
    rsyslogReload.append("rsyslog.service", "replace");
    try
    {
        sdbusplus::message_t reloadResponse = dbus->call(rsyslogReload);
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << e.what() << "\n";
    }
    // Set next record to 1
    nextRecordsCache.clear();
    nextRecordsCache.push_back(1);
    // Update backup file as well
    std::ofstream nextRecordStream(selLogDir / nextRecordFilename);
    nextRecordStream << '1' << '\n';
}

static bool selDeleteTargetRecord(const uint16_t& targetId)
{
    bool targetEntryFound = false;
    // Check if the ipmi_sel exist and save the path
    std::vector<std::filesystem::path> selLogFiles;
    if (!getSELLogFiles(selLogFiles))
    {
        return targetEntryFound;
    }

    // Go over all the ipmi_sel files to remove the entry with the target ID
    for (const std::filesystem::path& file : selLogFiles)
    {
        std::fstream logStream(file, std::ios::in);
        std::fstream tempFile(selLogDir / "temp", std::ios::out);
        if (!logStream.is_open())
        {
            return targetEntryFound;
        }
        std::string line;
        while (std::getline(logStream, line))
        {
            // Get the recordId of the current entry
            int left = line.find(" ");
            int right = line.find(",");
            int recordLen = right - left;
            std::string recordId = line.substr(left, recordLen);
            int newRecordId = std::stoi(recordId);

            if (newRecordId != targetId)
            {
                // Copy the entry from the original ipmi_sel to the temp file
                tempFile << line << '\n';
            }
            else
            {
                // Skip copying the target entry
                targetEntryFound = true;
            }
        }
        logStream.close();
        tempFile.close();
        if (targetEntryFound)
        {
            std::fstream logStream(file, std::ios::out);
            std::fstream tempFile(selLogDir / "temp", std::ios::in);
            while (std::getline(tempFile, line))
            {
                logStream << line << '\n';
            }
            logStream.close();
            tempFile.close();
            std::error_code ec;
            if (!std::filesystem::remove(selLogDir / "temp", ec))
            {
                std::cerr << ec.message() << std::endl;
            }
            break;
        }
    }
    return targetEntryFound;
}

static uint16_t selDeleteRecord(const uint16_t& recordId)
{
    std::filesystem::file_time_type prevAddTime =
        std::filesystem::last_write_time(selLogDir / selLogFilename);
    bool targetEntryFound = selDeleteTargetRecord(recordId);

    // Check if the Record Id was found
    if (!targetEntryFound)
    {
        return selInvalidRecID;
    }
    // Add to next record cache for reuse
    nextRecordsCache.push_back(recordId);
    // Add to backup file
    std::ofstream nextRecordStream(selLogDir / nextRecordFilename,
                                   std::ios::app);
    nextRecordStream << recordId << '\n';
    // Keep Last Add Time the same
    std::filesystem::last_write_time(selLogDir / selLogFilename, prevAddTime);
    // Update Last Del Time
    saveClearSelTimestamp();
    return recordId;
}
#else
static unsigned int initializeRecordId()
{
    std::vector<std::filesystem::path> selLogFiles;
    if (!getSELLogFiles(selLogFiles))
    {
        return 0;
    }
    std::ifstream logStream(selLogFiles.front());
    if (!logStream.is_open())
    {
        return 0;
    }
    std::string line;
    std::string newestEntry;
    while (std::getline(logStream, line))
    {
        newestEntry = line;
    }

    std::vector<std::string> newestEntryFields;
    boost::split(newestEntryFields, newestEntry, boost::is_any_of(" ,"),
                 boost::token_compress_on);
    if (newestEntryFields.size() < 4)
    {
        return 0;
    }

    return std::stoul(newestEntryFields[1]);
}

static unsigned int recordId = initializeRecordId();

static unsigned int getNewRecordId()
{
    static unsigned int recordId = initializeRecordId();

    // If the log has been cleared, also clear the current ID
    std::vector<std::filesystem::path> selLogFiles;
    if (!getSELLogFiles(selLogFiles))
    {
        recordId = selInvalidRecID;
    }

    if (++recordId >= selInvalidRecID)
    {
        recordId = selInvalidRecID;
    }
    return recordId;
}

void clearSelLogFiles()
{
    saveClearSelTimestamp();

    // Clear the SEL by deleting the log files
    std::vector<std::filesystem::path> selLogFiles;
    if (getSELLogFiles(selLogFiles))
    {
        for (const std::filesystem::path& file : selLogFiles)
        {
            std::error_code ec;
            std::filesystem::remove(file, ec);
        }
    }

    recordId = 0;

    // Reload rsyslog so it knows to start new log files
    boost::asio::io_context io;
    auto dbus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::message_t rsyslogReload = dbus->new_method_call(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "ReloadUnit");
    rsyslogReload.append("rsyslog.service", "replace");
    try
    {
        sdbusplus::message_t reloadResponse = dbus->call(rsyslogReload);
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << e.what() << "\n";
    }
}
#endif

#ifndef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
static bool isLinearSELPolicy()
{
    auto bus = sdbusplus::bus::new_default();

    try
    {
        // IPMI SEL Policy Object
        auto method = bus.new_method_call(
            selLogObj, selLogPath, "org.freedesktop.DBus.Properties", "Get");
        method.append(selLogIntf, "SelPolicy");
        auto reply = bus.call(method);
        if (reply.is_method_error())
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "isLinearSELPolicy: Failed to read sel policy",
                phosphor::logging::entry("PATH=%s", selLogPath),
                phosphor::logging::entry("INTERFACE=%s", selLogIntf));
            return false;
        }

        std::variant<std::string> value;
        reply.read(value);

        if (std::get<std::string>(value) ==
            "xyz.openbmc_project.Logging.Settings.Policy.Linear")
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    catch (std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "isLinearSELPolicy: Failed to get sel policy",
            phosphor::logging::entry("EXCEPTION=%s", e.what()));
        return false;
    }
}

static uint16_t getEntryCount()
{
    uint16_t entryCount = 0;
    // Loop through the directory looking for ipmi_sel log files
    // and get entry count form each file
    for (const std::filesystem::directory_entry& dirEnt :
         std::filesystem::directory_iterator(selLogDir))
    {
        std::string filename = dirEnt.path().filename();
        if (boost::starts_with(filename, selLogFilename))
        {
            std::string line;
            std::ifstream selStream(selLogDir / filename);

            while (getline(selStream, line))
            {
                entryCount++;
            }

            // Close files
            selStream.close();
        }
    }
    return entryCount;
}
#endif
#endif

#ifdef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
static void selAddSystemRecord(
    [[maybe_unused]] std::shared_ptr<sdbusplus::asio::connection> conn,
    [[maybe_unused]] const std::string& message, const std::string& path,
    const std::vector<uint8_t>& selData, const bool& assert,
    const uint16_t& genId, const std::optional<AdditionalData>& additionalData)
#else

template <typename... T>
static uint16_t selAddSystemRecord(
    [[maybe_unused]] std::shared_ptr<sdbusplus::asio::connection> conn,
    [[maybe_unused]] const std::string& message, const std::string& path,
    const std::vector<uint8_t>& selData, const bool& assert,
    const uint16_t& genId, [[maybe_unused]] T&&... metadata)
#endif
{
    // Only 3 bytes of SEL event data are allowed in a system record
    if (selData.size() > selEvtDataMaxSize)
    {
        throw std::invalid_argument("Event data too large");
    }
    unsigned int recordId;
    uint8_t sensorType = 0xFF;
    std::string selDataStr;
    toHexStr(selData, selDataStr);

#ifdef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
    std::string severity;
    switch (static_cast<eventReading>(selData[0]))
    {
        case eventReading::lowerCritGoingLow:
        case eventReading::upperCritGoingHigh:
            severity = errorLevel;
            break;
        case eventReading::lowerNonCritGoingLow:
        case eventReading::upperNonCritGoingHigh:
            severity = warningLevel;
            break;
        case eventReading::lowerNonRecoverableGoingLow:
        case eventReading::upperNonRecoverableGoingHigh:
            severity = errorLevel;
            break;
        default:
            severity = informationalLevel;
    }
    if (!assert)
    {
        severity = informationalLevel;
    }

    auto addData = additionalData.value();
    uint8_t eventType = 0x6f;
    auto itr = addData.find("EVENT_TYPE");
    if (itr != addData.end())
    {
        eventType = static_cast<uint8_t>(std::stoi(addData["EVENT_TYPE"]));
        eventType &= 0x7F; // skipping 8th bit to retrive eventReadingTypeCode.
        if (eventType != 0x01)
        {
            severity = naLevel;
        }
    }
    else
    {
        severity = naLevel;
    }

    sdbusplus::message_t AddToLog = conn->new_method_call(
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Logging.Create", "Create");

    std::string journalMsg = message;
    if (journalMsg.empty())
    {
        std::string journalMsg(
            message + " from " + path + ": " +
            " RecordType=" + std::to_string(selSystemType) + ", GeneratorID=" +
            std::to_string(genId) + ", EventDir=" + std::to_string(assert) +
            ", EventData=" + selDataStr);
    }
    auto initialize = [&]() -> std::map<std::string, std::string> {
        std::map<std::string, std::string> response{
            {"SENSOR_PATH", path},
            {"GENERATOR_ID", std::to_string(genId)},
            {"RECORD_TYPE", std::to_string(selSystemType)},
            {"EVENT_DIR", std::to_string(assert)},
            {"SENSOR_DATA", selDataStr}};

        if (additionalData.has_value())
        {
            auto addData = additionalData.value();
            auto itr = addData.find("SENSOR_TYPE");
            if (itr != addData.end())
            {
                sensorType =
                    static_cast<uint8_t>(std::stoi(addData["SENSOR_TYPE"]));
                return addData;
            }
            else
            {
                sensorType = getSensorTypeFromPath(path);
                return response;
            }
        }
        return response;
    };

    try
    {
        AddToLog.append(journalMsg, severity, initialize());
        conn->call(AddToLog);
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << "Failed adding this event: " << e.what() << "\n";
    }
#else
    recordId = getNewRecordId();
    if (recordId != 0)
    {
        sd_journal_send(
            "MESSAGE=%s", message.c_str(), "PRIORITY=%i", selPriority,
            "MESSAGE_ID=%s", selMessageId, "IPMI_SEL_RECORD_ID=%d", recordId,
            "IPMI_SEL_RECORD_TYPE=%x", selSystemType,
            "IPMI_SEL_GENERATOR_ID=%x", genId, "IPMI_SEL_SENSOR_PATH=%s",
            path.c_str(), "IPMI_SEL_EVENT_DIR=%x", assert, "IPMI_SEL_DATA=%s",
            selDataStr.c_str(), std::forward<T>(metadata)..., NULL);
    }
#endif
    // check for OS Critical Sensor Event
    uint8_t evtype = getSensorEventTypeFromPath(path);
    if (sensorType == osCriticalStop && evtype == sensorSpecificEvent)
    {
        auto bus = sdbusplus::bus::new_default();
        ipmi::Value event = static_cast<uint16_t>(1 << selData[0]);
        auto method = bus.new_method_call(
            osService, path.c_str(), "org.freedesktop.DBus.Properties", "Set");
        method.append(DiscreteIntf, "State", event);
	try
	{
           auto reply = bus.call(method);
           if (reply.is_method_error())
	   {
	       std::cerr << "Failed to update OS Critical Stop sensor";
	   }
	}
	catch (const sdbusplus::exception_t& e)
	{
            std::cerr << "Failed to update OS Critical Stop sensor " << e.what() << "\n";
	}
    }

    doPefTask(conn, path, assert, recordId, selData, message, sensorType);
}

static uint16_t selAddOemRecord(
    [[maybe_unused]] std::shared_ptr<sdbusplus::asio::connection> conn,
    [[maybe_unused]] const std::string& message,
    const std::vector<uint8_t>& selData, const uint8_t& recordType)
{
    // A maximum of 13 bytes of SEL event data are allowed in an OEM record
    if (selData.size() > selOemDataMaxSize)
    {
        throw std::invalid_argument("Event data too large");
    }
    std::string selDataStr;
    toHexStr(selData, selDataStr);

#ifdef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
    sdbusplus::message_t AddToLog = conn->new_method_call(
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Logging.Create", "Create");

    std::string journalMsg(
        message + ": " + " RecordType=" + std::to_string(recordType) +
        ", GeneratorID=" + std::to_string(0) +
        ", EventDir=" + std::to_string(0) + ", EventData=" + selDataStr);

    AddToLog.append(
        journalMsg, "xyz.openbmc_project.Logging.Entry.Level.Informational",
        std::map<std::string, std::string>(
            {{"SENSOR_PATH", ""},
             {"GENERATOR_ID", std::to_string(0)},
             {"RECORD_TYPE", std::to_string(recordType)},
             {"EVENT_DIR", std::to_string(0)},
             {"SENSOR_DATA", selDataStr}}));
    try
    {
        conn->call(AddToLog);
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << "Failed adding this event: " << e.what() << "\n";
    }

    return 0;
#else
    unsigned int recordId = getNewRecordId();
    if (recordId < selInvalidRecID)
    {
        sd_journal_send("MESSAGE=%s", message.c_str(), "PRIORITY=%i",
                        selPriority, "MESSAGE_ID=%s", selMessageId,
                        "IPMI_SEL_RECORD_ID=%d", recordId,
                        "IPMI_SEL_RECORD_TYPE=%x", recordType,
                        "IPMI_SEL_DATA=%s", selDataStr.c_str(), NULL);
    }
    return recordId;
#endif
}

#if defined(SEL_LOGGER_SEND_TO_LOGGING_SERVICE) && defined(SEL_LOGGER_ENABLE_SEL_EXTENDED)
static uint16_t selAddOemRecordExt(
    std::shared_ptr<sdbusplus::asio::connection> conn,
    const std::string& path,
    const std::vector<uint8_t>& selData, std::vector<uint8_t>& eventData, const std::string& extendedSel)
{
    // A maximum of 13 bytes of SEL event data are allowed in an OEM record
    if (selData.size() > selOemDataMaxSize)
    {
        throw std::invalid_argument("Event data too large");
    }

    if(selData[2] != oemRecordType)
    {
	throw std::invalid_argument("Not a Oem record");
    }

    eventData[0] = extendedSelSignature;
    uint8_t eventDir =  selData[13] & 0x80; // 80h representing the event direction
    uint8_t eventType = selData[13] & 0x7F; // 7F representing EventType.
    uint16_t generatorID = static_cast<uint16_t>(selData[7]| (selData[8] << 8));
    uint8_t sensorType = selData[11];

    std::string selDataStr;
    toHexStr(selData, selDataStr);

    std::string eventDataStr;
    toHexStr(eventData, eventDataStr);

    sdbusplus::message_t AddToLog = conn->new_method_call(
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Logging.Create", "Create");

    AddToLog.append("", "xyz.openbmc_project.Logging.Entry.Level.Informational",
                    std::map<std::string, std::string>(
                        {{"SENSOR_PATH", path.c_str()},
                         {"GENERATOR_ID", std::to_string(generatorID)},
                         {"RECORD_TYPE", std::to_string(oemRecordType)},
			 {"EVENT_DIR", std::to_string(eventDir)},
			 {"EVENT_TYPE", std::to_string(eventType)},
			 {"SENSOR_TYPE", std::to_string(sensorType)},
                         {"SENSOR_DATA", selDataStr},
			 {"EVENT_DATA", eventDataStr},
			 {"EXTENDED_SEL_DATA", extendedSel.c_str() }}));
    conn->call(AddToLog);
    return 0;
}
#endif

int main(int, char*[])
{
#ifndef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
#ifdef SEL_LOGGER_ENABLE_SEL_DELETE
    initializeRecordId();
#endif
#endif
    // setup connection to dbus
    boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);

    // IPMI SEL Object
    conn->request_name(ipmiSelObject);
    auto server = sdbusplus::asio::object_server(conn);

    // Add SEL Interface
    std::shared_ptr<sdbusplus::asio::dbus_interface> ifaceAddSel =
        server.add_interface(ipmiSelPath, ipmiSelAddInterface);

    // Add a new SEL entry
#ifdef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
    ifaceAddSel->register_method(
        "IpmiSelAdd",
        [conn](const std::string& message, const std::string& path,
               const std::vector<uint8_t>& selData, const bool& assert,
               const uint16_t& genId, const AdditionalData& addData) {
            return selAddSystemRecord(conn, message, path, selData, assert,
                                      genId, addData);
        });
#else
    ifaceAddSel->register_method(
        "IpmiSelAdd",
        [conn](const std::string& message, const std::string& path,
               const std::vector<uint8_t>& selData, const bool& assert,
               const uint16_t& genId) {
            return selAddSystemRecord(conn, message, path, selData, assert,
                                      genId);
        });
#endif
    // Add a new OEM SEL entry
    ifaceAddSel->register_method(
        "IpmiSelAddOem",
        [conn](const std::string& message, const std::vector<uint8_t>& selData,
               const uint8_t& recordType) {
            return selAddOemRecord(conn, message, selData, recordType);
        });

//#ifdef SEL_LOGGER_ENABLE_SEL_EXTENDED
#if defined(SEL_LOGGER_SEND_TO_LOGGING_SERVICE) && defined(SEL_LOGGER_ENABLE_SEL_EXTENDED)    
    // Add a new extended SEL entry
    ifaceAddSel->register_method("IpmiSelAddExtended",
                                 [conn](const std::string& path,
                                        const std::vector<uint8_t>& selData,
                                        std::vector<uint8_t>& eventData,
                                        const std::string extendedSel) {
        return selAddOemRecordExt(conn, path, selData, eventData, extendedSel);
    });
#endif    

#ifndef SEL_LOGGER_SEND_TO_LOGGING_SERVICE
    // Clear SEL entries
    ifaceAddSel->register_method("Clear", []() { clearSelLogFiles(); });
#ifdef SEL_LOGGER_ENABLE_SEL_DELETE
    // Delete a SEL entry
    ifaceAddSel->register_method("IpmiSelDelete", [](const uint16_t& recordId) {
        return selDeleteRecord(recordId);
    });
#endif
#endif
    ifaceAddSel->initialize();

#ifdef SEL_LOGGER_MONITOR_THRESHOLD_EVENTS
    sdbusplus::bus::match_t thresholdAssertMonitor =
        startThresholdAssertMonitor(conn);
#endif

#ifdef REDFISH_LOG_MONITOR_PULSE_EVENTS
    sdbusplus::bus::match_t pulseEventMonitor = startPulseEventMonitor(conn);
#endif

#ifdef SEL_LOGGER_MONITOR_WATCHDOG_EVENTS
    sdbusplus::bus::match_t watchdogEventMonitor =
        startWatchdogEventMonitor(conn);
#endif

#ifdef SEL_LOGGER_MONITOR_THRESHOLD_ALARM_EVENTS
    startThresholdAlarmMonitor(conn);
#endif

#ifdef SEL_LOGGER_MONITOR_HOST_ERROR_EVENTS
    startHostErrorEventMonitor(conn);
#endif

#ifdef SEL_LOGGER_LOG_CRASH
    sdbusplus::bus::match_t crashEventMonitorH = crashErrorEventMonitor(conn);
#endif
    io.run();

    return 0;
}
