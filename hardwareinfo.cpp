#include "hardwareinfo.h"
#include <setupapi.h>
#include <iphlpapi.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "iphlpapi.lib")

std::vector<BYTE> hardwareinfo::getsmbiosdata() {
    std::vector<BYTE> result;
    
    DWORD totalsize = GetSystemFirmwareTable('RSMB', 0, nullptr, 0);
    if (totalsize < 8) return result;
    
    std::vector<BYTE> buffer(totalsize);
    DWORD written = GetSystemFirmwareTable('RSMB', 0, buffer.data(), totalsize);
    if (written < 8) return result;
    
    int tablelength = *reinterpret_cast<int*>(&buffer[4]);
    if (tablelength <= 0) return result;
    
    int available = (int)written - 8;
    int copylen = std::min(tablelength, available);
    if (copylen <= 0) return result;
    
    result.resize(copylen);
    memcpy(result.data(), &buffer[8], copylen);
    return result;
}

std::vector<hardwareinfo::smbiosstructure> hardwareinfo::parsesmbiosstructures(const std::vector<BYTE>& data) {
    std::vector<smbiosstructure> structures;
    
    size_t offset = 0;
    size_t end = data.size();
    
    while (offset + 4 < end) {
        BYTE type = data[offset];
        BYTE length = data[offset + 1];
        
        if (type == 127) break;
        if (length < 4 || offset + length > end) break;
        
        smbiosstructure structure;
        structure.type = type;
        structure.length = length;
        
        structure.data.resize(length);
        memcpy(structure.data.data(), &data[offset], length);
        
        size_t stringoffset = offset + length;
        while (stringoffset < end - 1) {
            if (data[stringoffset] == 0 && data[stringoffset + 1] == 0) {
                stringoffset += 2;
                break;
            }
            
            std::string str;
            while (stringoffset < end && data[stringoffset] != 0) {
                str += static_cast<char>(data[stringoffset]);
                stringoffset++;
            }
            
            if (!str.empty()) {
                structure.strings.push_back(str);
            }
            stringoffset++;
            
            if (stringoffset >= end) break;
        }
        
        structures.push_back(structure);
        offset = stringoffset;
        
        if (offset >= end) break;
    }
    
    return structures;
}

std::string hardwareinfo::getsmbiosstring(const smbiosstructure& structure, BYTE index) {
    if (index == 0 || index > structure.strings.size()) return "";
    return structure.strings[index - 1];
}

std::string hardwareinfo::getsmbiosstringat(const smbiosstructure& structure, int dataindex) {
    int actualoffset = 4 + dataindex;
    if (structure.data.size() <= static_cast<size_t>(actualoffset)) return "n/a";
    return getsmbiosstring(structure, structure.data[actualoffset]);
}

std::string hardwareinfo::formatuuid(const BYTE* uuid) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    oss << std::setw(2) << (int)uuid[0] << std::setw(2) << (int)uuid[1]
        << std::setw(2) << (int)uuid[2] << std::setw(2) << (int)uuid[3] << "-"
        << std::setw(2) << (int)uuid[4] << std::setw(2) << (int)uuid[5] << "-"
        << std::setw(2) << (int)uuid[6] << std::setw(2) << (int)uuid[7] << "-"
        << std::setw(2) << (int)uuid[8] << std::setw(2) << (int)uuid[9] << "-"
        << std::setw(2) << (int)uuid[10] << std::setw(2) << (int)uuid[11]
        << std::setw(2) << (int)uuid[12] << std::setw(2) << (int)uuid[13]
        << std::setw(2) << (int)uuid[14] << std::setw(2) << (int)uuid[15];
    return oss.str();
}

