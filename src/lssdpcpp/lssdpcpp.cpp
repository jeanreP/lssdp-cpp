/******************************************************************************************
*
*  Copyright 2020 Pierre Voigtlaender(jeanreP)
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this
* software and associated documentation files(the "Software"), to deal in the Software
* without restriction, including without limitation the rights to use, copy, modify,
* merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to the following
* conditions:
*
* The above copyright notice and this permission notice shall be included in all copies
* or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
* PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
* LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
******************************************************************************************/

#include <lssdpcpp/lssdpcpp.h>
#include <url/url.hpp>
#include <string.h>
#include <string>
#include <iostream>
#include <map>

#ifdef WIN32
#include <WinSock2.h>

#include <iphlpapi.h>
#include <heapapi.h>
#include <ws2tcpip.h>
#pragma warning( disable : 4996 )

#define SOCKET_TYPE SOCKET
#define ssize_t     int

namespace {
std::string getErrorAsString()
{
    auto err = WSAGetLastError();
    char msgbuf[256];
    msgbuf[0] = '\0';

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,   // flags
        NULL,                // lpsource
        err,                 // message id
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),    // languageid
        msgbuf,              // output buffer
        sizeof(msgbuf),     // size of msgbuf, bytes
        NULL);               // va_list of arguments
    std::string error_string = std::string(msgbuf) + " (" + std::to_string(err) + ")";
    return std::move(error_string);
}
}

#else //WIN32
#include <unistd.h>     // close
#include <sys/time.h>   // gettimeofday
#include <sys/ioctl.h>  // ioctl, FIONBIO
#include <net/if.h>     // struct ifconf, struct ifreq
#include <sys/socket.h> // struct sockaddr, AF_INET, SOL_SOCKET, socklen_t, setsockopt, socket, bind, sendto, recvfrom
#include <netinet/in.h> // struct sockaddr_in, struct ip_mreq, INADDR_ANY, IPPROTO_IP, also include <sys/socket.h>
#include <arpa/inet.h>  // inet_aton, inet_ntop, inet_addr, also include <netinet/in.h>

#define SOCKET_TYPE int
#ifndef _SIZEOF_ADDR_IFREQ
#define _SIZEOF_ADDR_IFREQ sizeof
#endif

namespace {
std::string getErrorAsString()
{
    std::string error_string = std::string(strerror(errno)) + " (" + std::to_string(errno) + ")";
    return std::move(error_string);
}
}

#endif //WIN32

#include <fcntl.h>      // fcntl, F_GETFD, F_SETFD, FD_CLOEXEC

#ifdef __linux__
#include <sys/utsname.h>
#endif

namespace
{
    // SSDP Header
    constexpr static const char* const LSSDP_HEADER_MSEARCH = "M-SEARCH * HTTP/1.1\r\n";
    constexpr static const char* const LSSDP_HEADER_NOTIFY = "NOTIFY * HTTP/1.1\r\n";
    constexpr static const char* const LSSDP_HEADER_RESPONSE = "HTTP/1.1 200 OK\r\n";
    // SSDP Method
    constexpr static const char* const LSSDP_MSEARCH = "M-SEARCH";
    constexpr static const char* const LSSDP_NOTIFY = "NOTIFY";
    constexpr static const char* const LSSDP_RESPONSE = "OK";
    
    constexpr static const char* const LSSDP_NOTIFY_NTS_ALIVE = "ssdp:alive";
    constexpr static const char* const LSSDP_NOTIFY_NTS_BYEBYE = "ssdp:byebye";

    constexpr static const char* const LSSDP_SEARCH_TARGET_ALL = "ssdp:all";

    constexpr static const char* const LSSDP_ADDR_LOCALHOST = "127.0.0.1";
    constexpr static const char* const LSSDP_ADDR_LOCALHOST_MASK = "255.0.0.0";

    constexpr size_t LSSDP_MAX_BUFFER_LEN = 2048;
    constexpr size_t LSSDP_FIELD_LEN = 128;
    constexpr size_t LSSDP_LOCATION_LEN = 256;

    //option for receiving from my host
    constexpr bool LSSDP_RECEIVE_PACKETS_FROM_MYSELF = true;
    //option for sending to my host
    constexpr bool LSSDP_SEND_TO_LOCALHOST = true;
}

//#define LSSDP_DEBUGGING_ON
#ifdef LSSDP_DEBUGGING_ON
namespace
{
    void logDebugMessage(const std::string& message)
    {
        std::cout << message << std::endl;
    }
}
#define LSSDP_LOG_DEBUG_MESSAGE(_expr_) do { logDebugMessage( _expr_ );  } while(false)
#else //LSSDP_DEBUGGING_ON
#define LSSDP_LOG_DEBUG_MESSAGE(_expr_) 
#endif

namespace lssdp
{

/*******************************************************************************************************/
/* Helper class Initialize the Windows Socket Lib                                                      */
/*******************************************************************************************************/
class Initializer
{
private:
    Initializer()
    {
#ifdef WIN32
        WORD wVersionRequested;
        WSADATA wsaData;
        wVersionRequested = MAKEWORD(2, 2);
        auto err = WSAStartup(wVersionRequested, &wsaData);
        if (err != 0)
        {
            throw std::runtime_error("WSAStartup failed with error: " + std::to_string(err));
        }
        else
        {
            _is_initialized = true;
        }
#endif
    }
    ~Initializer()
    {
#ifdef WIN32
        if (_is_initialized)
        {
            WSACleanup();
        }
#endif
    }
    bool _is_initialized = false;
public:
    static void init()
    {
        static Initializer initializer;
    }
};

/*******************************************************************************************************/
/* Helper Packet Parser */
/*******************************************************************************************************/
struct LSSDPPacket 
{
    char            _method[LSSDP_FIELD_LEN];      // M-SEARCH, NOTIFY, RESPONSE
    char            _st[LSSDP_FIELD_LEN];          // Search Target
    char            _usn[LSSDP_FIELD_LEN];         // Unique Service Name
    char            _location[LSSDP_LOCATION_LEN]; // Location
    char            _nts[LSSDP_FIELD_LEN];         // nts 

