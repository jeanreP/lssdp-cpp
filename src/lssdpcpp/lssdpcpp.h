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
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <iostream>

namespace lssdp
{

/**
 * @brief Default UPNP Address URL
 * 
 */
constexpr static const char* const LSSDP_DEFAULT_URL = "http://239.255.255.250:1900";

/**
 * @brief Convinience class for NetworkInterfaces 
 * @detail Usually this class must not be in the API,
 *         but it is helpful to test that, because it is internally used.
 * 
 */
class NetworkInterface
{
public:
    /**
     * @brief Default CTOR is deleted
     * 
     */
    NetworkInterface() = delete;
    /**
     * @brief CTOR
     * 
     * @param name name of the interface
     * @param ip4 ip of the interface as doted ip-string (xxx.xxx.xxx.xxx)
     * @param netmask_ip4 network mask of the interface as doted ip-string (xxx.xxx.xxx.xxx)
     */
    NetworkInterface(const std::string& name,
        const std::string& ip4,
        const std::string& netmask_ip4);
    /**
     * @brief CTOR
     * 
     * @param name name of the interface
     * @param addr_ip4 ip of the interface as 4Byte network value (socket_addr)
     * @param addr_netmask_ip4 network mask of the interface as 4Byte network value (socket_addr)
     */
    NetworkInterface(const std::string& name,
        uint32_t addr_ip4,
        uint32_t addr_netmask_ip4);

    /**
     * @brief DTOR
     * 
     */
    virtual ~NetworkInterface();
    /**
     * @brief Default move CTOR
     * 
     */
    NetworkInterface(NetworkInterface&&) = default;
    /**
     * @brief Default move operator
     * 
     * @return the reference to the moved 
     */
    NetworkInterface& operator=(NetworkInterface&&) = default;
    /**
     * @brief copy CTOR
     * 
     * @param other the network interface to copy
     */
    NetworkInterface(const NetworkInterface& other);
    /**
     * @brief copy operator
     * 
     * @param other the network interface to copy
     * @return the reference to the copied 
     */
    NetworkInterface& operator=(const NetworkInterface& other);

    /**
     * @brief equality operator
     * 
     * @param other the network interface to match
     * @retval true the addresses and names are equal
     * @retval fale the addresses and names are not equal
     */
    bool operator==(const NetworkInterface& other) const;

    /**
     * @brief Get the Name
     * 
     * @return the name
     */
    std::string getName() const;
    /**
     * @brief Get the Ip4
     * 
     * @return ip of the interface as doted ip-string (xxx.xxx.xxx.xxx)
     */
    std::string getIp4() const;
    /**
     * @brief Get the Net Mask Ip4
     * 
     * @return netmask ip of the interface as doted ip-string (xxx.xxx.xxx.xxx)
     */
    std::string getNetMaskIp4() const;
    /**
     * @brief Get the Addr Ip4
     * 
     * @return IP as 4Byte network value (socket_addr)
     */
    std::uint32_t getAddrIp4() const;
    /**
     * @brief Get the Addr Net Mask Ip4
     * 
     * @return Netmask IP as 4Byte network value (socket_addr)
     */
    std::uint32_t getAddrNetMaskIp4() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * @brief updates the vector of NetworkInterface s 
 * @detail Usually this class must not be in the API,
 *         but it is helpful to test that, because it is internally used.
 * 
 * @param interfaces the vector to update
 * @retval true the given interfaces has been changed
 * @return false the given interfaces has not been changed
 */
bool updateNetworkInterfaces(std::vector<NetworkInterface>& interfaces);

/**
 * @brief service description class
 *        which may contain all properties of a service
 * 
 */
class ServiceDescription
{
public:
    /**
     * @brief Default CTOR
     * 
     */
    ServiceDescription();
    /**
     * @brief CTOR
     * 
     * @param location_url Address of the service as well-formed URL where
     *                     the service is located. In UPnP this is a link to the service 
     *                     or device schema (see chapter 2.8, 2.9, 2.11).
     * @param unique_service_name Unique name of that service
     * @param search_target The notification type (NT) and search target (ST)
     * @param product_name Product name of the device (usually some vendor device name)
     * @param product_version Product version of the device (usually some vendor version)
     * @param sm_id Service id will be added to the messages as *SM_ID:*.
     * @param device_type Device type of the device will be added to the messages as *DEV_TYPE:*.
     */
    ServiceDescription(const std::string& location_url,
                       const std::string& unique_service_name,
                       const std::string& search_target,
                       const std::string& product_name,
                       const std::string& product_version,
                       const std::string& sm_id = std::string(),
                       const std::string& device_type = std::string());
    /**
     * @brief DTOR
     * 
     */
    virtual ~ServiceDescription();
    /**
     * @brief Default move CTOR
     * 
     */
    ServiceDescription(ServiceDescription&&) = default;
    /**
     * @brief Default move operator
     * 
     * @return the moved references
     */
    ServiceDescription& operator=(ServiceDescription&&) = default;
    /**
     * @brief Default copy CTOR
     * 
     * @param other the service description to copy
     */
    ServiceDescription(const ServiceDescription& other) = default;
    /**
     * @brief Default copy operator
     * 
     * @param other the service description to copy
     * @return the copied references
     */
    ServiceDescription& operator=(const ServiceDescription& other) = default;