std::vector<hardwareitem> hardwareinfo::getbiosinfo() {
    std::vector<hardwareitem> items;
    
    auto smbiosdata = getsmbiosdata();
    if (smbiosdata.empty()) {
        items.push_back({L"bios", L"error", L"failed to retrieve smbios data", L"may require administrator privileges"});
        return items;
    }
    
    auto structures = parsesmbiosstructures(smbiosdata);
    
    for (const auto& s : structures) {
        if (s.type == 0) {
            std::string vendor = getsmbiosstringat(s, 0);
            std::string version = getsmbiosstringat(s, 1);
            std::string releasedate = getsmbiosstringat(s, 2);
            
            items.push_back({L"bios", L"vendor", std::wstring(vendor.begin(), vendor.end()), L""});
            items.push_back({L"bios", L"version", std::wstring(version.begin(), version.end()), L""});
            items.push_back({L"bios", L"releasedate", std::wstring(releasedate.begin(), releasedate.end()), L""});
        }
    }
    
    for (const auto& s : structures) {
        if (s.type == 1) {
            std::string manufacturer = getsmbiosstringat(s, 0);
            std::string productname = getsmbiosstringat(s, 1);
            std::string version = getsmbiosstringat(s, 2);
            std::string serialnumber = getsmbiosstringat(s, 3);
            
            items.push_back({L"systemproduct", L"manufacturer", std::wstring(manufacturer.begin(), manufacturer.end()), L""});
            items.push_back({L"systemproduct", L"productname", std::wstring(productname.begin(), productname.end()), L""});
            items.push_back({L"systemproduct", L"version", std::wstring(version.begin(), version.end()), L""});
            items.push_back({L"systemproduct", L"version", std::wstring(version.begin(), version.end()), L""});
            
            std::string serialcheck = serialnumber;
            std::transform(serialcheck.begin(), serialcheck.end(), serialcheck.begin(), ::tolower);
            
            bool validserial = true;
            if (serialnumber.empty() || serialcheck == "n/a" || serialcheck == "none" || 
                serialcheck.find("o.e.m.") != std::string::npos || serialcheck.find("default") != std::string::npos) {
                validserial = false;
            }
            
            if (!validserial) {
                 std::wstring wmiserial = getwmiproperty(L"Win32_ComputerSystemProduct", L"IdentifyingNumber");
                 if (!wmiserial.empty()) {
                     items.push_back({L"systemproduct", L"serialnumber", wmiserial, L""});
                 } else {
                     items.push_back({L"systemproduct", L"serialnumber", std::wstring(serialnumber.begin(), serialnumber.end()), L""});
                 }
            } else {
                items.push_back({L"systemproduct", L"serialnumber", std::wstring(serialnumber.begin(), serialnumber.end()), L""});
            }
            
            if (s.data.size() >= 24) {
                std::string uuid = formatuuid(&s.data[8]);
                items.push_back({L"systemproduct", L"uuid", std::wstring(uuid.begin(), uuid.end()), L""});
            }
        }
    }
    
    for (const auto& s : structures) {
        if (s.type == 2) {
            std::string manufacturer = getsmbiosstringat(s, 0);
            std::string product = getsmbiosstringat(s, 1);
            std::string version = getsmbiosstringat(s, 2);
            std::string serialnumber = getsmbiosstringat(s, 3);
            
            items.push_back({L"baseboard", L"manufacturer", std::wstring(manufacturer.begin(), manufacturer.end()), L""});
            items.push_back({L"baseboard", L"product", std::wstring(product.begin(), product.end()), L""});
            items.push_back({L"baseboard", L"version", std::wstring(version.begin(), version.end()), L""});
            
            std::string serialcheck = serialnumber;
            std::transform(serialcheck.begin(), serialcheck.end(), serialcheck.begin(), ::tolower);
            
            bool validserial = true;
            if (serialnumber.empty() || serialcheck == "n/a" || serialcheck == "none" || 
                serialcheck.find("o.e.m.") != std::string::npos || serialcheck.find("default") != std::string::npos) {
                validserial = false;
            }
            
            if (!validserial) {
                const std::wstring bbkey = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
                std::wstring regserial = readregistrystringraw(HKEY_LOCAL_MACHINE, bbkey, L"BaseBoardSerialNumber");
                if (regserial == L"n/a" || regserial.empty()) {
                    regserial = readregistrystringraw(HKEY_LOCAL_MACHINE, bbkey, L"BaseBoardSerial");
                }
                
                if (regserial != L"n/a" && !regserial.empty()) {
                    items.push_back({L"baseboard", L"serialnumber", regserial, L""});
                } else {
                     std::wstring wmiserial = getwmiproperty(L"Win32_BaseBoard", L"SerialNumber");
                     if (!wmiserial.empty()) {
                         items.push_back({L"baseboard", L"serialnumber", wmiserial, L""});
                     } else {
                         items.push_back({L"baseboard", L"serialnumber", std::wstring(serialnumber.begin(), serialnumber.end()), L""});
                     }
                }
            } else {
                items.push_back({L"baseboard", L"serialnumber", std::wstring(serialnumber.begin(), serialnumber.end()), L""});
            }
        }
    }
    
    for (const auto& s : structures) {
        if (s.type == 3) {
            std::string manufacturer = getsmbiosstringat(s, 0);
            std::string version = getsmbiosstringat(s, 2);
            std::string serialnumber = getsmbiosstringat(s, 3);
            std::string assettag = getsmbiosstringat(s, 4);
            
            items.push_back({L"chassis", L"manufacturer", std::wstring(manufacturer.begin(), manufacturer.end()), L""});
            items.push_back({L"chassis", L"version", std::wstring(version.begin(), version.end()), L""});
            items.push_back({L"chassis", L"serialnumber", std::wstring(serialnumber.begin(), serialnumber.end()), L""});
            items.push_back({L"chassis", L"assettag", std::wstring(assettag.begin(), assettag.end()), L""});
            
            if (s.data.size() > 1) {
                items.push_back({L"chassis", L"type", std::to_wstring(s.data[1]), L""});
            }
        }
    }
    
    bool hasbaseboard = false;
    bool hassystemproduct = false;
    for (const auto& item : items) {
        if (item.category == L"baseboard") hasbaseboard = true;
        if (item.category == L"systemproduct") hassystemproduct = true;
    }
    
    if (!hasbaseboard) {
        const std::wstring bbkey = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
        std::wstring manufacturer = readregistrystringraw(HKEY_LOCAL_MACHINE, bbkey, L"BaseBoardManufacturer");
        std::wstring product = readregistrystringraw(HKEY_LOCAL_MACHINE, bbkey, L"BaseBoardProduct");
        std::wstring version = readregistrystringraw(HKEY_LOCAL_MACHINE, bbkey, L"BaseBoardVersion");
        std::wstring serial = readregistrystringraw(HKEY_LOCAL_MACHINE, bbkey, L"BaseBoardSerialNumber");
        
        if (serial == L"n/a" || serial.empty()) {
            serial = readregistrystringraw(HKEY_LOCAL_MACHINE, bbkey, L"BaseBoardSerial");
        }
        
        if (serial == L"n/a" || serial.empty()) {
             serial = getwmiproperty(L"Win32_BaseBoard", L"SerialNumber");
        }
        
        items.push_back({L"baseboard", L"manufacturer", manufacturer, L""});
        items.push_back({L"baseboard", L"product", product, L""});
        items.push_back({L"baseboard", L"version", version, L""});
        items.push_back({L"baseboard", L"serialnumber", serial, L""});
    }
    
    if (!hassystemproduct) {
        const std::wstring syskey = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
        std::wstring manufacturer = readregistrystringraw(HKEY_LOCAL_MACHINE, syskey, L"SystemManufacturer");
        std::wstring productname = readregistrystringraw(HKEY_LOCAL_MACHINE, syskey, L"SystemProductName");
        std::wstring version = readregistrystringraw(HKEY_LOCAL_MACHINE, syskey, L"SystemVersion");
        std::wstring serial = readregistrystringraw(HKEY_LOCAL_MACHINE, syskey, L"SystemSerialNumber");
        
        if (serial == L"n/a" || serial.empty()) {
             serial = getwmiproperty(L"Win32_ComputerSystemProduct", L"IdentifyingNumber");
        }
        
        items.push_back({L"systemproduct", L"manufacturer", manufacturer, L""});
        items.push_back({L"systemproduct", L"productname", productname, L""});
        items.push_back({L"systemproduct", L"version", version, L""});
        items.push_back({L"systemproduct", L"serialnumber", serial, L""});
    }
    
    return items;
}