    /* Additional SSDP Header Fields */
    char            _sm_id[LSSDP_FIELD_LEN];
    char            _device_type[LSSDP_FIELD_LEN];

    std::chrono::system_clock::time_point _update_time;
    
    uint32_t        _received_from;

    LSSDPPacket() : _update_time(),
                    _received_from(0)
    {
        memset(_method, 0, sizeof(_method));
        memset(_st, 0, sizeof(_st));
        memset(_usn, 0, sizeof(_usn));
        memset(_location, 0, sizeof(_location));
        memset(_sm_id, 0, sizeof(_sm_id));
        memset(_device_type, 0, sizeof(_device_type));
        memset(_nts, 0, sizeof(_nts));
    }

    ~LSSDPPacket() = default;
    LSSDPPacket(const LSSDPPacket&) = default;
    LSSDPPacket& operator=(const LSSDPPacket&) = default;
    LSSDPPacket(LSSDPPacket&&) = default;
    LSSDPPacket& operator=(LSSDPPacket&&) = default;

    bool parse(const char * data, size_t data_len)
    {
        if (data == NULL)
        {
            return false;
        }

        if (data_len != strlen(data))
        {
            return false;
        }

        // 1. compare SSDP Method Header: M-SEARCH, NOTIFY, RESPONSE
        size_t i;
        if ((i = strlen(LSSDP_HEADER_MSEARCH)) < data_len && memcmp(data, LSSDP_HEADER_MSEARCH, i) == 0) {
            strcpy(_method, LSSDP_MSEARCH);
        }
        else if ((i = strlen(LSSDP_HEADER_NOTIFY)) < data_len && memcmp(data, LSSDP_HEADER_NOTIFY, i) == 0) {
            strcpy(_method, LSSDP_NOTIFY);
        }
        else if ((i = strlen(LSSDP_HEADER_RESPONSE)) < data_len && memcmp(data, LSSDP_HEADER_RESPONSE, i) == 0) {
            strcpy(_method, LSSDP_RESPONSE);
        }
        else
        {
            return false;
        }

        // 2. parse each field line
        size_t start = i;
        for (i = start; i < data_len; i++)
        {
            if (data[i] == '\n' && i - 1 > start && data[i - 1] == '\r')
            {
                parse_field_line(data, start, i - 2);
                start = i + 1;
            }
        }
        return true;
    }

    private:
    #ifdef WIN32
        size_t strncasecmp(const char* src, const char* dest, size_t len)
        {
            return _strnicmp(src, dest, len);
        }
    #endif 