    /**
     * @brief Convinience equality operator
     * 
     * @param other the service description to compare
     * @return true search_target and unique service name matches!
     * @return false search_target and unique service name is not equal!
     */
    bool operator==(const ServiceDescription& other) const;

    /**
     * @brief Get the Location URL
     * 
     * @return the location URL
     */
    std::string getLocationURL() const;
    /**
     * @brief Get the Unique Service Name
     * 
     * @return teh unique service name 
     */
    std::string getUniqueServiceName() const;
    /**
     * @brief Get the Search Target (or also called notification type)
     * 
     * @return the search target
     */
    std::string getSearchTarget() const;
    /**
     * @brief Get the "SM_ID"
     * 
     * @return the sm id
     */
    std::string getSMID() const;
    /**
     * @brief Get the Device Type 
     * 
     * @return the device type
     */
    std::string getDeviceType() const;
    /**
     * @brief Get the Product Name 
     * 
     * @return the product name
     */
    std::string getProductName() const;
    /**
     * @brief Get the Product Version 
     * 
     * @return the product version
     */
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

/**
 * Service class to setup a discoverable service.
 * 
 * 
 */
class Service
{
public:
    /**
     * Default CTOR is deleted
     * 
     */
    Service() = delete;
    /**
     * @brief CTOR
     * @detail The *Service* will immediately discover all lssdp::NetworkInterface s and 
     *         open the socket for the given *discover_url*.
     * 
     * @param discover_url well-formed URL with the multicast address and port.
     *                     Unfortunately, it is not yet checked wether you give a valid
     *                     multicast address or not. So beware of that! 
     * @param max_age The UPnP specification recommends a value greater than or equal to 1800 seconds.
     * @param location_url  Address of the service as well-formed URL where the service is located
     * @param unique_service_name Unique name of that service
     * @param search_target The notification type. The service will notify with this type and 
     *                      will only response to search requests containing this 
     *                      *search_target (ST)* or *ssdp:all*
     * @param product_name  Product name of the device (usually some vendor device name)
     * @param product_version Product version of the device (usually some vendor version)
     * @param sm_id Optional service id will be added to the messages as *SM_ID:*.
     * @param device_type Optional Device type of the device will be added to the messages as *DEV_TYPE:*.
     *                    This service will only response to search requests containing this device type.
     */
    Service(std::string discover_url,
            std::chrono::seconds max_age,
            std::string location_url,
            std::string unique_service_name,
            std::string search_target,
            std::string product_name,
            std::string product_version,
            std::string sm_id = std::string(),
            std::string device_type = std::string());
    /**
     * @brief DTOR
     * 
     */
    ~Service();
    /**
     * @brief Default move CTOR
     * 
     */
    Service(Service&&) = default;
    /**
     * @brief Default move operator
     * 
     * @return reference of this service
     */
    Service& operator=(Service&&) = default;
    /**
     * @brief Default copy CTOR is deleted
     * 
     */
    Service(const Service& other) = delete;
    /**
     * @brief Default copy operator is deleted
     * 
     * @return reference of this service
     */
    Service& operator=(const Service& other) = delete;

