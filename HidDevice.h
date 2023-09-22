/** \file
    \brief Simplified interface for HID device
    \note Module was tested with Generic HID from MCHPUSB 2.5 demo project running on PIC18F2550.
    Feature report reading/writing was not tested.

    \note Communicating with mouse/keyboard (quote from http://www.lvr.com/hidfaq.htm)

    Why do I receive "Access denied" when attempting to access my HID?

    Windows 2000 and later have exclusive read/write access to HIDs that are configured as a system keyboards or mice.
    An application can obtain a handle to a system keyboard or mouse by not requesting READ or WRITE access with CreateFile.
    Applications can then use HidD_SetFeature and HidD_GetFeature (if the device supports Feature reports).
*/

#ifndef HIDDEVICE_H_INCLUDED
#define HIDDEVICE_H_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddk/hidusage.h>
#include <ddk/hidpi.h>
#include <windef.h>
#include <string>

namespace nsHidDevice {

    class HidDevice {
    private:
        HANDLE handle;
        HANDLE readHandle;
        HANDLE writeHandle;
        HANDLE hEventObject;
        void* pOverlapped;
        GUID hidGuid;        /* GUID for HID driver */
        int VID, PID;
        std::string path;
        int usagePage;
        PHIDP_PREPARSED_DATA preparsedData;
        unsigned long reportInLength;
        unsigned long reportOutLength;
        unsigned int offHookSetTimer; ///< Jabra Evolve 65 sends back offhook when it receives offhook, generating false button event

        struct ButtonCapEntry {
            bool valid;
            unsigned char reportId;
            bool absolute;              ///< Specifies, if TRUE, that the button usage or usage range provides absolute data. Otherwise, if IsAbsolute is FALSE, the button data is the change in state from the previous value.
            ButtonCapEntry(void):
                valid(false),
                reportId(0xFF),
                absolute(true)
            {}
        } bcTelephonyHookSwitch, bcTelephonyMute, bcTelephonyRedial, bcLedOnline, bcLedOffHook, bcLedRing, bcLedMute;

        int CreateReadWriteHandles(std::string path);

        HidDevice(const HidDevice& source);
        HidDevice& operator=(const HidDevice&);
        static void UnicodeToAscii(char *buffer);

    public:
        enum E_ERROR
        {
            E_ERR_NONE = 0,
            E_ERR_INV_PARAM,
            E_ERR_NOTFOUND,
            E_ERR_IO,
            E_ERR_TIMEOUT,
            E_ERR_OTHER,
            E_ERR_LIMIT // dummy
        };
        typedef struct
        {
            int nCode;
            const char *lpName;
        } SERROR;
        static SERROR tabErrorsName[E_ERR_LIMIT];
        static std::string GetErrorDesc(int ErrorCode);

        enum E_REPORT_TYPE
        {
            E_REPORT_IN,
            E_REPORT_OUT,
            E_REPORT_FEATURE
        };

        HidDevice(void);
        ~HidDevice(void);

        void GetHidGuid(GUID *guid) const;
        HANDLE GetHandle(void) const {
            return handle;
        }

        std::string GetPath(void) const {
            return path;
        }


        /** \brief Search and open device with specified parameters
            \param VID required Vendor ID
            \param PID required Product ID
            \param vendorName required vendor name string, ignored if NULL
            \param productName required product name string, ignored if NULL
            \return 0 on success
        */
        int Open(int VID, int PID, char *vendorName, char *productName, int usagePage);

        /** \brief Dump device capabilities as text
            \return 0 on success
        */
        int DumpCapabilities(std::string &dump);

        /** \brief Write report to device
            \return 0 on success
        */
        int WriteReport(enum E_REPORT_TYPE type, int id, unsigned char *buffer, int len);

        int Write(unsigned char *buffer, int len);

        int WriteUsage(USAGE usage, bool state);

        /** \brief Read report from device
            \param timeout operation timeout [ms]
            \return 0 on success
        */
        int ReadReport(enum E_REPORT_TYPE type, int id, unsigned char *buffer, int *len, int timeout);

        int Read(unsigned char *buffer, int &len, int timeout);

        /** \brief Close connection to device
        */
        void Close(void);

        int GetVid(void) const {
            return VID;
        }

        int GetPid(void) const {
            return PID;
        }

        int GetUsagePage(void) const {
            return usagePage;
        }

        unsigned long GetReportInLength(void) const {
            return reportInLength;
        }

        int ParseReceivedReport(unsigned char* buffer, int len, bool &offHook, bool &mute, bool &redial, bool &lineBusy);

        unsigned int GetOffHookSetTimer(void) const {
            return offHookSetTimer;
        }
    };

};

#endif // HIDDEVICE_H_INCLUDED