    bool parse_field_line(const char * data, size_t start, size_t end) {
        // 1. find the colon
        if (data[start] == ':')
        {
            return false;
        }

        int colon = get_colon_index(data, start + 1, end);
        if (colon == -1)
        {
            return false;
        }

        if (colon == static_cast<int>(end))
        {
            // value is empty
            return true;
        }


        // 2. get field, field_len
        size_t i = start;
        size_t j = colon - 1;
        if (trim_spaces(data, &i, &j) == -1)
        {
            return false;
        }
        const char * field = &data[i];
        size_t field_len = j - i + 1;


        // 3. get value, value_len
        i = colon + 1;
        j = end;
        if (trim_spaces(data, &i, &j) == -1)
        {
            return false;
        };
        const char * value = &data[i];
        size_t value_len = j - i + 1;


        // 4. set each field's value to packet
        if (field_len == strlen("st") && strncasecmp(field, "st", field_len) == 0)
        {
            memcpy(_st, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
            return true;
        }

        if (field_len == strlen("nt") && strncasecmp(field, "nt", field_len) == 0)
        {
            memcpy(_st, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
            return true;
        }

        if (field_len == strlen("usn") && strncasecmp(field, "usn", field_len) == 0)
        {
            memcpy(_usn, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
            return true;
        }

        if (field_len == strlen("location") && strncasecmp(field, "location", field_len) == 0)
        {
            memcpy(_location, value, value_len < LSSDP_LOCATION_LEN ? value_len : LSSDP_LOCATION_LEN - 1);
            return true;
        }

        if (field_len == strlen("sm_id") && strncasecmp(field, "sm_id", field_len) == 0)
        {
            memcpy(_sm_id, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
            return true;
        }

        if (field_len == strlen("dev_type") && strncasecmp(field, "dev_type", field_len) == 0)
        {
            memcpy(_device_type, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
            return true;
        }
        if (field_len == strlen("nts") && strncasecmp(field, "nts", field_len) == 0)
        {
            memcpy(_nts, value, value_len < LSSDP_FIELD_LEN ? value_len : LSSDP_FIELD_LEN - 1);
            return true;
        }

        // the field is not in the struct packet
        return false;
    }

    int get_colon_index(const char * string, size_t start, size_t end) {
        size_t i;
        for (i = start; i <= end; i++) {
            if (string[i] == ':') {
                return (int)i;
            }
        }
        return -1;
    }

    int trim_spaces(const char * string, size_t * start, size_t * end) {
        int i = static_cast<int>(*start);
        int j = static_cast<int>(*end);

        while (i <= static_cast<int>(*end) && (!isprint(string[i]) || isspace(string[i]))) i++;
        while (j >= static_cast<int>(*start) && (!isprint(string[j]) || isspace(string[j]))) j--;

        if (i > j) {
            return -1;
        }

        *start = i;
        *end = j;
        return 0;
    }
};


/*******************************************************************************************************/
/* Helper class OS Version */
/*******************************************************************************************************/
class OSVersion
{
private:
    std::string _os_name;
    std::string _os_version;

    OSVersion()
    {
#ifdef WIN32
        _os_name = "Windows";
        OSVERSIONINFO   vi;
        memset(&vi, 0, sizeof vi);
        vi.dwOSVersionInfoSize = sizeof vi;
        GetVersionEx(&vi);
        _os_version = std::to_string(vi.dwMajorVersion) + "." + std::to_string(vi.dwMinorVersion);
#elif __linux__
        struct utsname buf;
        int retval = uname(&buf);
        if (0 == retval)
        {
            _os_name = buf.sysname;
            _os_version = buf.release;
        }
        else
        {
            _os_name = "Linux";
            _os_version = "version";
        }
#elif __unix__
        _os_name = "unix";
        _os_version = "version";
#elif __APPLE__
        _os_name = "apple";
        _os_version = "version";
#elif __FreeBSD__
        _os_name = "freebsd";
        _os_version = "version";
#elif __ANDROID__
        _os_name = "android";
        _os_version = "version";
#endif
    }
public:
    static OSVersion getOsVersion()
    {
        static OSVersion version;
        return version;
    }
    std::string getName() const
    {
        return _os_name;
    }
    std::string getVersion() const
    {
        return _os_version;
    }
};

/*****************************************************************************************/
struct NetworkInterface::Impl
{
    Impl(const std::string& name,
                  const std::string& ip4,
                  const std::string& netmask_ip4) : 
        _name(name),
        _ip4(ip4),
        _netmask_ip4(netmask_ip4),
        _addr_ip4(inet_addr(ip4.c_str())),
        _addr_netmask_ip4(inet_addr(netmask_ip4.c_str()))
    {
    }
    Impl(const std::string& name,
        uint32_t addr_ip4,
        uint32_t addr_netmask_ip4) :
        _name(name),
        _ip4(inet_ntoa(*reinterpret_cast<struct in_addr*>(&addr_ip4))),
        _netmask_ip4(inet_ntoa(*reinterpret_cast<struct in_addr*>(&addr_netmask_ip4))),
        _addr_ip4(addr_ip4),
        _addr_netmask_ip4(addr_netmask_ip4)
    {
    }
    ~Impl()
    {
    }
    std::string _name;
    std::string _ip4;
    std::string _netmask_ip4;
    uint32_t _addr_ip4;
    uint32_t _addr_netmask_ip4;
};

NetworkInterface::~NetworkInterface()
{
}

NetworkInterface::NetworkInterface(const std::string& name,
    const std::string& ip4,
    const std::string& netmask_ip4) :
    _impl(new Impl(name, ip4, netmask_ip4))
{
}

NetworkInterface::NetworkInterface(const std::string& name,
    uint32_t addr_ip4,
    uint32_t addr_netmask_ip4) :
    _impl(new Impl(name, addr_ip4, addr_netmask_ip4))
{
}

NetworkInterface::NetworkInterface(const NetworkInterface& other) : 
    _impl(new Impl(other.getName(), other.getIp4(), other.getNetMaskIp4()))
{
}

NetworkInterface& NetworkInterface::operator=(const NetworkInterface& other)
{
    _impl.reset(new Impl(other.getName(), other.getIp4(), other.getNetMaskIp4()));
    return *this;
}

std::string NetworkInterface::getName() const
{
    return _impl->_name;
}

std::string NetworkInterface::getIp4() const
{
    return _impl->_ip4;
}

std::string NetworkInterface::getNetMaskIp4() const
{
    return _impl->_netmask_ip4;
}

std::uint32_t NetworkInterface::getAddrIp4() const
{
    return _impl->_addr_ip4;
}

std::uint32_t NetworkInterface::getAddrNetMaskIp4() const
{
    return _impl->_addr_netmask_ip4;
}

bool NetworkInterface::operator==(const NetworkInterface& other) const
{
    //we only compare the addresses ... no strings ... its always the same
    return (getName() == other.getName()
        && getAddrIp4() == other.getAddrIp4()
        && getAddrNetMaskIp4() == other.getAddrNetMaskIp4());
}

bool updateNetworkInterfaces(std::vector<NetworkInterface>& interfaces)
{
    Initializer::init();

    std::vector<NetworkInterface> new_interfaces;

#ifdef WIN32
    #define WORKING_BUFFER_SIZE 15000
    #define MAX_TRIES 3

    #define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
    #define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

    DWORD dwRetVal = 0;

    ULONG byte_size_of_adapters_info = 1 * sizeof(IP_ADAPTER_INFO);
    PIP_ADAPTER_INFO p_adaptersinfo = static_cast<PIP_ADAPTER_INFO>(MALLOC(byte_size_of_adapters_info));
    dwRetVal = GetAdaptersInfo(p_adaptersinfo, &byte_size_of_adapters_info);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW)
    {
        FREE(p_adaptersinfo);
        if (byte_size_of_adapters_info > 0)
        {
            p_adaptersinfo = static_cast<PIP_ADAPTER_INFO>(MALLOC(byte_size_of_adapters_info));
        }
        else
        {
            throw std::runtime_error("Error calling GetAdaptersInfo");
        }
    }
    else if (dwRetVal != NO_ERROR)
    {
        FREE(p_adaptersinfo);
        throw std::runtime_error("Call to GetAdaptersAddresses failed with error: " + std::to_string(dwRetVal));
    }
    dwRetVal = GetAdaptersInfo(p_adaptersinfo, &byte_size_of_adapters_info);
    if (dwRetVal != NO_ERROR)
    {
        FREE(p_adaptersinfo);
        throw std::runtime_error("Call to GetAdaptersAddresses failed with error: " + std::to_string(dwRetVal));
    }
    
    new_interfaces.emplace_back(NetworkInterface("localhost", LSSDP_ADDR_LOCALHOST, LSSDP_ADDR_LOCALHOST_MASK));

    PIP_ADAPTER_INFO current_p_adaptersinfo = p_adaptersinfo;
    while (current_p_adaptersinfo != NULL)
    {
        PIP_ADDR_STRING current_address = &(current_p_adaptersinfo->IpAddressList);
        while (current_address != NULL)
        {
            uint32_t addr = inet_addr(current_address->IpAddress.String);
            if (addr != 0)
            {
                new_interfaces.emplace_back(NetworkInterface(current_p_adaptersinfo->Description,
                                                             current_address->IpAddress.String,
                                                             current_address->IpMask.String));
            }
             current_address = current_address->Next;
        }
         current_p_adaptersinfo = current_p_adaptersinfo->Next;
        
    }
    FREE(p_adaptersinfo);

#else //WIN32
    /* Reference to this article:
    * http://stackoverflow.com/a/8007079
    */

    // in lin create UDP socket
    SOCKET_TYPE fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) 
    {
        throw std::runtime_error(std::string("create socket failed, errno = ") 
                                 + getErrorAsString());
    }
#define LOCAL_BUFFER_LEN    2048
    // get ifconfig
    char buffer[LOCAL_BUFFER_LEN];
    struct ifconf ifc;
    ifc.ifc_len = sizeof(buffer);
    ifc.ifc_buf = (caddr_t)buffer;

    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0)
    {
        throw std::runtime_error(std::string("ioctl SIOCGIFCONF failed, errno = ") 
                                 + getErrorAsString());
    }

    // set to new interfaces 
    int i;
    struct ifreq * ifr;
    for (i = 0; i < ifc.ifc_len; i += _SIZEOF_ADDR_IFREQ(*ifr))
    {
        ifr = (struct ifreq *)(buffer + i);
        if (ifr->ifr_addr.sa_family != AF_INET) {
            // only support IPv4
            continue;
        }

        // get interface ip address
        struct sockaddr_in * addr = (struct sockaddr_in *) &ifr->ifr_addr;

        // get network mask
        struct ifreq netmask = {};
        strcpy(netmask.ifr_name, ifr->ifr_name);
        if (ioctl(fd, SIOCGIFNETMASK, &netmask) != 0) 
        {
            continue;
        }
        struct sockaddr_in * netmask_addr  = (struct sockaddr_in *) &netmask.ifr_addr;

        new_interfaces.emplace_back(NetworkInterface(ifr->ifr_name,
                                                     addr->sin_addr.s_addr,
                                                     netmask_addr->sin_addr.s_addr));
    }
       
    // close socket
    if (fd >= 0 && close(fd) != 0)
    {
       throw std::runtime_error(std::string("closing of socket failed, errno = ") 
                                 + getErrorAsString());
    }

#endif //WIN32
    bool is_equal = new_interfaces == interfaces;
    if (is_equal)
    {
        return false;
    }
    else
    {
        interfaces = new_interfaces;
        return true;
    }
}

/*****************************************************************************************/
ServiceDescription::ServiceDescription() : 
    _location_url(),
    _unique_service_name(),
    _search_target(),
    _device_type(),
    _product_name(),
    _product_version()
{
}

ServiceDescription::ServiceDescription(
    const std::string& location_url,
    const std::string& unique_service_name,
    const std::string& search_target,
    const std::string& product_name,
    const std::string& product_version,
    const std::string& sm_id,
    const std::string& device_type) :
    _location_url(location_url),
    _unique_service_name(unique_service_name),
    _search_target(search_target),
    _sm_id(sm_id),
    _device_type(device_type),
    _product_name(product_name),
    _product_version(product_version)
{
}
ServiceDescription::~ServiceDescription()
{
}

bool ServiceDescription::operator==(const ServiceDescription& other) const
{
    return (other.getSearchTarget() == getSearchTarget()
            && other.getUniqueServiceName() == getUniqueServiceName());
}

std::string ServiceDescription::getLocationURL() const
{
    return _location_url;
}

std::string ServiceDescription::getUniqueServiceName() const
{
    return _unique_service_name;
}
std::string ServiceDescription::getSearchTarget() const
{
    return _search_target;
}
std::string ServiceDescription::getSMID() const
{
    return _sm_id;
}

std::string ServiceDescription::getDeviceType() const
{
    return _device_type;
}


std::string ServiceDescription::getProductName() const
{
    return _product_name;
}

std::string ServiceDescription::getProductVersion() const
{
    return _product_version;
}

/**********************************************************************************/
class NonBlockingMulticastSocket
{
public:
    NonBlockingMulticastSocket() = default;
    ~NonBlockingMulticastSocket()
    {
        close();
    }
    void open(uint32_t multicast_socket_addr, uint16_t multicast_socket_port)
    {
        Initializer::init();

        close();

        if (multicast_socket_port == 0)
        {
            throw std::runtime_error(std::string("SSDP port ")
                + std::to_string(multicast_socket_port) + " has not been setup right");
        }
        _multicast_socket_addr = multicast_socket_addr;
        _multicast_socket_port = multicast_socket_port;

        // create UDP socket
        _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_socket < 0) 
        {
            std::string throw_msg = std::string("create socket failed, errno = ")
                + getErrorAsString();
            close();
            throw std::runtime_error(throw_msg);
        }

#ifdef WIN32
        u_long mode = 1;
        if (ioctlsocket(_socket, FIONBIO, &mode) != 0)
        {
            std::string throw_msg = std::string("ioctl FIONBIO failed, errno = ")
                + getErrorAsString();
            close();
            throw std::runtime_error(throw_msg);
        }
        // set reuse address
        int opt = 1;
        if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) != 0)
        {
            std::string throw_msg = std::string("setsockopt SO_REUSEADDR failed, errno = ")
                + getErrorAsString();
            close();
            throw std::runtime_error(throw_msg);
        }
#else // WIN32
        int opt = 1;
        if (ioctl(_socket, FIONBIO, &opt) != 0)
        {
            std::string throw_msg = std::string("ioctl FIONBIO failed, errno = ")
                + getErrorAsString();
            close();
            throw std::runtime_error(throw_msg);
        }
        // set reuse address
        if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0)
        {
            std::string throw_msg = std::string("setsockopt SO_REUSEADDR failed, errno = ")
                + getErrorAsString();
            close();
            throw std::runtime_error(throw_msg);
        }
#endif // WIN32

#ifdef WIN32
       //found no solution for that in WinSock API 
#else
        // set FD_CLOEXEC (http://kaivy2001.pixnet.net/blog/post/32726732)
        int sock_opt = fcntl(_socket, F_GETFD);
        if (sock_opt == -1)
        {
            std::string throw_msg = std::string("fcntl F_GETFD failed, errno = ")
                + getErrorAsString();
            close();
            throw std::runtime_error(throw_msg);
        }
        else
        {
            // F_SETFD
            if (fcntl(_socket, F_SETFD, sock_opt | FD_CLOEXEC) == -1)
            {
                std::string throw_msg = std::string("fcntl F_SETFD FD_CLOEXEC failed, errno = ")
                    + getErrorAsString();
                close();
                throw std::runtime_error(throw_msg);
            }
        }
#endif
        // bind socket
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(multicast_socket_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        
        if (bind(_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        {
            std::string throw_msg = std::string("bind failed to ADDR ANY for multicast, errno = ")
                + getErrorAsString();
            close();
            throw std::runtime_error(throw_msg);
        }

        struct in_addr multicast_address;
        memset(&multicast_address, 0, sizeof(multicast_address));
        multicast_address.s_addr = multicast_socket_addr;

        // set IP_ADD_MEMBERSHIP
        struct ip_mreq imr;
        memset(&imr, 0, sizeof(imr));
        imr.imr_multiaddr.s_addr = multicast_address.s_addr;
        imr.imr_interface.s_addr = htonl(INADDR_ANY);
        
#ifdef WIN32
        if (setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&imr, sizeof(imr)) != 0)
        {
            std::string throw_msg = std::string("setsockopt IP_ADD_MEMBERSHIP failed, errno = ")
                + getErrorAsString();
            close();
            throw std::runtime_error(throw_msg);
        }
#else
        if (setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(imr)) != 0)
        {
            std::string throw_msg = std::string("setsockopt IP_ADD_MEMBERSHIP failed, errno = ")
                + getErrorAsString();
            close();
            throw std::runtime_error(throw_msg);
        }

#endif
    }

