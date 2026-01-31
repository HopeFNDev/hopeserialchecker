#include "hardwareinfo.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <conio.h>

void setupconsole() {
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    
    if (hout != INVALID_HANDLE_VALUE) {
        DWORD dwmode = 0;
        if (GetConsoleMode(hout, &dwmode)) {
            dwmode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            dwmode &= ~DISABLE_NEWLINE_AUTO_RETURN;
            SetConsoleMode(hout, dwmode);
        }
    }
    
    if (hin != INVALID_HANDLE_VALUE) {
        DWORD dwmode = 0;
        if (GetConsoleMode(hin, &dwmode)) {
            dwmode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_INSERT_MODE);
            dwmode |= ENABLE_EXTENDED_FLAGS;
            SetConsoleMode(hin, dwmode);
        }
    }
    
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    SetConsoleTitleW(L"hope's serial checker");
    
    HWND consolewindow = GetConsoleWindow();
    if (consolewindow) {
        LONG style = GetWindowLong(consolewindow, GWL_STYLE);
        style &= ~(WS_MAXIMIZEBOX | WS_SIZEBOX);
        SetWindowLong(consolewindow, GWL_STYLE, style);
        
        SMALL_RECT windowsize = {0, 0, 99, 34};
        SetConsoleWindowInfo(hout, TRUE, &windowsize);
        
        COORD buffersize = {100, 300};
        SetConsoleScreenBufferSize(hout, buffersize);
    }
}

void clearscreen() {
    std::cout << "\033[2J\033[H";
}

std::string widetoutf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size, nullptr, nullptr);
    return result;
}