std::wstring hardwareinfo::readregistrystring(HKEY hkey, const std::wstring& subkey, const std::wstring& valuename) {
    HKEY key;
    if (RegOpenKeyExW(hkey, subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return L"n/a";
    }
    
    DWORD type = 0;
    DWORD size = 0;
    RegQueryValueExW(key, valuename.c_str(), nullptr, &type, nullptr, &size);
    
    if (size == 0 || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(key);
        return L"n/a";
    }
    
    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1);
    if (RegQueryValueExW(key, valuename.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return L"n/a";
    }
    
    RegCloseKey(key);
    
    std::wstring result(buffer.data());
    size_t start = result.find_first_not_of(L" \t\r\n");
    size_t end = result.find_last_not_of(L" \t\r\n");
    if (start != std::wstring::npos && end != std::wstring::npos) {
        return result.substr(start, end - start + 1);
    }
    return result;
}

std::wstring hardwareinfo::readregistrystringraw(HKEY hkey, const std::wstring& subkey, const std::wstring& valuename) {
    HKEY key;
    if (RegOpenKeyExW(hkey, subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return L"n/a";
    }
    
    DWORD type = 0;
    DWORD size = 0;
    RegQueryValueExW(key, valuename.c_str(), nullptr, &type, nullptr, &size);
    
    if (size == 0 || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(key);
        return L"n/a";
    }
    
    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1);
    if (RegQueryValueExW(key, valuename.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return L"n/a";
    }
    
    RegCloseKey(key);
    
    std::wstring result(buffer.data());
    size_t start = result.find_first_not_of(L" \t\r\n");
    size_t end = result.find_last_not_of(L" \t\r\n");
    if (start != std::wstring::npos && end != std::wstring::npos) {
        return result.substr(start, end - start + 1);
    }
    return result;
}

DWORD hardwareinfo::readregistrydword(HKEY hkey, const std::wstring& subkey, const std::wstring& valuename) {
    HKEY key;
    if (RegOpenKeyExW(hkey, subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return 0;
    }
    
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    DWORD value = 0;
    if (RegQueryValueExW(key, valuename.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) != ERROR_SUCCESS || type != REG_DWORD) {
        RegCloseKey(key);
        return 0;
    }
    
    RegCloseKey(key);
    return value;
}

std::vector<hardwareitem> hardwareinfo::getprocessorinfo() {
    std::vector<hardwareitem> items;
    
    const std::wstring cpukey = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    
    items.push_back({L"cpu", L"processor", readregistrystring(HKEY_LOCAL_MACHINE, cpukey, L"ProcessorNameString"), L""});
    items.push_back({L"cpu", L"vendor", readregistrystring(HKEY_LOCAL_MACHINE, cpukey, L"VendorIdentifier"), L""});
    items.push_back({L"cpu", L"identifier", readregistrystring(HKEY_LOCAL_MACHINE, cpukey, L"Identifier"), L""});
    
    DWORD mhz = readregistrydword(HKEY_LOCAL_MACHINE, cpukey, L"~MHz");
    if (mhz > 0) {
        items.push_back({L"cpu", L"mhz", std::to_wstring(mhz), L""});
    }
    
    return items;
}

#define IOCTL_STORAGE_QUERY_PROPERTY 0x002D1400

typedef enum _STORAGE_PROPERTY_ID {
    StorageDeviceProperty = 0
} STORAGE_PROPERTY_ID;

typedef enum _STORAGE_QUERY_TYPE {
    PropertyStandardQuery = 0
} STORAGE_QUERY_TYPE;

typedef struct _STORAGE_PROPERTY_QUERY {
    STORAGE_PROPERTY_ID PropertyId;
    STORAGE_QUERY_TYPE QueryType;
    BYTE AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY;

typedef struct _STORAGE_DEVICE_DESCRIPTOR {
    DWORD Version;
    DWORD Size;
    BYTE DeviceType;
    BYTE DeviceTypeModifier;
    BOOLEAN RemovableMedia;
    BOOLEAN CommandQueueing;
    DWORD VendorIdOffset;
    DWORD ProductIdOffset;
    DWORD ProductRevisionOffset;
    DWORD SerialNumberOffset;
    DWORD BusType;
    DWORD RawPropertiesLength;
    BYTE RawDeviceProperties[1];
} STORAGE_DEVICE_DESCRIPTOR;

std::wstring hardwareinfo::getdiskserial(HANDLE handle) {
    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    
    std::vector<BYTE> buffer(4096);
    DWORD bytesreturned = 0;
    
    if (!DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query), buffer.data(), (DWORD)buffer.size(),
        &bytesreturned, nullptr) || bytesreturned == 0) {
        return L"";
    }
    
    auto descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
    if (descriptor->SerialNumberOffset > 0 && descriptor->SerialNumberOffset < bytesreturned) {
        const char* serial = reinterpret_cast<const char*>(buffer.data() + descriptor->SerialNumberOffset);
        std::string s(serial);
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        size_t start = s.find_first_not_of(" \t");
        size_t end = s.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            s = s.substr(start, end - start + 1);
        }
        return std::wstring(s.begin(), s.end());
    }
    
    return L"";
}

std::wstring hardwareinfo::getdiskmodel(HANDLE handle) {
    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    
    std::vector<BYTE> buffer(4096);
    DWORD bytesreturned = 0;
    
    if (!DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query), buffer.data(), (DWORD)buffer.size(),
        &bytesreturned, nullptr) || bytesreturned == 0) {
        return L"";
    }
    
    auto descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
    if (descriptor->ProductIdOffset > 0 && descriptor->ProductIdOffset < bytesreturned) {
        const char* model = reinterpret_cast<const char*>(buffer.data() + descriptor->ProductIdOffset);
        std::string s(model);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        size_t start = s.find_first_not_of(" \t");
        size_t end = s.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            s = s.substr(start, end - start + 1);
        }
        return std::wstring(s.begin(), s.end());
    }
    
    return L"";
}