    void closeSocket(SOCKET_TYPE* socket_to_close)
    {
#ifdef WIN32
        closesocket(*socket_to_close);
#else //WIN32
        ::close(*socket_to_close);
#endif
    }

    void close()
    {

        // check lssdp->sock
        if (_socket > 0)
        {
            // close socket
            closeSocket(&_socket);
        }

        _socket = 0;
        _multicast_socket_addr = 0;
        _multicast_socket_port = 0;
    }

    void sendDataTo(const char* data, uint32_t address, uint16_t port)
    {
        if (data == nullptr)
        {
            throw std::runtime_error("invalid data");
        }

        size_t data_len = strlen(data);
        if (data_len == 0)
        {
            throw std::runtime_error("invalid data size");
        }
        // 1. create UDP socket
        SOCKET_TYPE fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0)
        {
            std::string throw_msg = std::string("create socket failed, errno = ")
                + getErrorAsString();
            closeSocket(&fd);
            throw std::runtime_error(throw_msg);
        }

        // 2. bind socket
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = address;
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            std::string throw_msg = std::string("bind to address ")
                                    + inet_ntoa(addr.sin_addr)
                                    + std::string(" failed, errno =  ")
                                    + getErrorAsString();
            closeSocket(&fd);
            throw std::runtime_error(throw_msg);
        }

        // 3. enable IP_MULTICAST_LOOP for us, because we want that if the 
        //    option "send to myself is set"
        if (LSSDP_SEND_TO_LOCALHOST)
        {
            int opt = 1;
#ifdef WIN32
            const char* value_to_set = (const char*)&opt;
#else
            int* value_to_set = &opt;
#endif

            if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, value_to_set, sizeof(opt)) != 0)
            {
                std::string throw_msg = std::string("setsockopt IP_MULTICAST_LOOP failed, errno = ")
                    + getErrorAsString();
                closeSocket(&fd);
                throw std::runtime_error(throw_msg);
            }
        }

        // 4. set destination address
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        dest_addr.sin_addr.s_addr = _multicast_socket_addr;
        
        // 5. send data
        int send_data_size = sendto(fd, data, (int)data_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (send_data_size < 0)
        {
            std::string throw_msg = std::string("sendto ") + inet_ntoa(addr.sin_addr) + ":" + std::to_string(port) 
                + " for multicast address "+ inet_ntoa(dest_addr.sin_addr) + ":" + std::to_string(port)
                + " failed, errno = "
                + getErrorAsString();
            closeSocket(&fd);
            throw std::runtime_error(throw_msg);
        }
        else
        {
            LSSDP_LOG_DEBUG_MESSAGE(std::to_string(send_data_size) + " size sent");
        }
        
        closeSocket(&fd);
    }

    std::pair<bool, LSSDPPacket> receivePacket()
    {
        // check socket and port
        if (_socket <= 0 || _multicast_socket_port == 0)
        {
            throw std::runtime_error(std::string("invalid state of multicast port"));
        }
        char buffer[LSSDP_MAX_BUFFER_LEN];
        memset(buffer, 0, sizeof(buffer));
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        socklen_t address_len = sizeof(struct sockaddr_in);

        ssize_t recv_len = recvfrom(_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&address, &address_len);
        if (recv_len < 0)
        {
            LSSDP_LOG_DEBUG_MESSAGE("receive lower 0");
            if (errno)
            {
                throw std::runtime_error(std::string("recvfrom ") + inet_ntoa(*(in_addr*)&_multicast_socket_addr)
                    + " failed, errno = "
                    + getErrorAsString());
            }
        }
        else if (recv_len == 0)
        {
            LSSDP_LOG_DEBUG_MESSAGE("receive 0");
            //socket has been closed
        }
        else
        {
            LSSDP_LOG_DEBUG_MESSAGE(std::string("Packet received: ") + std::string(buffer));

            LSSDPPacket packet;
            packet._update_time = std::chrono::system_clock::now();
            packet._received_from = address.sin_addr.s_addr;

            auto valid = packet.parse(buffer, recv_len);

            return { valid, std::move(packet) };
        }
        return { false, LSSDPPacket() };
    }

    SOCKET_TYPE _socket = 0;
    uint32_t    _multicast_socket_addr = 0;
    uint16_t    _multicast_socket_port = 0;
};


