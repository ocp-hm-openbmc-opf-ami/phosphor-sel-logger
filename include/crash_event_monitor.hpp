#pragma once
#include <sel_logger.hpp>

#include <string>
#include <string_view>
#include <variant>

inline static void
    sendCrashEventLog(std::shared_ptr<sdbusplus::asio::connection> conn,
                      const std::string unitName)
{
    std::string eventMessage = "Service " + unitName +
                               " has exited unsuccessfully";
    sdbusplus::message_t newLogEntry = conn->new_method_call(
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Logging.Create", "Create");
    const std::string logLevel =
        "xyz.openbmc_project.Logging.Entry.Level.Error";
    const std::string serviceName = "UNIT_NAME";
    newLogEntry.append(std::move(eventMessage), std::move(logLevel),
                       std::map<std::string, std::string>(
                           {{std::move(serviceName), std::move(unitName)}}));
    try
    {
       conn->call(newLogEntry);
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << "Failed adding crash event: " << e.what() << "\n";
    }
}
inline static sdbusplus::bus::match_t
    crashErrorEventMonitor(std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto crashEventMatcherCallback = [conn](sdbusplus::message_t& msg) {
        uint32_t jobID{};
        sdbusplus::message::object_path jobPath;
        std::string jobUnit{};
        std::string jobResult{};
        try
        {
           msg.read(jobID, jobPath, jobUnit, jobResult);
        }
        catch (const sdbusplus::exception_t& e)
        {
              std::cerr << "Failed to read value from " << msg.get_path() << " e= " << e.what() << "\n";
        }
        std::string test = jobPath.str;

        if (jobResult == "failed")
        {
            sendCrashEventLog(conn, jobUnit);
        }
    };

    sdbusplus::bus::match_t crashEventMatcher(
        static_cast<sdbusplus::bus_t&>(*conn),
        "type='signal',interface='org.freedesktop.systemd1.Manager',"
        "member='JobRemoved'",
        std::move(crashEventMatcherCallback));

    return crashEventMatcher;
}
