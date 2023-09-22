#include "CustomSettings.h"
#include <json/json.h>

CustomSettings customSettings;


CustomSettings::CustomSettings(void):
    usbVid(0x0000),
    usbPid(0x0000),
    usbUsagePage(0xb),
    logReceivedHidReports(true)
{
#if 0
    {
        // answer/hangup button
        struct ReportAction ra;
        ra.report.push_back(0x01);
        ra.script =
            "local callState = GetCallState()\r\n"
            "if callState == 1 then\t-- INCOMING\r\n"
            "\tAnswer()\r\n"
            "else\r\n"
            "\tHangup()\r\n"
            "end";
        reportActions.push_back(ra);
    }
#endif

    {
        // answer/hangup button
        struct ReportAction ra;
        ra.report.push_back(0x02);
        ra.report.push_back(0x10);
        ra.script =
            "-- Jabra Evolve 65 reject (double tap on incoming call)?\r\n"
            "Hangup()";
        reportActions.push_back(ra);
    }

    scriptOffHook =
            "local callState = GetCallState()\r\n"
            "if callState == 1 then\t-- INCOMING\r\n"
            "\tAnswer()\r\n"
            "else\r\n"
            "\tHangup()\r\n"
            "end";
}

void CustomSettings::fromJson(const Json::Value &jv)
{
    jv.getUInt("usbVid", usbVid);
    jv.getUInt("usbPid", usbPid);
    jv.getInt("usbUsagePage", usbUsagePage);
    jv.getBool("logReceivedHidReports", logReceivedHidReports);

    {
        const Json::Value &jReportActions = jv["reportActions"];
        if (jReportActions.type() == Json::arrayValue)
        {
            reportActions.clear();
            for (unsigned int i=0; i<jReportActions.size(); i++)
            {
                const Json::Value &jr = jReportActions[i];
                struct ReportAction r;
                const Json::Value &jReport = jr["report"];
                if (jReport.type() == Json::arrayValue)
                {
                    for (unsigned int j=0; j<jReport.size(); j++)
                    {
                        r.report.push_back(jReport[j].asInt());
                    }
                }
                jr.getString("script", r.script);
                if (!r.report.empty())
                {
                    reportActions.push_back(r);
                }
            }
        }
    }

    jv.getString("scriptOffHook", scriptOffHook);
}

void CustomSettings::toJson(Json::Value &jv) const
{
    jv["usbVid"] = usbVid;
    jv["usbPid"] = usbPid;
    jv["usbUsagePage"] = usbUsagePage;
    jv["logReceivedHidReports"] = logReceivedHidReports;

    {
        Json::Value &jReportActions = jv["reportActions"];
        jReportActions = Json::Value(Json::arrayValue);
        for (unsigned int i=0; i<reportActions.size(); i++)
        {
            const struct ReportAction &r = reportActions[i];
            Json::Value &jr = jReportActions.append(Json::Value(Json::objectValue));
            jr["report"] = Json::Value(Json::arrayValue);
            for (unsigned int j=0; j<r.report.size(); j++)
            {
                jr["report"].append(r.report[j]);
            }
            jr["script"] = r.script;
        }
    }

    jv["scriptOffHook"] = scriptOffHook;
}