/*****************************************************************************************/
struct Service::Impl : public ServiceDescription
{
    Impl(std::string discover_url,
        std::chrono::seconds max_age,
        std::string location_url,
        std::string unique_service_name,
        std::string search_target,
        std::string product_name,
        std::string product_version,
        std::string sm_id,
        std::string device_type) 
        : ServiceDescription(location_url,
                             unique_service_name,
                             search_target,
                             product_name,
                             product_version,
                             sm_id,
                             device_type)
    {
        cxxurl::Url url(discover_url);

        _port = std::stoi(url.port());

        //very important !! this must be an IP address ... we check that
        if (url.ip_version() != 4)
        {
            throw std::runtime_error("The given url " + discover_url + " does not contain a IPv4 multicast address for host");
        }
        _address = inet_addr(url.host().c_str());

        //prepare notify alive message
        _notify_alive_message = 
            std::string(LSSDP_HEADER_NOTIFY)
            + "HOST:" + url.host() + ":" + url.port() + "\r\n"
            + "CACHE-CONTROL:max-age=" + std::to_string(max_age.count()) + "\r\n"
            + "LOCATION:" + getLocationURL() +"\r\n"
            + "SERVER:" + OSVersion::getOsVersion().getName() + "/" + OSVersion::getOsVersion().getVersion()
                        + " " + getProductName() + "/" + getProductVersion() + "\r\n"
            + "NT:" + getSearchTarget() + "\r\n"
            + "NTS:" + LSSDP_NOTIFY_NTS_ALIVE + "\r\n"
            + "USN:" + getUniqueServiceName() + "\r\n";
            
        if (!getSMID().empty())
        {
            _notify_alive_message += "SM_ID:" + getSMID() + "\r\n";
        }
        if (!getDeviceType().empty())
        {
            _notify_alive_message += "DEV_TYPE:" + getDeviceType() + "\r\n";
        }
        _notify_alive_message += std::string("\r\n");

        //prepare notify byebye message
        _notify_byebye_message = 
            std::string(LSSDP_HEADER_NOTIFY)
            + "HOST:" + url.host() + ":" + url.port() + "\r\n"
            + "NT:" + getSearchTarget() + "\r\n"
            + "NTS:" + LSSDP_NOTIFY_NTS_BYEBYE + "\r\n"
            + "USN:" + getUniqueServiceName() + "\r\n"
            + std::string("\r\n");
        
        //prepare response message
        _response_message =
            std::string(LSSDP_HEADER_RESPONSE)
            + "CACHE-CONTROL:max-age=" + std::to_string(max_age.count()) + "\r\n"
            + "DATE:\r\n"
            + "EXT:\r\n"
            + "LOCATION:" + getLocationURL() + "\r\n"
            + "SERVER:" + OSVersion::getOsVersion().getName() + "/" + OSVersion::getOsVersion().getVersion()
            + " " + getProductName() + "/" + getProductVersion() + "\r\n"
            + "ST:" + getSearchTarget() + "\r\n"
            + "USN:" + getUniqueServiceName() + "\r\n";
        if (!getSMID().empty())
        {
            _response_message += "SM_ID: " + getSMID() + "\r\n";
        }
        if (!getDeviceType().empty())
        {
            _response_message += "DEV_TYPE: " + getDeviceType() + "\r\n";
        }
        _response_message += std::string("\r\n");

        //open the socket NOW for the NOTIFY Messages 
        ::lssdp::updateNetworkInterfaces(_network_interfaces);
        openSocket();
    }
    ~Impl()
    {
        closeSocket();
    }