std::vector<hardwareitem> hardwareinfo::getdiskinfodirect(const std::wstring& devicepath, int index) {
    std::vector<hardwareitem> items;
    
    HANDLE handle = CreateFileW(devicepath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    
    if (handle == INVALID_HANDLE_VALUE) {
        return items;
    }
    
    std::wstring serial = getdiskserial(handle);
    if (!serial.empty()) {
        items.push_back({L"disk", L"serial_" + std::to_wstring(index), serial, L"physical drive " + std::to_wstring(index)});
    }
    
    std::wstring model = getdiskmodel(handle);
    if (!model.empty()) {
        items.push_back({L"disk", L"model_" + std::to_wstring(index), model, L"physical drive " + std::to_wstring(index)});
    }
    
    CloseHandle(handle);
    return items;
}

std::vector<hardwareitem> hardwareinfo::getdiskinfo() {
    std::vector<hardwareitem> items;
    
    for (int i = 0; i < 32; i++) {
        std::wstring devicepath = L"\\\\.\\PhysicalDrive" + std::to_wstring(i);
        auto diskinfo = getdiskinfodirect(devicepath, i);
        items.insert(items.end(), diskinfo.begin(), diskinfo.end());
    }
    
    if (items.empty()) {
        items.push_back({L"disk", L"info", L"no physical drives found", L"may require administrator privileges"});
    }
    
    return items;
}

std::vector<hardwareitem> hardwareinfo::getvideocontrollerinfo() {
    std::vector<hardwareitem> items;
    
    const std::wstring basekey = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}";
    
    HKEY key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, basekey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        items.push_back({L"gpu", L"error", L"could not open registry key", L""});
        return items;
    }
    
    int index = 0;
    wchar_t subkeyname[256];
    DWORD subkeynamesize = 256;
    
    for (DWORD i = 0; RegEnumKeyExW(key, i, subkeyname, &subkeynamesize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; i++) {
        subkeynamesize = 256;
        
        if (subkeyname[0] != L'0') continue;
        
        std::wstring fullpath = basekey + L"\\" + subkeyname;
        std::wstring driverdesc = readregistrystring(HKEY_LOCAL_MACHINE, fullpath, L"DriverDesc");
        
        if (driverdesc == L"n/a" || driverdesc.empty()) continue;
        
        items.push_back({L"gpu", L"name_" + std::to_wstring(index), driverdesc, L""});
        items.push_back({L"gpu", L"driverversion_" + std::to_wstring(index), readregistrystring(HKEY_LOCAL_MACHINE, fullpath, L"DriverVersion"), L""});
        items.push_back({L"gpu", L"driverdate_" + std::to_wstring(index), readregistrystring(HKEY_LOCAL_MACHINE, fullpath, L"DriverDate"), L""});
        
        index++;
    }
    
    RegCloseKey(key);
    
    if (items.empty()) {
        items.push_back({L"gpu", L"info", L"no display adapters found", L""});
    }
    
    return items;
}