void printheader() {
    std::cout << R"(
  _   _                  _       ____            _       _    ____ _               _             
 | | | | ___  _ __   ___( )___  / ___|  ___ _ __(_) __ _| |  / ___| |__   ___  ___| | _____ _ __ 
 | |_| |/ _ \| '_ \ / _ \// __| \___ \ / _ \ '__| |/ _` | | | |   | '_ \ / _ \/ __| |/ / _ \ '__|
 |  _  | (_) | |_) |  __/ \__ \  ___) |  __/ |  | | (_| | | | |___| | | |  __/ (__|   <  __/ |   
 |_| |_|\___/| .__/ \___| |___/ |____/ \___|_|  |_|\__,_|_|  \____|_| |_|\___|\___|_|\_\___|_|   
             |_|                                                                                  
)" << std::endl;
}

void printmainmenu() {
    clearscreen();
    printheader();
    
    std::cout << std::endl;
    std::cout << "  select a category to view:" << std::endl;
    std::cout << std::endl;
    
    std::cout << "  ______________________________________ " << std::endl;
    std::cout << "  |  [1] bios / system information     |" << std::endl;
    std::cout << "  |  [2] cpu information               |" << std::endl;
    std::cout << "  |  [3] disk information              |" << std::endl;
    std::cout << "  |  [4] gpu information               |" << std::endl;
    std::cout << "  |  [5] network adapters              |" << std::endl;
    std::cout << "  |  [6] monitor information           |" << std::endl;
    std::cout << "  |  [7] usb devices                   |" << std::endl;
    std::cout << "  |  [8] arp table                     |" << std::endl;
    std::cout << "  |____________________________________|" << std::endl;
    std::cout << "  |  [0] exit                          |" << std::endl;
    std::cout << "  |____________________________________|" << std::endl;
    std::cout << std::endl;
    std::cout << "  press a number key to select..." << std::endl;
}

void printcategoryheader(const std::string& category) {
    std::cout << std::endl;
    std::string lowered = category;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    std::cout << "================================================================================" << std::endl;
    std::cout << " " << std::left << std::setw(78) << lowered << std::endl;
    std::cout << "================================================================================" << std::endl;
}

void printcategoryfooter() {
    std::cout << "================================================================================" << std::endl;
}

void printitem(const hardwareitem& item) {
    std::string name = widetoutf8(item.name);
    std::string value = widetoutf8(item.value);
    std::string notes = widetoutf8(item.notes);
    
    if (name.length() > 25) name = name.substr(0, 22) + "...";
    if (value.length() > 35) value = value.substr(0, 32) + "...";
    if (notes.length() > 15) notes = notes.substr(0, 12) + "...";
    
    std::cout << "| ";
    std::cout << std::left << std::setw(25) << name;
    std::cout << " | ";
    std::cout << std::left << std::setw(35) << value;
    
    if (!notes.empty()) {
        std::cout << " | " << notes;
    }
    
    std::cout << std::endl;
}

void printsection(const std::string& sectionname, const std::vector<hardwareitem>& items) {
    if (items.empty()) return;
    
    printcategoryheader(sectionname);
    
    std::map<std::wstring, std::vector<hardwareitem>> grouped;
    for (const auto& item : items) {
        grouped[item.category].push_back(item);
    }
    
    bool first = true;
    for (const auto& [category, categoryitems] : grouped) {
        if (!first) {
            std::cout << "--------------------------------------------------------------------------------" << std::endl;
        }
        first = false;
        
        if (grouped.size() > 1) {
            std::cout << "| [ " << widetoutf8(category) << " ]" << std::endl;
        }
        
        for (const auto& item : categoryitems) {
            printitem(item);
        }
    }
    
    printcategoryfooter();
}

void showcategorypage(const std::string& title, const std::vector<hardwareitem>& items) {
    clearscreen();
    printheader();
    
    printsection(title, items);
    
    std::cout << std::endl;
    std::cout << "  press esc to go back to main menu..." << std::endl;
    
    while (true) {
        int key = _getch();
        if (key == 27) {
            break;
        }
    }
}

int main() {
    setupconsole();
    clearscreen();
    printheader();
    
    hardwareinfo hwinfo;
    
    const int delay_per_fetch = 625;
    
    std::vector<hardwareitem> biosinfo, cpuinfo, diskinfo, gpuinfo, nicinfo, monitorinfo, usbinfo, arpinfo;
    
    int loadingline = 10;
    
    auto showfetching = [&](const std::string& name, int index) {
        std::cout << "\033[" << (loadingline + index) << ";1H";
        std::cout << "                                                                        ";
        std::cout << "\033[" << (loadingline + index) << ";1H";
        std::string lowername = name;
        std::transform(lowername.begin(), lowername.end(), lowername.begin(), ::tolower);
        std::cout << "  [" << (index + 1) << "] fetching " << lowername << " information..." << std::flush;
    };
    
    auto showcomplete = [&](const std::string& name, int index, int itemcount) {
        std::cout << "\033[" << (loadingline + index) << ";1H";
        std::cout << "                                                                        ";
        std::cout << "\033[" << (loadingline + index) << ";1H";
        std::string lowername = name;
        std::transform(lowername.begin(), lowername.end(), lowername.begin(), ::tolower);
        std::cout << "  [" << (index + 1) << "] + " << lowername << " (" << itemcount << " items)" << std::flush;
    };
    
    std::cout << "\033[" << (loadingline - 1) << ";1H";
    std::cout << "  initializing hardware detection..." << std::endl;
    
    showfetching("bios/system", 0);
    Sleep(delay_per_fetch);
    biosinfo = hwinfo.getbiosinfo();
    showcomplete("bios/system", 0, (int)biosinfo.size());
    
    showfetching("cpu", 1);
    Sleep(delay_per_fetch);
    cpuinfo = hwinfo.getprocessorinfo();
    showcomplete("cpu", 1, (int)cpuinfo.size());
    
    showfetching("disk", 2);
    Sleep(delay_per_fetch);
    diskinfo = hwinfo.getdiskinfo();
    showcomplete("disk", 2, (int)diskinfo.size());
    
    showfetching("gpu", 3);
    Sleep(delay_per_fetch);
    gpuinfo = hwinfo.getvideocontrollerinfo();
    showcomplete("gpu", 3, (int)gpuinfo.size());
    
    showfetching("network adapter", 4);
    Sleep(delay_per_fetch);
    nicinfo = hwinfo.getnetworkadapterinfo();
    showcomplete("network adapter", 4, (int)nicinfo.size());
    
    showfetching("monitor", 5);
    Sleep(delay_per_fetch);
    monitorinfo = hwinfo.getmonitorinfo();
    showcomplete("monitor", 5, (int)monitorinfo.size());
    
    showfetching("usb device", 6);
    Sleep(delay_per_fetch);
    usbinfo = hwinfo.getusbdevices();
    showcomplete("usb device", 6, (int)usbinfo.size());
    
    showfetching("arp table", 7);
    Sleep(delay_per_fetch);
    arpinfo = hwinfo.getarptable();
    showcomplete("arp table", 7, (int)arpinfo.size());
    
    std::cout << "\033[" << (loadingline + 9) << ";1H";
    std::cout << std::endl;
    std::cout << "  + all hardware information loaded! " << std::endl;
    std::cout << "  press any key to continue..." << std::flush;
    
    _getch();
    
    bool running = true;
    
    while (running) {
        printmainmenu();
        
        int key = _getch();
        
        switch (key) {
            case '1':
                showcategorypage("bios / system information", biosinfo);
                break;
            case '2':
                showcategorypage("cpu information", cpuinfo);
                break;
            case '3':
                showcategorypage("disk information", diskinfo);
                break;
            case '4':
                showcategorypage("gpu information", gpuinfo);
                break;
            case '5':
                showcategorypage("network adapter information", nicinfo);
                break;
            case '6':
                showcategorypage("monitor information (edid)", monitorinfo);
                break;
            case '7':
                showcategorypage("usb devices", usbinfo);
                break;
            case '8':
                showcategorypage("arp table", arpinfo);
                break;
            case '0':
            case 27:
                running = false;
                break;
            default:
                break;
        }
    }
    
    clearscreen();
    std::cout << "  goodbye!" << std::endl;
    
    return 0;
}