    void openSocket()
    {
        _multicast_socket.open(_address, _port);
    }

    void closeSocket()
    {
        _multicast_socket.close();
    }

    void updateNetworkInterfaces()
    {
        bool updated = ::lssdp::updateNetworkInterfaces(_network_interfaces);
        if (updated)
        {
            closeSocket();
            openSocket();
        }
    }

    enum MessageType
    {
        alive,
        byebye
    };

    bool sendNotify(MessageType m_type)
    {
        bool error_occured = false;
        updateNetworkInterfaces();
        const char* message_to_send = _notify_alive_message.c_str();
        if (m_type == byebye)
        {
            message_to_send = _notify_byebye_message.c_str();
        }
        for (const auto& current_interface : _network_interfaces)
        {
            if (!LSSDP_SEND_TO_LOCALHOST)
            {
                if (current_interface.getIp4() == LSSDP_ADDR_LOCALHOST)
                {
                    continue;
                }
            }
            try
            {
                _multicast_socket.sendDataTo(message_to_send,
                    current_interface.getAddrIp4(),
                    _port);
            }
            catch (std::runtime_error& ex)
            {
                error_occured = true;
                _send_errors[current_interface.getIp4()] = ex.what();
            }
        }
        return (!error_occured);
    }

    bool sendResponse(uint32_t address_to)
    {
        bool error_occured = false;
        bool found = false;
        std::string found_address;
        // 1. find the interface which is in LAN
        for (const auto& intf : _network_interfaces)
        {
            if ((intf.getAddrIp4() & intf.getAddrNetMaskIp4())
                == (address_to & intf.getAddrNetMaskIp4()))
            {
                found = true;
                found_address = intf.getIp4();
            }
        }

        if (!found)
        {
            return true;
        }

        try
        {
            _multicast_socket.sendDataTo(_response_message.c_str(),
                address_to,
                _port);
        }
        catch (std::runtime_error& ex)
        {
            error_occured = true;
            _send_errors[found_address] = ex.what();
        }
        LSSDP_LOG_DEBUG_MESSAGE(std::string("send response: ") + response_message_generic);
        return (!error_occured);
    }