std::vector<hardwareitem> hardwareinfo::getnetworkadapterinfo() {
    std::vector<hardwareitem> items;
    
    const std::wstring nickey = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}";
    
    HKEY key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, nickey.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS) {
        wchar_t subkeyname[256];
        DWORD subkeynamesize = 256;
        int regindex = 0;
        
        for (DWORD i = 0; RegEnumKeyExW(key, i, subkeyname, &subkeynamesize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; i++) {
            subkeynamesize = 256;
            
            if (subkeyname[0] != L'0') continue;
            
            std::wstring fullpath = nickey + L"\\" + subkeyname;
            std::wstring mac = readregistrystring(HKEY_LOCAL_MACHINE, fullpath, L"NetworkAddress");
            
            if (mac == L"n/a" || mac.empty()) continue;
            
            std::wstring adaptername = readregistrystring(HKEY_LOCAL_MACHINE, fullpath, L"DriverDesc");
            
            if (mac.length() == 12) {
                std::wstring formattedmac;
                for (size_t j = 0; j < 12; j += 2) {
                    if (j > 0) formattedmac += L":";
                    formattedmac += mac.substr(j, 2);
                }
                mac = formattedmac;
            }
            std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);
            
            items.push_back({L"nic", L"registrymac_" + std::to_wstring(regindex), mac, L"adapter: " + adaptername});
            regindex++;
        }
        
        RegCloseKey(key);
    }
    
    ULONG buffersize = 0;
    GetAdaptersInfo(nullptr, &buffersize);
    
    if (buffersize > 0) {
        std::vector<BYTE> buffer(buffersize);
        PIP_ADAPTER_INFO adapterinfo = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data());
        
        if (GetAdaptersInfo(adapterinfo, &buffersize) == ERROR_SUCCESS) {
            int kernelindex = 0;
            PIP_ADAPTER_INFO current = adapterinfo;
            
            while (current) {
                std::wostringstream macstream;
                for (UINT j = 0; j < current->AddressLength; j++) {
                    if (j > 0) macstream << L":";
                    macstream << std::hex << std::setfill(L'0') << std::setw(2) << (int)current->Address[j];
                }
                
                std::string desc(current->Description);
                std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);
                std::wstring wdesc(desc.begin(), desc.end());
                
                items.push_back({L"nic", L"kernelmac_" + std::to_wstring(kernelindex), macstream.str(), L"adapter: " + wdesc});
                kernelindex++;
                
                current = current->Next;
            }
        }
    }
    
    return items;
}

