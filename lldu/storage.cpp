
#include "storage.hpp"

#include <assert.h>
#include <iostream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <string>
#include <format>

#ifdef HAVE_WIN
#define byte win_byte_override  // Fix for c++ v17
#include <windows.h>
#undef byte                     // Fix for c++ v17
#include <winioctl.h>


#include <vector>
#include <SetupAPI.h>
#include <devguid.h>

#include <Wbemidl.h> // For WMI
#include <comdef.h>  // For _bstr_t

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "setupapi.lib")
#endif


#ifdef HAVE_WIN

bool IsProcessElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;

    // Get a handle to the access token for the current process
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        std::cerr << "OpenProcessToken failed: " << GetLastError() << std::endl;
        if (hToken) {
            CloseHandle(hToken);
        }
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD dwSize;

    // Retrieve information about the token's elevation status
    if (!GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
        std::cerr << "GetTokenInformation failed: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        return false;
    }

    isElevated = elevation.TokenIsElevated;
    CloseHandle(hToken);

    return isElevated;
}

// Function to get the drive type as a string
std::string GetDriveTypeAsString(STORAGE_BUS_TYPE type) {
    switch (type) {
    case BusTypeScsi:
        return "SCSI";
    case BusTypeAta:
        return "ATA";
    case BusTypeSata:
        return "SATA";
    case BusTypeUsb:
        return "USB";
    case BusType1394:
        return "IEEE 1394";
    case BusTypeSsa:
        return "SSA";
    case BusTypeFibre:
        return "Fibre Channel";
    case BusTypeSas:
        return "SAS";
    case BusTypeNvme:
        return "NVMe";
    default:
        return "Unknown";
    }
}


