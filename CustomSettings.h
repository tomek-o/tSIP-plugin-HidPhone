#ifndef CustomSettingsH
#define CustomSettingsH

#include <vector>
#include <string>

namespace Json
{
    class Value;
}

struct ReportAction
{
    std::vector<unsigned char> report;
    std::string script;
};

class CustomSettings
{
public:
    CustomSettings(void);
    void fromJson(const Json::Value &jv);
    void toJson(Json::Value &jv) const;

    unsigned int usbVid;
    unsigned int usbPid;
    int usbUsagePage;
    bool logReceivedHidReports;
    std::vector<ReportAction> reportActions;
    std::string scriptOffHookToggle;
    std::string scriptOffHook1;
    std::string scriptOffHook0;

    std::string scriptMuteToggle;
    std::string scriptMute1;
    std::string scriptMute0;

    bool controlMuteLed;
};

extern CustomSettings customSettings;

#endif // CustomSettingsH