std::wstring hardwareinfo::extractedidstring(const BYTE* edid, int start) {
    std::wstring result;
    for (int i = 0; i < 13; i++) {
        BYTE b = edid[start + i];
        if (b == 0x0A) break;
        if (b >= 32 && b <= 126) result += static_cast<wchar_t>(b);
    }
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    size_t startpos = result.find_first_not_of(L" \t");
    size_t endpos = result.find_last_not_of(L" \t");
    if (startpos != std::wstring::npos && endpos != std::wstring::npos) {
        return result.substr(startpos, endpos - startpos + 1);
    }
    return result;
}

std::wstring hardwareinfo::getmonitornamefromedid(const BYTE* edid, size_t length) {
    if (length < 128) return L"";
    
    int offsets[] = {54, 72, 90, 108};
    for (int offset : offsets) {
        if (edid[offset] == 0x00 && edid[offset + 1] == 0x00 && 
            edid[offset + 2] == 0x00 && edid[offset + 3] == 0xFC) {
            return extractedidstring(edid, offset + 5);
        }
    }
    return L"";
}

std::wstring hardwareinfo::getmonitorserialfromedid(const BYTE* edid, size_t length) {
    if (length < 128) return L"";
    
    int offsets[] = {54, 72, 90, 108};
    for (int offset : offsets) {
        if (edid[offset] == 0x00 && edid[offset + 1] == 0x00 && 
            edid[offset + 2] == 0x00 && edid[offset + 3] == 0xFF) {
            return extractedidstring(edid, offset + 5);
        }
    }
    return L"";
}