/*

To get the size of a physical storage drive on Windows without administrator privileges, 
you can't directly query the raw physical device. Instead, you need to query the logical 
volumes or partitions that reside on the physical drive. User accounts typically have access 
to these logical representations, which are assigned drive letters (like C:, D:, etc.), 
without requiring elevated permissions.

Using the Windows API (C++)
The recommended way to do this programmatically is by using the Windows Management 
Instrumentation (WMI) via the Win32_LogicalDisk class. This approach is widely supported 
and doesn't require administrator rights for basic information queries like disk size.

*/
// Does NOT requires admin privilege
int ListStorageDevicesNoAdmin() {
    // 1. Initialize COM
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM, Error: " << hr << std::endl;
        return 1;
    }

    // 2. Set general COM security levels
    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to initialize security, Error: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 3. Obtain a locator to the WMI service
    IWbemLocator* pLoc = NULL;
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create IWbemLocator object, Error: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 4. Connect to the WMI namespace
    IWbemServices* pSvc = NULL;
    hr = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &pSvc
    );

    if (FAILED(hr)) {
        std::cerr << "Could not connect to WMI, Error: " << hr << std::endl;
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 5. Set security on the proxy
    hr = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hr)) {
        std::cerr << "Could not set proxy blanket, Error: " << hr << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 6. Query for all logical disks
    IEnumWbemClassObject* pEnumerator = NULL;
    hr = pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        // _bstr_t(L"SELECT * FROM Win32_LogicalDisk WHERE DriveType = 3"), // DriveType 3 is a local disk
        _bstr_t(L"SELECT * FROM Win32_LogicalDisk"), // DriveType 3 is a local disk
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );

    if (FAILED(hr)) {
        std::cerr << "Query for logical disks failed, Error: " << hr << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 7. Iterate through the results and get the size
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;
        // Get the drive letter (DeviceID)
        hr = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
        std::wcout << std::setw(4) << vtProp.bstrVal;
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"VolumeName", 0, &vtProp, 0, 0);
        if (hr == S_OK ) {
            std::wcout << std::setw(20) << (vtProp.bstrVal != nullptr ? vtProp.bstrVal : L"" );
        }
        VariantClear(&vtProp);
        
        long long totalSize, freeSize;

        // Get the total size
        hr = pclsObj->Get(L"Size", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr) {
            totalSize = _wtoi64(vtProp.bstrVal);
            std::wcout << L" Size:" << std::setw(7) <<  std::format(L"{:L}", totalSize / ( 1024 * 1024 * 1024 )) << L" GB";
        }
        VariantClear(&vtProp);

        // Get the free space
        hr = pclsObj->Get(L"FreeSpace", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr) {
            freeSize = _wtoi64(vtProp.bstrVal);
            std::wcout << L" Free:" << std::setw(7) << std::format(L"{:L}", freeSize / ( 1024 * 1024 * 1024 )) << L" GB";
            if (totalSize > 0) 
                std::wcout << L" (" << std::setw(3) << ( freeSize * 100 / totalSize ) << "%)";
        }
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"MediaType", 0, &vtProp, 0, 0);
        if (hr == S_OK) {
            const char* type = "Unknown";
            switch (vtProp.intVal) {
            case 12:
                type = "Fixed";
                break;
            case 11:
                type = "Removable";
                break;
            case 5:
                type = "CDRom";
                break;
            default:
                break;
            }
            std::wcout << std::setw(10) << type;
        }
        VariantClear(&vtProp);

        pclsObj->Release();
        std::wcout << std::endl;
    }

    // 8. Clean up
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return 0;
}

 
int ListMediaNoAdmin() {
    // 1. Initialize COM
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM, Error: " << hr << std::endl;
        return 1;
    }

    // 2. Set general COM security levels
    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to initialize security, Error: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 3. Obtain a locator to the WMI service
    IWbemLocator* pLoc = NULL;
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create IWbemLocator object, Error: " << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 4. Connect to the WMI namespace
    IWbemServices* pSvc = NULL;
    hr = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &pSvc
    );

    if (FAILED(hr)) {
        std::cerr << "Could not connect to WMI, Error: " << hr << std::endl;
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 5. Set security on the proxy
    hr = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hr)) {
        std::cerr << "Could not set proxy blanket, Error: " << hr << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 6. Query for all logical disks
    IEnumWbemClassObject* pEnumerator = NULL;
    hr = pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT * FROM Win32_PhysicalMedia"), // DriveType 3 is a local disk
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );

    if (FAILED(hr)) {
        std::cerr << "Query for media failed, Error: " << hr << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    // 7. Iterate through the results and get the size
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    std::wcout << L"\nMedia\n";
    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;

        hr = pclsObj->Get(L"Tag", 0, &vtProp, 0, 0);
        if (hr == S_OK)
            std::wcout << std::setw(22) << ( vtProp.bstrVal != nullptr ? vtProp.bstrVal : L"" );
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
        if (hr == S_OK)
            std::wcout << std::setw(22) << ( vtProp.bstrVal != nullptr ? vtProp.bstrVal : L"" );
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"Description", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr)  
            std::wcout << std::setw(20) << vtProp.bstrVal;
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr)  
            std::wcout << std::setw(20) << vtProp.bstrVal;
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr) 
            std::wcout << std::setw(20) << vtProp.bstrVal;
        VariantClear(&vtProp);

        hr = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
        if (hr == S_OK && vtProp.bstrVal != nullptr)  
            std::wcout << std::setw(20) << vtProp.bstrVal;
        VariantClear(&vtProp);

        pclsObj->Release();
        std::wcout << std::endl;
    }

    // 8. Clean up
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return 0;
}

