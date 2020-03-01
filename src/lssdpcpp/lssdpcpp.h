/******************************************************************************************
*
*  Copyright 2020 Pierre Voigtl�nder(jeanreP)
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
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <iostream>

namespace lssdp
{

constexpr static const char* const LSSDP_DEFAULT_URL = "http://239.255.255.250:1900";

/*********************************************************************************************************/
class NetworkInterface
{
public:
    NetworkInterface() = delete;
    NetworkInterface(const std::string& name,
        const std::string& ip4,
        const std::string& netmask_ip4);
    NetworkInterface(const std::string& name,
        uint32_t addr_ip4,
        uint32_t addr_netmask_ip4);

    virtual ~NetworkInterface();
    NetworkInterface(NetworkInterface&&) = default;
    NetworkInterface& operator=(NetworkInterface&&) = default;
    NetworkInterface(const NetworkInterface& other);
    NetworkInterface& operator=(const NetworkInterface& other);

    bool operator==(const NetworkInterface& other) const;

    std::string getName() const;
    std::string getIp4() const;
    std::string getNetMaskIp4() const;
    std::uint32_t getAddrIp4() const;
    std::uint32_t getAddrNetMaskIp4() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief 
 * 
 * @param interfaces 
 * @return true 
 * @return false 
 */
bool updateNetworkInterfaces(std::vector<NetworkInterface>& interfaces);

/*********************************************************************************************************/
class ServiceDescription
{
public:
    ServiceDescription();
    ServiceDescription(const std::string& location_url,
                       const std::string& unique_service_name,
                       const std::string& search_target,
                       const std::string& product_name,
                       const std::string& product_version,
                       const std::string& sm_id = std::string(),
                       const std::string& device_type = std::string());
    
    virtual ~ServiceDescription();
    ServiceDescription(ServiceDescription&&) = default;
    ServiceDescription& operator=(ServiceDescription&&) = default;
    ServiceDescription(const ServiceDescription& other) = default;
    ServiceDescription& operator=(const ServiceDescription& other) = default;

    bool operator==(const ServiceDescription& other) const;

    std::string getLocationURL() const;
    std::string getUniqueServiceName() const;
    std::string getSearchTarget() const;
    std::string getSMID() const;
    std::string getDeviceType() const;
    std::string getProductName() const;
    std::string getProductVersion() const;

private:
    std::string _location_url;
    std::string _unique_service_name;
    std::string _search_target;
    std::string _sm_id;
    std::string _device_type;
    std::string _product_name;
    std::string _product_version;
};

/*********************************************************************************************************/
class Service
{
public:
    Service() = delete;
    Service(std::string discover_url,
            std::chrono::seconds max_age,
            std::string location_url,
            std::string unique_service_name,
            std::string search_target,
            std::string product_name,
            std::string product_version,
            std::string sm_id = std::string(),
            std::string device_type = std::string());
    ~Service();
    Service(Service&&) = default;
    Service& operator=(Service&&) = default;
    Service(const Service& other) = delete;
    Service& operator=(const Service& other) = delete;

    void sendNotifyAlive();
    void sendNotifyByeBye();
    bool checkForMSearchAndSendResponse(std::chrono::milliseconds timeout);

    bool operator==(const ServiceDescription& other) const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/*********************************************************************************************************/
/**
 * ServiceFinder 
 * 
 * 
 */
class ServiceFinder
{
public:
    struct ServiceUpdateEvent
    {
        enum UpdateEvent
        {
            notify_alive,
            notify_byebye,
            response
        };
        ServiceDescription _service_description;
        UpdateEvent        _event_id;
    };

public:
    ServiceFinder() = delete;
    ServiceFinder(const std::string& discover_url,
        const std::string& product_name,
        const std::string& product_version,
        const std::string& search_target = std::string(),
        const std::string& device_type_filter = std::string());

    virtual ~ServiceFinder();
    ServiceFinder(ServiceFinder&&) = default;
    ServiceFinder& operator=(ServiceFinder&&) = default;
    ServiceFinder(const ServiceFinder& other) = delete;
    ServiceFinder& operator=(const ServiceFinder& other) = delete;

    void sendMSearch();
    void checkNetworkChanges();
    bool checkForServices(const std::function<void(const ServiceUpdateEvent& update_service)>& update_callback,
                          std::chrono::milliseconds timeout);

    std::string getUrl() const;

private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

} //namespace lssdp

inline std::ostream& operator <<(std::ostream& stream, const lssdp::ServiceDescription& desc)
{
    stream << std::string("USN: ") << desc.getUniqueServiceName() << std::endl;
    stream << std::string("ST:") << desc.getSearchTarget() << std::endl;
    stream << std::string("DEV_TYPE:") << desc.getDeviceType() << std::endl;
    stream << std::string("LOCATION:") << desc.getLocationURL() << std::endl;
    stream << std::string("PRODUCT:") << desc.getProductName() << "/" << desc.getProductVersion() << std::endl;

    return stream;
}

inline std::ostream& operator <<(std::ostream& stream, const lssdp::ServiceFinder::ServiceUpdateEvent& event)
{
    if (event._event_id == event.notify_alive)
    {
        stream << std::string("notify_alive ");
    } 
    else if (event._event_id == event.notify_byebye)
    {
        stream << std::string("notify_byebye ");
    }
    else if (event._event_id == event.response)
    {
        stream << std::string("response OK ");
    }
    stream << event._service_description;
    
    return stream;
}