std::vector<hardwareitem> hardwareinfo::getmonitorinfo() {
    std::vector<hardwareitem> items;
    
    HKEY displaykey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY", 0, KEY_READ, &displaykey) != ERROR_SUCCESS) {
        items.push_back({L"monitor", L"error", L"could not open display registry", L""});
        return items;
    }
    
    wchar_t monitorid[256];
    DWORD monitoridsize = 256;
    
    for (DWORD i = 0; RegEnumKeyExW(displaykey, i, monitorid, &monitoridsize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; i++) {
        monitoridsize = 256;
        
        HKEY monitorkey;
        if (RegOpenKeyExW(displaykey, monitorid, 0, KEY_READ, &monitorkey) != ERROR_SUCCESS) continue;
        
        wchar_t instanceid[256];
        DWORD instanceidsize = 256;
        
        for (DWORD j = 0; RegEnumKeyExW(monitorkey, j, instanceid, &instanceidsize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; j++) {
            instanceidsize = 256;
            
            HKEY instancekey;
            std::wstring instancepath = std::wstring(monitorid) + L"\\" + instanceid;
            if (RegOpenKeyExW(monitorkey, instanceid, 0, KEY_READ, &instancekey) != ERROR_SUCCESS) continue;
            
            HKEY paramskey;
            if (RegOpenKeyExW(instancekey, L"Device Parameters", 0, KEY_READ, &paramskey) == ERROR_SUCCESS) {
                DWORD edidsize = 0;
                DWORD type = 0;
                RegQueryValueExW(paramskey, L"EDID", nullptr, &type, nullptr, &edidsize);
                
                if (edidsize >= 128 && type == REG_BINARY) {
                    std::vector<BYTE> edid(edidsize);
                    if (RegQueryValueExW(paramskey, L"EDID", nullptr, &type, edid.data(), &edidsize) == ERROR_SUCCESS) {
                        std::wstring name = getmonitornamefromedid(edid.data(), edidsize);
                        if (name.empty()) name = monitorid;
                        
                        std::wstring serial = getmonitorserialfromedid(edid.data(), edidsize);
                        if (!serial.empty()) {
                            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                            std::transform(serial.begin(), serial.end(), serial.begin(), ::toupper);
                            std::wstring inst(instanceid);
                            std::transform(inst.begin(), inst.end(), inst.begin(), ::tolower);
                            items.push_back({L"monitor", name, serial, L"instance: " + inst});
                        }
                    }
                }
                
                RegCloseKey(paramskey);
            }
            
            RegCloseKey(instancekey);
        }
        
        RegCloseKey(monitorkey);
    }
    
    RegCloseKey(displaykey);
    
    if (items.empty()) {
        items.push_back({L"monitor", L"info", L"no monitors found with edid serials", L""});
    }
    
    return items;
}

std::wstring hardwareinfo::extractserialfrominstance(const std::wstring& instanceid, bool isusbstor) {
    if (instanceid.empty()) return L"";
    
    size_t idx = instanceid.rfind(L'\\');
    std::wstring tail = (idx != std::wstring::npos) ? instanceid.substr(idx + 1) : instanceid;
    
    if (isusbstor) {
        std::wstring serial = tail;
        size_t amp = serial.find(L'&');
        if (amp != std::wstring::npos && amp > 0) {
            serial = serial.substr(0, amp);
        }
        
        size_t start = serial.find_first_not_of(L" \t");
        size_t end = serial.find_last_not_of(L" \t");
        if (start != std::wstring::npos && end != std::wstring::npos) {
            serial = serial.substr(start, end - start + 1);
        }
        
        if (serial.empty()) return L"";
        
        bool allzeros = true;
        for (wchar_t c : serial) {
            if (c != L'0') { allzeros = false; break; }
        }
        if (allzeros) return L"";
        
        return serial;
    } else {
        if (tail.find(L'&') != std::wstring::npos) return L"";
        
        size_t start = tail.find_first_not_of(L" \t");
        size_t end = tail.find_last_not_of(L" \t");
        if (start != std::wstring::npos && end != std::wstring::npos) {
            return tail.substr(start, end - start + 1);
        }
        return tail;
    }
}

std::vector<hardwareitem> hardwareinfo::getusbdevices() {
    std::vector<hardwareitem> items;
    
    HDEVINFO deviceinfoset = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (deviceinfoset == INVALID_HANDLE_VALUE) {
        items.push_back({L"usb", L"error", L"setupdigetclassdevs failed", L""});
        return items;
    }
    
    SP_DEVINFO_DATA deviceinfodata;
    deviceinfodata.cbSize = sizeof(SP_DEVINFO_DATA);
    
    std::map<std::wstring, bool> seen;
    
    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceinfoset, i, &deviceinfodata); i++) {
        wchar_t enumerator[256] = {};
        if (!SetupDiGetDeviceRegistryPropertyW(deviceinfoset, &deviceinfodata, SPDRP_ENUMERATOR_NAME,
            nullptr, reinterpret_cast<PBYTE>(enumerator), sizeof(enumerator), nullptr)) {
            continue;
        }
        
        std::wstring enumname(enumerator);
        if (_wcsicmp(enumname.c_str(), L"USB") != 0 && _wcsicmp(enumname.c_str(), L"USBSTOR") != 0) {
            continue;
        }
        
        wchar_t devicename[256] = {};
        if (!SetupDiGetDeviceRegistryPropertyW(deviceinfoset, &deviceinfodata, SPDRP_FRIENDLYNAME,
            nullptr, reinterpret_cast<PBYTE>(devicename), sizeof(devicename), nullptr)) {
            SetupDiGetDeviceRegistryPropertyW(deviceinfoset, &deviceinfodata, SPDRP_DEVICEDESC,
                nullptr, reinterpret_cast<PBYTE>(devicename), sizeof(devicename), nullptr);
        }
        
        if (wcslen(devicename) == 0) wcscpy_s(devicename, L"USB Device");
        std::wstring wdevicename(devicename);
        std::transform(wdevicename.begin(), wdevicename.end(), wdevicename.begin(), ::tolower);
        
        wchar_t instanceidbuf[512] = {};
        if (!SetupDiGetDeviceInstanceIdW(deviceinfoset, &deviceinfodata, instanceidbuf, 512, nullptr)) {
            continue;
        }
        
        std::wstring instanceid(instanceidbuf);
        if (seen.find(instanceid) != seen.end()) continue;
        seen[instanceid] = true;
        
        bool isusbstor = (_wcsicmp(enumname.c_str(), L"USBSTOR") == 0);
        std::wstring serial = extractserialfrominstance(instanceid, isusbstor);
        std::transform(serial.begin(), serial.end(), serial.begin(), ::toupper);
        
        items.push_back({L"usb", wdevicename, serial.empty() ? L"" : serial, L""});
    }
    
    SetupDiDestroyDeviceInfoList(deviceinfoset);
    
    if (items.empty()) {
        items.push_back({L"usb", L"info", L"no connected usb devices found", L""});
    }
    
    return items;
}