// Requires admin privilege
int ListStorageDevicesWithAdmin() {
    std::cout << "Listing all physical disk drives..." << std::endl;

    for (int i = 0; i < 64; ++i) { // Check up to 64 physical drives
        std::string devicePath = std::format("\\\\.\\PhysicalDrive{}", i);
        HANDLE hDevice = CreateFileA(
            devicePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (hDevice != INVALID_HANDLE_VALUE) {
            // Get disk size
            GET_LENGTH_INFORMATION diskLengthInfo;
            DWORD bytesReturned;
            if (DeviceIoControl(
                hDevice,
                IOCTL_DISK_GET_LENGTH_INFO,
                NULL,
                0,
                &diskLengthInfo,
                sizeof(diskLengthInfo),
                &bytesReturned,
                NULL
                )) {
                double sizeGB = static_cast<double>( diskLengthInfo.Length.QuadPart ) / ( 1024.0 * 1024.0 * 1024.0 );

                // Get drive type
                STORAGE_PROPERTY_QUERY query;
                STORAGE_DESCRIPTOR_HEADER header;
                query.PropertyId = StorageDeviceProperty;
                query.QueryType = PropertyStandardQuery;

                if (DeviceIoControl(
                    hDevice,
                    IOCTL_STORAGE_QUERY_PROPERTY,
                    &query,
                    sizeof(query),
                    &header,
                    sizeof(header),
                    &bytesReturned,
                    NULL
                    )) {
                    // Allocate memory for the full descriptor
                    std::vector<BYTE> buffer(header.Size);
                    PSTORAGE_DEVICE_DESCRIPTOR pDesc = reinterpret_cast<PSTORAGE_DEVICE_DESCRIPTOR>( buffer.data() );

                    if (DeviceIoControl(
                        hDevice,
                        IOCTL_STORAGE_QUERY_PROPERTY,
                        &query,
                        sizeof(query),
                        pDesc,
                        header.Size,
                        &bytesReturned,
                        NULL
                        )) {
                        std::string driveType = GetDriveTypeAsString(pDesc->BusType);
                        std::cout << "  - Disk " << i << ": " << std::fixed << std::setprecision(2) << sizeGB << " GB, Type: " << driveType << std::endl;
                    }
                }
            }
            CloseHandle(hDevice);
        }
    }
    return 0;
}



struct StorageDevice {
    std::string path;
    std::string   friendlyName, className, compatibleIds, devDescription, enumName, uiNumberStr;
    unsigned long long sizeInBytes;
};

bool getProperty(const HDEVINFO& hDevInfo, SP_DEVINFO_DATA& devInfoData, DWORD propertyId, string& result) {
    char property[1024];
    if (SetupDiGetDeviceRegistryPropertyA(
        hDevInfo,
        &devInfoData,
        propertyId,
        NULL,
        (PBYTE)property,
        sizeof(property),
        NULL
        )) {
            result = property;
            return true;
    }
    return false;
}

// Requires admin privilege
std::vector<StorageDevice> getStorageDevicesWithAdmin() {
    std::vector<StorageDevice> devices;
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVCLASS_DISKDRIVE,
        NULL,
        NULL,
        DIGCF_PRESENT
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to get device info, Error: " << GetLastError() << std::endl;
        return devices;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
        string friendlyName, className, compatibleIds, devDescription, enumName, uiNumberStr;
 
        if (getProperty( hDevInfo, devInfoData, SPDRP_FRIENDLYNAME, friendlyName)) {
            getProperty(hDevInfo, devInfoData, SPDRP_CLASS, className);
            getProperty(hDevInfo, devInfoData, SPDRP_COMPATIBLEIDS, compatibleIds);
            getProperty(hDevInfo, devInfoData, SPDRP_DEVICEDESC, devDescription);
            getProperty(hDevInfo, devInfoData, SPDRP_ENUMERATOR_NAME, enumName);
            getProperty(hDevInfo, devInfoData, SPDRP_UI_NUMBER_DESC_FORMAT, uiNumberStr);

            // getProperty(hDevInfo, devInfoData, SPDRP_REMOVAL_POLICY,  xxx);

            char deviceInstanceId[MAX_PATH];
            SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, deviceInstanceId, sizeof(deviceInstanceId), NULL);

            // Constructing a device path
            std::string devicePath = "\\\\.\\" + std::string(deviceInstanceId);

            // Getting disk size
            HANDLE hDevice = CreateFileA(
                devicePath.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (hDevice != INVALID_HANDLE_VALUE) {
                DISK_GEOMETRY_EX diskGeometry;
                DWORD bytesReturned;
                if (DeviceIoControl(
                    hDevice,
                    IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                    NULL,
                    0,
                    &diskGeometry,
                    sizeof(diskGeometry),
                    &bytesReturned,
                    NULL
                    )) {
                    devices.push_back({devicePath, friendlyName, className, compatibleIds, devDescription, enumName, uiNumberStr, ( unsigned long long )diskGeometry.DiskSize.QuadPart});
                } else {
                    std::cerr << "Failed to get disk geometry. Error: " << GetLastError() << std::endl;
                    devices.push_back({devicePath, friendlyName, 0});
                }
                CloseHandle(hDevice);
            } else {
                std::cerr << "Failed to open device  " << friendlyName << ", Error: " << GetLastError() << std::endl;
                devices.push_back({"", friendlyName, className, compatibleIds, devDescription, enumName, uiNumberStr, 0});
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return devices;
}

void Storage::ListStorageSizes() {
    std::locale::global(std::locale("en_US.UTF-8"));    // Required to get comma when using format("{:L}", value)

    bool hasAdmin =  IsProcessElevated();
    if (hasAdmin) {
        std::cout << "Have admin privilege\n";
        auto devices = getStorageDevicesWithAdmin();
        if (devices.empty()) {
            std::cout << "No storage devices found." << std::endl;
        } else {
            std::cout << "Available Storage Devices:" << std::endl;
            for (const auto& dev : devices) {
                std::cout << dev.friendlyName << std::endl;
                std::cout << "\t" << dev.className << std::endl;
                std::cout << "\t" << dev.compatibleIds << std::endl;
                std::cout << "\t" << dev.devDescription << std::endl;
                std::cout << "\t" << dev.enumName << std::endl;
                std::cout << "\t" << dev.uiNumberStr << std::endl;

                if (dev.sizeInBytes > 0) {
                    std::cout << "Path: " << dev.path << std::endl;
                    std::cout << "Size: " << ( dev.sizeInBytes / ( 1024.0 * 1024.0 * 1024.0 ) ) << " GB" << std::endl;
                }
            }
        }

        ListStorageDevicesWithAdmin();
    } else {
        std::cout << "No admin privilege\n";
        ListStorageDevicesNoAdmin();
        ListMediaNoAdmin();
    }
}

#endif

#ifdef __linux__
std::vector<StorageDevice> getStorageDevices() {
    std::vector<StorageDevice> devices;
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir("/sys/block")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name == "." || name == "..") {
                continue;
            }

            std::string size_path = "/sys/block/" + name + "/size";
            FILE *fp = fopen(size_path.c_str(), "r");
            if (fp) {
                long long blocks;
                if (fscanf(fp, "%lld", &blocks) == 1) {
                    devices.push_back({name, blocks * 512});
                }
                fclose(fp);
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Error: Could not open /sys/block." << std::endl;
    }
    return devices;
}
#endif

#ifdef HAVE_LINUX2
// You can list connected storage devices on Linux using C++17 by interacting with
// the / proc / mounts file. This approach is a lightweight, effective way to get 
// information similar to the df command without relying on external commands. 
// 
// The / proc / mounts file contains a list of currently mounted filesystems.

 
#include <string>
#include <vector>
#include <tuple>
#include <numeric>
#include <filesystem>
#include <sstream>

// A struct to hold device information
struct StorageDevice {
    std::string device_name;
    std::string mount_point;
    std::string filesystem_type;
    unsigned long long total_space_gb;
    unsigned long long available_space_gb;
};

// Function to get the size of a directory
unsigned long long get_directory_size(const std::string& path) {
    unsigned long long total_size = 0;
    try {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                if (std::filesystem::is_regular_file(entry.path())) {
                    total_size += std::filesystem::file_size(entry.path());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // Handle potential permission errors
        std::cerr << "Error accessing " << path << ": " << e.what() << std::endl;
        return 0;
    }
    return total_size;
}

// Function to get device information
std::vector<StorageDevice> getStorageDevices() {
    std::vector<StorageDevice> devices;
    std::ifstream mounts_file("/proc/mounts");
    if (!mounts_file) {
        std::cerr << "Error: Could not open /proc/mounts" << std::endl;
        return devices;
    }

    std::string line;
    while (std::getline(mounts_file, line)) {
        std::string device, mount_point, fs_type;
        std::istringstream iss(line);
        if (iss >> device >> mount_point >> fs_type) {
            // Filter out non-local filesystems and special mounts
            if (device.rfind("/dev/", 0) == 0 && fs_type != "squashfs" && fs_type != "tmpfs" && fs_type != "overlay" && fs_type != "fuse") {
                try {
                    std::filesystem::space_info si = std::filesystem::space(mount_point);
                    unsigned long long total_gb = si.capacity / ( 1024 * 1024 * 1024 );
                    unsigned long long available_gb = si.available / ( 1024 * 1024 * 1024 );
                    devices.push_back({device, mount_point, fs_type, total_gb, available_gb});
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "Error accessing space info for " << mount_point << ": " << e.what() << std::endl;
                }
            }
        }
    }
    return devices;
}

// Function to print the device information
void Storage::ListStorageSizes() {
    std::vector<StorageDevice> devices = getStorageDevices();
    
    if (devices.empty()) {
        std::cout << "No storage devices found." << std::endl;
        return;
    }
    std::cout << "Filesystem\tSize (GB)\tUsed (GB)\tAvail (GB)\tMounted on" << std::endl;
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    for (const auto& device : devices) {
        unsigned long long used_gb = device.total_space_gb - device.available_space_gb;
        std::cout << device.device_name << "\t"
            << device.total_space_gb << "\t\t"
            << used_gb << "\t\t"
            << device.available_space_gb << "\t\t"
            << device.mount_point << std::endl;
    }
}

#endif

#if defined(TARGET_OS_OSX) || defined(__APPLE__)

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/IOBSD.h>

struct StorageDevice {
    std::string name;
    std::string vendor;
    std::string model;
    bool open, writable, removable;
    long long sizeBytes;
};


std::vector<StorageDevice> getStorageDevices() {
    std::vector<StorageDevice> devices;
    kern_return_t kr;
    io_iterator_t iterator = MACH_PORT_NULL;

    // Create a matching dictionary for IOBlockStorageDriver services
    CFMutableDictionaryRef matchingDict = IOServiceMatching("IOMedia");
    if (!matchingDict) {
        std::cerr << "Failed to create matching dictionary." << std::endl;
        return devices;
    }

    // Get an iterator for the matching services
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iterator);
    if (kr != KERN_SUCCESS) {
        std::cerr << "IOServiceGetMatchingServices failed: " << kr << std::endl;
        CFRelease(matchingDict); // Remember to release if matching failed
        return devices;
    }
    
    IOOptionBits kIOOptionNone;
    io_service_t service;
    while ((service = IOIteratorNext(iterator))) {
        CFMutableDictionaryRef serviceProperties;
        kr = IORegistryEntryCreateCFProperties(service, &serviceProperties, kCFAllocatorDefault, kIOOptionNone);

        if (kr == KERN_SUCCESS && serviceProperties) {
            CFStringRef deviceName = (CFStringRef)CFDictionaryGetValue(serviceProperties, CFSTR("BSD Name"));
            if (deviceName) {
                char bsdName[128];
                if (CFStringGetCString(deviceName, bsdName, sizeof(bsdName), kCFStringEncodingUTF8)) {
                    std::cout << "Found storage device: " << bsdName << std::endl;
                }
            }

            CFStringRef deviceVendor = (CFStringRef)CFDictionaryGetValue(serviceProperties, CFSTR("Device Vendor"));
            if (deviceVendor) {
                char vendorName[128];
                if (CFStringGetCString(deviceVendor, vendorName, sizeof(vendorName), kCFStringEncodingUTF8)) {
                    std::cout << "  Vendor: " << vendorName << std::endl;
                }
            }
            
            CFStringRef deviceModel = (CFStringRef)CFDictionaryGetValue(serviceProperties, CFSTR("Device Characteristics"));
            if (deviceModel) {
                CFDictionaryRef characteristicsDict = (CFDictionaryRef)CFDictionaryGetValue(serviceProperties, CFSTR("Device Characteristics"));
                if (characteristicsDict) {
                    CFStringRef modelString = (CFStringRef)CFDictionaryGetValue(characteristicsDict, CFSTR("Model"));
                    if (modelString) {
                        char modelName[256];
                        if (CFStringGetCString(modelString, modelName, sizeof(modelName), kCFStringEncodingUTF8)) {
                            std::cout << "  Model: " << modelName << std::endl;
                        }
                    }
                }
            }
            
            CFRelease(serviceProperties);
        }
        IOObjectRelease(service);
    }

    // Clean up
    IOObjectRelease(iterator);
    
    return devices;
}

bool   getProperty(const CFDictionaryRef& properties, const char* key, std::string& value) {
    // CFStringRef nameRef = (CFStringRef) CFDictionaryGetValue(properties, CFSTR("key"));
    CFStringRef keyRef = CFStringCreateWithCString(
            kCFAllocatorDefault,
            key,
            kCFStringEncodingUTF8
        );
    
    CFStringRef nameRef = (CFStringRef) CFDictionaryGetValue(properties, keyRef);
    if (nameRef) {
        char buffer[1024];
        // kCFStringEncodingUTF8
        // kCFStringEncodingASCII
        try {
            if (CFStringGetCString(nameRef, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                value = buffer;
                return true;
            }
        } catch (...) {
        }
    }
    return false;
}
bool getProperty(const CFDictionaryRef& properties, const char* key, bool& value) {
    // CFStringRef nameRef = (CFStringRef) CFDictionaryGetValue(properties, CFSTR("key"));
    CFStringRef keyRef = CFStringCreateWithCString(
            kCFAllocatorDefault,
            key,
            kCFStringEncodingUTF8
        );
    
    CFBooleanRef nameRef = (CFBooleanRef) CFDictionaryGetValue(properties, keyRef);
    if (nameRef) {
        char buffer[1024];
        try {
            bool b = (bool)CFBooleanGetValue(nameRef);
            value = b;
            return true;
        } catch (...) {
        }
    }
    return false;
}

std::vector<StorageDevice> getStorageDevices1() {
    std::vector<StorageDevice> devices;
    kern_return_t kr;
    io_iterator_t iterator;
    io_service_t service;

    CFMutableDictionaryRef matchingDict = IOServiceMatching("IOMedia");
    if (!matchingDict) {
        std::cerr << "Error: Could not create matching dictionary." << std::endl;
        return devices;
    }

    kr = IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &iterator);
    if (kr != KERN_SUCCESS) {
        std::cerr << "Error: IOServiceGetMatchingServices failed." << std::endl;
        return devices;
    }

    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        CFDictionaryRef properties;
        IOOptionBits kIOOptionUnfiltered;
        kr = IORegistryEntryCreateCFProperties(service, (CFMutableDictionaryRef *)&properties, kCFAllocatorDefault, kIOOptionUnfiltered);

        if (kr == KERN_SUCCESS) {
            StorageDevice storageDev;
            
            getProperty(properties, "BSD Name", storageDev.name);
            getProperty(properties, "Open", storageDev.open);
            getProperty(properties, "Writable", storageDev.writable);
            getProperty(properties, "Removable", storageDev.removable);
            getProperty(properties, "Device Vendor", storageDev.vendor) || getProperty(properties, "Vendor", storageDev.vendor);
            
            CFStringRef deviceModel = (CFStringRef)CFDictionaryGetValue(properties, CFSTR("Device Characteristics"));
            if (deviceModel) {
                CFDictionaryRef characteristicsDict = (CFDictionaryRef)CFDictionaryGetValue(properties, CFSTR("Device Characteristics"));
                if (characteristicsDict) {
                    CFStringRef modelString = (CFStringRef)CFDictionaryGetValue(characteristicsDict, CFSTR("Model"));
                    if (modelString) {
                        char modelName[256];
                        if (CFStringGetCString(modelString, modelName, sizeof(modelName), kCFStringEncodingUTF8)) {
                            std::cout << "  Model: " << modelName << std::endl;
                        }
                    }
                }
            }
            CFNumberRef sizeRef = (CFNumberRef) CFDictionaryGetValue(properties, CFSTR("Size"));
            if (sizeRef) {
                long long size;
                if (CFNumberGetValue(sizeRef, kCFNumberLongLongType, &size)) {
                    storageDev.sizeBytes = size;
                }
            }

            devices.push_back(storageDev);
         
            CFRelease(properties);
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);

    return devices;
}

// Function to print the device information
void Storage::ListStorageSizes() {
    std::vector<StorageDevice> devices = getStorageDevices1();
    
    if (devices.empty()) {
        std::cout << "No storage devices found." << std::endl;
        return;
    }
    std::cout << "Filesystem  Size GB  Open  Writable  Removable  Vendor" << std::endl;
    std::cout << "----------  -------  ----  --------  --------- -----------------------" << std::endl;
    for (const auto& device : devices) {
        std::cout
            << std::setw(10) << device.name
            << std::setw(9) << (device.sizeBytes / (1024 * 1024 * 1024))
            << std::setw(6) << device.open
            << std::setw(10) << device.writable
            << std::setw(11) << device.removable
            << device.vendor << std::endl;
    }
    std::cout << std::endl;
}
#endif
