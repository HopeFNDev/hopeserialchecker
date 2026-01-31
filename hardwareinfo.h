#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>

struct hardwareitem {
    std::wstring category;
    std::wstring name;
    std::wstring value;
    std::wstring notes;
};

class hardwareinfo {
public:
    std::vector<hardwareitem> getbiosinfo();
    std::vector<hardwareitem> getprocessorinfo();
    std::vector<hardwareitem> getdiskinfo();
    std::vector<hardwareitem> getvideocontrollerinfo();
    std::vector<hardwareitem> getnetworkadapterinfo();
    std::vector<hardwareitem> getmonitorinfo();
    std::vector<hardwareitem> getusbdevices();
    std::vector<hardwareitem> getarptable();

private:
    std::vector<BYTE> getsmbiosdata();
    
    struct smbiosstructure {
        BYTE type;
        BYTE length;
        std::vector<BYTE> data;
        std::vector<std::string> strings;
    };
    
    std::vector<smbiosstructure> parsesmbiosstructures(const std::vector<BYTE>& data);
    std::string getsmbiosstring(const smbiosstructure& structure, BYTE index);
    std::string getsmbiosstringat(const smbiosstructure& structure, int dataindex);
    std::string formatuuid(const BYTE* uuid);
    
    std::vector<hardwareitem> getdiskinfodirect(const std::wstring& devicepath, int index);
    std::wstring getdiskserial(HANDLE handle);
    std::wstring getdiskmodel(HANDLE handle);
    
    std::wstring getmonitornamefromedid(const BYTE* edid, size_t length);
    std::wstring getmonitorserialfromedid(const BYTE* edid, size_t length);
    std::wstring extractedidstring(const BYTE* edid, int start);
    
    std::wstring extractserialfrominstance(const std::wstring& instanceid, bool isusbstor);
    
    std::wstring readregistrystring(HKEY hkey, const std::wstring& subkey, const std::wstring& valuename);
    DWORD readregistrydword(HKEY hkey, const std::wstring& subkey, const std::wstring& valuename);
};