    /**
     * @brief Will send a *NOTIFY* message to all
     *        networks that the service is alive.
     *        The notify sub type is "ssdp:alive".
     * 
     * @remark The *NOTIFY* messages will NOT contain "UPnP/1.1" 
     *         as version within the *SERVER:* tag!
     * @retval true sent notify without errors
     * @retval false sent notify with errors, check with getLastSendErrors
     */
    bool sendNotifyAlive();
    /**
     * @brief Will send a *NOTIFY* message to all
     *        networks that the service is shuting down.
     *        The notify sub type is "ssdp:byebye".
     * 
     * @remark The *NOTIFY* messages will NOT contain "UPnP/1.1" 
     *         as version within the *SERVER:* tag!
     * @retval true sent notify without errors
     * @retval false sent notify with errors, check with getLastSendErrors
     */
    bool sendNotifyByeBye();
    /**
     * @brief check for a *M-SEARCH* messages and response if *search target (ST)* 
     *        and optionally *device type (DEV_TYPE)* matches
     * 
     * @param timeout Timeout in milliseconds (min wait time is 100 ms for now)
     * @retval true receiving from and responding to socket okay, timeout reached 
     * @retval false receiving from failed
     * 
     * @remark The *OK* messages will NOT contain "UPnP/1.1" 
     *         as version within the *SERVER:* tag!
     */
    bool checkForMSearchAndSendResponse(std::chrono::milliseconds timeout);

    /**
     * @brief Convinience equality operator
     * 
     * @param other the service description to compare
     * @retval true search_target and unique service name matches!
     * @retval false search_target and unique service name is not equal!
     */
    bool operator==(const ServiceDescription& other) const;

    /**
     * @brief Convinience to get the setup of this service
     * @return the current setup of this service
     */
    ServiceDescription getServiceDescription() const;

    /**
     * @brief Get send errors if search fails to one of the networkinterfaces
     *
     * @return the send errors
     */
    std::string getLastSendErrors() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/*********************************************************************************************************/
/**
 * ServiceFinder class to discover services. 
 * 
 * 
 */
class ServiceFinder
{
public:
    /**
     * @brief Service Update Event used within ServiceFinder::checkForServices
     * 
     */
    struct ServiceUpdateEvent
    {
        /**
         * @brief Event code
         * 
         */
        enum UpdateEvent
        {
            /**
             * *NOTIFIY* message with ssdp:alive received
             * 
             */
            notify_alive,
            /**
             * *NOTIFIY* message with ssdp:byebye received
             * 
             */
            notify_byebye,
            /**
             * response *OK* message was received
             * 
             */
            response
        };
        /**
         * @brief The service information within the message
         * @remark The service description will usually not always completed.
         *         Depending on the message type the content of the message is set.
         *         But at least the *search_target (ST)* and the *unique_service_name (USN)* is set.
         * 
         */
        ServiceDescription _service_description;
        /**
         * @brief the event id code
         * 
         */
        UpdateEvent        _event_id;
    };

public:
    /**
     * @brief Default CTOR is deleted
     * 
     */
    ServiceFinder() = delete;
    /**
     * @brief CTOR for the *lssdp::ServiceFinder* will immediately
     *        discover all *lssdp::NetworkInterface*s and 
     *        open the socket for the given *discovery_url*.
     *        This CTOR throws if something went wrong.
     * 
     * 
     * @param discover_url well-formed URL with the multicast address and port.
     *                     Unfortunately, it is not yet checked wether you give a
     *                     valid multicast address or not. So beware of that! 
     * @param product_name Product name of the device (usually some vendor device name)
     * @param product_version Product version of the device (usually some vendor version)
     * @param search_target Define the search target you are looking for. The ServiceFinder 
     *                      will react only on notifications and responses of this type and 
     *                      will send *M-SEARCH* messages only for this type.
     *                      If not set *ssdp:all* will be used for *M-SEARCH*.
     * @param device_type_filter Device type of the device will be added to the *M-SEARCH*
     *                           message as *DEV_TYPE:*. If setup the *ServiceFinder* will
     *                           only react to responses and notififactions containing this device type.
     */
    ServiceFinder(const std::string& discover_url,
        const std::string& product_name,
        const std::string& product_version,
        const std::string& search_target = std::string(),
        const std::string& device_type_filter = std::string());