std::vector<hardwareitem> hardwareinfo::getarptable() {
    std::vector<hardwareitem> items;
    
    std::map<DWORD, std::wstring> indertoname;
    
    ULONG buffersize = 0;
    GetAdaptersInfo(nullptr, &buffersize);
    
    if (buffersize > 0) {
        std::vector<BYTE> buffer(buffersize);
        PIP_ADAPTER_INFO adapterinfo = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data());
        
        if (GetAdaptersInfo(adapterinfo, &buffersize) == ERROR_SUCCESS) {
            PIP_ADAPTER_INFO current = adapterinfo;
            while (current) {
                std::string desc(current->Description);
                std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);
                indertoname[current->Index] = std::wstring(desc.begin(), desc.end());
                current = current->Next;
            }
        }
    }
    
    ULONG arpsize = 0;
    GetIpNetTable(nullptr, &arpsize, TRUE);
    
    if (arpsize == 0) {
        items.push_back({L"arp", L"info", L"no entries", L""});
        return items;
    }
    
    std::vector<BYTE> arpbuffer(arpsize);
    PMIB_IPNETTABLE arptable = reinterpret_cast<PMIB_IPNETTABLE>(arpbuffer.data());
    
    if (GetIpNetTable(arptable, &arpsize, TRUE) != NO_ERROR) {
        items.push_back({L"arp", L"error", L"getipnettable failed", L""});
        return items;
    }
    
    for (DWORD i = 0; i < arptable->dwNumEntries; i++) {
        MIB_IPNETROW& row = arptable->table[i];
        
        if (row.dwPhysAddrLen == 0) continue;
        if (row.dwType == 2) continue;
        
        IN_ADDR ipaddr;
        ipaddr.S_un.S_addr = row.dwAddr;
        std::wstring ip;
        ip = std::to_wstring((row.dwAddr) & 0xFF) + L"." +
             std::to_wstring((row.dwAddr >> 8) & 0xFF) + L"." +
             std::to_wstring((row.dwAddr >> 16) & 0xFF) + L"." +
             std::to_wstring((row.dwAddr >> 24) & 0xFF);
        
        std::wostringstream macstream;
        for (DWORD j = 0; j < row.dwPhysAddrLen && j < 6; j++) {
            if (j > 0) macstream << L":";
            macstream << std::hex << std::setfill(L'0') << std::setw(2) << (int)row.bPhysAddr[j];
        }
        
        std::wstring typestr;
        switch (row.dwType) {
            case 4: typestr = L"static"; break;
            case 3: typestr = L"dynamic"; break;
            case 1: typestr = L"other"; break;
            default: typestr = L"type " + std::to_wstring(row.dwType); break;
        }
        
        std::wstring adaptername = L"ifindex " + std::to_wstring(row.dwIndex);
        auto it = indertoname.find(row.dwIndex);
        if (it != indertoname.end()) {
            adaptername = it->second;
        }
        
        items.push_back({L"arp", ip, macstream.str(), typestr + L"; adapter: " + adaptername});
    }
    
    if (items.empty()) {
        items.push_back({L"arp", L"info", L"no arp entries found", L""});
    }
    
    return items;
}

std::wstring hardwareinfo::getwmiproperty(const std::wstring& wmiclass, const std::wstring& property) {
    std::wstring result = L"";
    
    HRESULT hres;
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres) && hres != RPC_E_CHANGED_MODE) {
        return L"";
    }
    
    hres = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    
    IWbemLocator *ploc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&ploc);
    
    if (FAILED(hres)) {
        CoUninitialize();
        return L"";
    }
    
    IWbemServices *psvc = NULL;
    hres = ploc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &psvc);
    
    if (FAILED(hres)) {
        ploc->Release();
        CoUninitialize();
        return L"";
    }
    
    hres = CoSetProxyBlanket(psvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    
    if (FAILED(hres)) {
        psvc->Release();
        ploc->Release();
        CoUninitialize();
        return L"";
    }
    
    IEnumWbemClassObject* penumerator = NULL;
    std::wstring query = L"SELECT " + property + L" FROM " + wmiclass;
    hres = psvc->ExecQuery(bstr_t("WQL"), bstr_t(query.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &penumerator);
    
    if (FAILED(hres)) {
        psvc->Release();
        ploc->Release();
        CoUninitialize();
        return L"";
    }
    
    IWbemClassObject *pcls = NULL;
    ULONG ureturn = 0;
    
    while (penumerator) {
        HRESULT hr = penumerator->Next(WBEM_INFINITE, 1, &pcls, &ureturn);
        if (0 == ureturn) {
            break;
        }
        
        VARIANT vtprop;
        hr = pcls->Get(property.c_str(), 0, &vtprop, 0, 0);
        if (SUCCEEDED(hr)) {
            if (vtprop.vt == VT_BSTR) {
                result = std::wstring(vtprop.bstrVal, SysStringLen(vtprop.bstrVal));
            } else if (vtprop.vt == VT_I4) {
                 result = std::to_wstring(vtprop.intVal);
            }
            VariantClear(&vtprop);
        }
        
        pcls->Release();
        if (!result.empty()) break;
    }
    
    psvc->Release();
    ploc->Release();
    if (penumerator) penumerator->Release();
    CoUninitialize();
    
    return result;
}