    std::string getSendErrors()
    {
        std::string created_message;
        for (const auto& current : _send_errors)
        {
            created_message += current.second;
        }
        _send_errors.clear();
        return created_message;
    }
    

private:
    friend class Service;

    uint16_t _port = 0;
    uint32_t _address = 0;

    std::string _dicover_url;

    std::string _notify_alive_message;
    std::string _notify_byebye_message;
    std::string _response_message;
   

    std::vector<NetworkInterface>   _network_interfaces;
    NonBlockingMulticastSocket      _multicast_socket;
    std::map<std::string, std::string> _send_errors;

};

/*****************************************************************************************/
Service::Service(std::string discover_url,
                 std::chrono::seconds max_age,
                 std::string location_url,
                 std::string unique_service_name,
                 std::string search_target,
                 std::string product_name,
                 std::string product_version,
                 std::string sm_id ,
                 std::string device_type) :
    _impl(std::make_unique<Impl>(discover_url,
        max_age,
        location_url,
        unique_service_name,
        search_target,
        product_name,
        product_version,
        sm_id,
        device_type
        ))
{
}

Service::~Service()
{
}

bool Service::sendNotifyAlive()
{
    return _impl->sendNotify(Impl::alive);
}

bool Service::sendNotifyByeBye()
{
    return _impl->sendNotify(Impl::byebye);
}

bool Service::checkForMSearchAndSendResponse(std::chrono::milliseconds timeout)
{
    fd_set fs;
    FD_ZERO(&fs);
    FD_SET(_impl->_multicast_socket._socket, &fs);
    struct timeval tv;
    memset(&tv, 0, sizeof(tv));
    tv.tv_usec = 100 * 1000;   // 100 ms

    auto begin_time = std::chrono::system_clock::now();
    bool return_value = true;
    bool go_ahead = true;
    bool error_while_sending = false;

    do
    {
        #ifdef WIN32
        int used_socket_in_select = 0;
        #else
        int used_socket_in_select = _impl->_multicast_socket._socket + 1;
        #endif
        int ret = select(used_socket_in_select, &fs, NULL, NULL, &tv);
        if (ret < 0)
        {
            std::string error_msg = std::string("select on ") + _impl->_dicover_url
                + " failed, errno = "
                + getErrorAsString();
            _impl->_send_errors[_impl->_dicover_url] = error_msg;
            go_ahead = false;
            return_value = false;
        }
        else if (ret == 0)
        {
            FD_SET(_impl->_multicast_socket._socket, &fs);
            //we check for the overall timeout at the and of the loop
            go_ahead = true;
        }
        else
        {
            std::pair<bool, LSSDPPacket> packet = _impl->_multicast_socket.receivePacket();
            if (packet.first)
            {
                if (strcmp(packet.second._method, LSSDP_MSEARCH) == 0)
                {
                    if (strcmp(packet.second._st, LSSDP_SEARCH_TARGET_ALL) == 0
                        || strcmp(packet.second._st, _impl->getSearchTarget().c_str()) == 0)
                    {
                        if (!_impl->sendResponse(packet.second._received_from))
                        {
                            error_while_sending = true;
                        }
                    }
                }
            }
        }

        //timeout check
        if ((std::chrono::system_clock::now() - begin_time) >= timeout)
        {
            go_ahead = false;
        }

    } while (go_ahead);
    return (return_value && !error_while_sending);
}

bool Service::operator==(const ServiceDescription& other) const
{
    return (other == *_impl.get());
}

ServiceDescription Service::getServiceDescription() const
{
    return *_impl;
}

std::string Service::getLastSendErrors() const
{
    return _impl->getSendErrors();
}




/*****************************************************************************************/
class ServiceFinder::Impl
{
public:
    Impl() = delete;
    virtual ~Impl()
    {
        closeSocket();
    }
    
    Impl(const Impl& other)
    {
        //todo : make that ready !
        _discover_url = other._discover_url;
        _search_target = other._search_target;
        _device_type_filter = other._device_type_filter;
        _port = other._port;
    }

    Impl(const std::string& discover_url,
         const std::string& product_name,
         const std::string& product_version,
         const std::string& search_target,
         const std::string& device_type_filter)
        : _discover_url(discover_url),
          _search_target(search_target),
          _device_type_filter(device_type_filter),
          _rcv_packetes_from_myself(true),
          _send_to_localhost(true)
    {
         cxxurl::Url url(discover_url);

         _port = static_cast<uint16_t>(std::stoi(url.port()));

         //very important !! this must be an IP address ... we check that
         if (url.ip_version() != 4)
         {
             throw std::runtime_error("The given url " + discover_url + " does not contain a IPv4 multicast address for host");
         }
         _address = inet_addr(url.host().c_str());
         
         //set to ssdp:all 
         if (_search_target.empty())
         {
             _search_target = LSSDP_SEARCH_TARGET_ALL;
         }

         //preparing the message to prevent string memory allocation on each request m-search requaest
         _m_search_message =
             std::string(LSSDP_HEADER_MSEARCH)
             + "HOST:" + url.host() + ":" + url.port() + "\r\n"
             + "MAN:\"ssdp:discover\"\r\n"
             + "MX:5\r\n"
             + "ST:" + _search_target + "\r\n"
             + "USER-AGENT:" + OSVersion::getOsVersion().getName() + "/" + OSVersion::getOsVersion().getVersion()
                             + " " + product_name + "/" + product_version + "\r\n"
             + std::string("\r\n");

         //open the socket NOW for the M SEARCH AND NOTIFY Messages 
         ::lssdp::updateNetworkInterfaces(_network_interfaces);
         //open socket
         openSocket();
    }

    void openSocket()
    {
        _multicast_socket.open(_address, _port);
    }