    /**
     * @brief DTOR
     * 
     */
    virtual ~ServiceFinder();
    /**
     * @brief Default move CTOR
     * 
     */
    ServiceFinder(ServiceFinder&&) = default;
    /**
     * @brief Default move operator
     * 
     * @return reference to the moved object
     */
    ServiceFinder& operator=(ServiceFinder&&) = default;
    /**
     * @brief Default copy CTOR is deleted
     * 
     */
    ServiceFinder(const ServiceFinder&) = delete;
    /**
     * @brief Default copy operator is deleted
     * 
     * @return reference to the copied object
     */
    ServiceFinder& operator=(const ServiceFinder&) = delete;

    /**
     * @brief Sends a *M-SEARCH* message immediately to the opened socket.
     *        This *M-SEARCH* will contain *search_target (ST)* and/or *device_type (DEV_TYPE)*
     *        as defined in CTOR.
     * 
     * @remark The *M-SEARCH* messages will NOT contain "UPnP/1.1" 
     *         as version within the *USER-AGENT:* tag!
     * 
     * @retval true sent notify without errors
     * @retval false sent notify with errors, check with getLastSendErrors
     */
    bool sendMSearch();
    /**
     * @brief check for network interface changes explicitely.
     *        sendMSearch will usually check for that, but if you do not want to
     *        send search requests and only to react on notifications this method will help you.
     * 
     */
    void checkNetworkChanges();
    /**
     * @brief Check for service responses and notifications.
     *        Received and filtered responses and notifications will be 
     *        informed via ServiceUpdateEvent in @p update_callback
     * 
     * @param update_callback function to inform notification and response events to.
     * @param timeout overall timeout of this function (min timeout is 100 ms)
     * @retval true receiving from the socket okay, timeout reached 
     * @retval false receiving from failed
     * 
     */
    bool checkForServices(const std::function<void(const ServiceUpdateEvent&)>& update_callback,
                          std::chrono::milliseconds timeout);

    /**
     * @brief Get the discovery Url 
     * 
     * @return the discovery Url 
     */
    std::string getUrl() const;

    /**
     * @brief Get send errors if search fails to one of the networkinterfaces
     *
     * @return the send errors
     */
    std::string getLastSendErrors() const;

private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

} //namespace lssdp

/**
 * @brief Convienience streaming operator for ServiceDescription
 * 
 * @param stream where to stream the service description to
 * @param desc the service description
 * @return the stream 
 */
inline std::ostream& operator <<(std::ostream& stream, const lssdp::ServiceDescription& desc)
{
    stream << std::string("USN: ") << desc.getUniqueServiceName() << std::endl;
    stream << std::string("ST:") << desc.getSearchTarget() << std::endl;
    stream << std::string("DEV_TYPE:") << desc.getDeviceType() << std::endl;
    stream << std::string("LOCATION:") << desc.getLocationURL() << std::endl;
    stream << std::string("PRODUCT:") << desc.getProductName() << "/" << desc.getProductVersion() << std::endl;

    return stream;
}

/**
 * @brief Convienience streaming operator for ServiceUpdateEvent
 * 
 * @param stream where to stream the ServiceUpdateEvent to
 * @param event the ServiceUpdateEvent
 * @return the stream 
 */
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