    void closeSocket()
    {
        _multicast_socket.close();
    }

    void updateNetworkInterfaces()
    {
        bool updated = ::lssdp::updateNetworkInterfaces(_network_interfaces);
        if (updated)
        {
            closeSocket();
            openSocket();
        }
    }

    bool sendMSearch()
    {
        bool error_occured = false;
        updateNetworkInterfaces();
        for (const auto& current_interface : _network_interfaces)
        {
            if (!_send_to_localhost)
            {
                if (current_interface.getIp4() == LSSDP_ADDR_LOCALHOST)
                {
                    continue;
                }
            }
            try
            {
                _multicast_socket.sendDataTo(_m_search_message.c_str(),
                    current_interface.getAddrIp4(),
                    _port);
            }
            catch (const std::runtime_error& ex)
            {
                error_occured = true;
                _send_errors[current_interface.getIp4()] = ex.what();
            }
        }
        return (!error_occured);
    }

    std::string getSendErrors() 
    {
        std::string created_message;
        for (const auto& current : _send_errors)
        {
            created_message += current.second;
        }
        _send_errors.clear();
        return created_message;
    }

private:
    friend class ServiceFinder;
    bool _rcv_packetes_from_myself;
    bool _send_to_localhost;

    uint16_t _port = 0;
    uint32_t _address = 0;

    std::string _discover_url;
    std::string _search_target;
    std::string _device_type_filter;

    std::string _m_search_message;

    std::vector<NetworkInterface>   _network_interfaces;
    NonBlockingMulticastSocket      _multicast_socket;

    std::map<std::string, std::string> _send_errors;
};

ServiceFinder::~ServiceFinder()
{
}

ServiceFinder::ServiceFinder(const std::string& url,
                             const std::string& product_name,
                             const std::string& product_version,
                             const std::string& search_target,
                             const std::string& device_type_filter) 
    : _impl(std::make_unique<Impl>(url, 
                                   product_name,
                                   product_version,
                                   search_target,
                                   device_type_filter))
{
}

std::string ServiceFinder::getUrl() const
{
    return _impl->_discover_url;
}

bool ServiceFinder::sendMSearch()
{
    return _impl->sendMSearch();
}

void ServiceFinder::checkNetworkChanges()
{
    _impl->updateNetworkInterfaces();
}

bool ServiceFinder::checkForServices(const std::function<void(const ServiceUpdateEvent& update_service)>& update_callback,
                                     std::chrono::milliseconds timeout)
{
    fd_set fs;
    FD_ZERO(&fs);
    FD_SET(_impl->_multicast_socket._socket, &fs);
    struct timeval tv;
    memset(&tv, 0, sizeof(tv));
    tv.tv_usec = 100 * 1000;   // 100 ms

    auto begin_time = std::chrono::system_clock::now();
    bool return_value = true;
    bool go_ahead = true;
    do
    {
        #ifdef WIN32
        int used_socket_in_select = 0;
        #else
        int used_socket_in_select = _impl->_multicast_socket._socket + 1;
        #endif
        int ret = select(used_socket_in_select, &fs, NULL, NULL, &tv);
        if (ret < 0)
        {
            std::string error_msg = std::string("select on ") + _impl->_discover_url
                + " failed, errno = "
                + getErrorAsString();
            _impl->_send_errors[_impl->_discover_url] = error_msg;
            go_ahead = false;
            return_value = false;
        } 
        else if (ret == 0)
        {
            FD_SET(_impl->_multicast_socket._socket, &fs);
            //we check for the overall timeout at the and of the loop
            go_ahead = true;
        }
        else
        {
            std::pair<bool, LSSDPPacket> packet = _impl->_multicast_socket.receivePacket();
            if (packet.first)
            {
                bool package_interest = true;
                if (!_impl->_device_type_filter.empty())
                {
                    if (strcmp(packet.second._device_type, _impl->_device_type_filter.c_str()) != 0)
                    {
                        //its not out device looking for
                        package_interest = false;
                    }
                }
                if (!_impl->_search_target.empty() && _impl->_search_target != std::string(LSSDP_SEARCH_TARGET_ALL))
                {
                    if (strcmp(packet.second._st, _impl->_search_target.c_str()) != 0)
                    {
                        //its not our target looking for
                        package_interest = false;
                    }
                }
                if (package_interest)
                {
                    if (strcmp(packet.second._method, LSSDP_NOTIFY) == 0)
                    {
                        ServiceUpdateEvent event;
                        event._event_id = ServiceUpdateEvent::notify_alive;
                        if (strcmp(packet.second._nts, LSSDP_NOTIFY_NTS_ALIVE) == 0)
                        {
                            event._event_id = ServiceUpdateEvent::notify_alive;
                        }
                        else if (strcmp(packet.second._nts, LSSDP_NOTIFY_NTS_BYEBYE) == 0)
                        {
                            event._event_id = ServiceUpdateEvent::notify_byebye;
                        }
                        event._service_description = ServiceDescription(packet.second._location,
                            packet.second._usn,
                            packet.second._st,
                            "",
                            "",
                            packet.second._sm_id,
                            packet.second._device_type);
                        update_callback(event);
                    }
                    else if (strcmp(packet.second._method, LSSDP_RESPONSE) == 0)
                    {
                        ServiceUpdateEvent event;
                        event._event_id = ServiceUpdateEvent::response;
                        event._service_description = ServiceDescription(packet.second._location,
                            packet.second._usn,
                            packet.second._st,
                            "",
                            "",
                            packet.second._sm_id,
                            packet.second._device_type);
                        update_callback(event);
                    }
                }
            }
        }
        
        //timeout check
        if ((std::chrono::system_clock::now() - begin_time) >= timeout)
        {
            go_ahead = false;
        }
        
    } while (go_ahead);
    return return_value;
}

std::string ServiceFinder::getLastSendErrors() const 
{
    return _impl->getSendErrors();
}

} //namespace lssdp

