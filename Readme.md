# lssdp-cpp
light weight SSDP library for C++

## What is SSDP

The Simple Service Discovery Protocol (SSDP) is a network protocol based on the Internet Protocol Suite for advertisement and discovery of network services and presence information.

This implementation will follow chapter 1 of the specification http://www.upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.1.pdf,
but is no fully UPnP/1.1 implementation.

### Supported SDDP messages

#### Service

* sending *NOTIFY ssdp:alive*
* sending *NOTIFY ssdp:byebye*
* receiving *M-SEARCH*
* sending search response *OK*

#### ServiceFinder

* receiving *NOTIFY ssdp:alive*
* receiving *NOTIFY ssdp:byebye*
* sending *M-SEARCH*
* receiving search response *OK*

_____________________________________________________

## Tested Platforms

* Linux Ubuntu 16.04 with gcc 5.4
* Windows 10 x64 with VC++ 2019

_____________________________________________________

## How to build and test

### Using CMake for Linux

    mkdir build 
    cd build
    cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALL_PREFIX=./../package --G "Unix Makefile" ./.. 
    make install

If the cmake variable *lssdpcpp_enable_tests* is ON the 

### Using CMake for Windows

    mkdir build 
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=./../package/ -G "Visual Studio 15 2017 Win64" ./source
    devenv ./lssdpcpp-project.sln

## How to use lssdp-cpp

### Using as library

The cmake build will install a target under the given *CMAKE_INSTALL_PREFIX*. This static target library is ready to use in other project by linking against it.

    find_package(lssdpcpp REQUIRED)
    #... your cmake stuff and targets
    target_link_libraries(your_target PRIVATE lssdpcpp)


Within code you simply add the include:

    #include <lssdpcpp/lssdpcpp.h>

### Using as code

The *lssdp-cpp* library code is designed to be built into your own project. 
You simply need to copy and add following files and folders: 

* *lsspdpcpp/lsspdpcpp.h*
* *lsspdpcpp/lsspdpcpp.cpp*
* *url/url.hpp*
* *url/url.cpp*

I did not want to merge the URL parser code into the *lsspdpcpp* files by purpose. Feel free to do so.

Additionally your target will depend on following system libraries: 

* Windows: *Ws2_32.lib Iphlpapi.lib*
* Linux: no additional dependency

_____________________________________________________

## lssdp-cpp API 

### *Service* daemon setup

If you want to provide a service for discovery you will use the *lssdp::Service* class.

#### 1. setup the service and its properties

The *lssdp::Service* will immediately discover all *lssdp::NetworkInterface*s and open the socket for the given *discovery_url*.
This CTOR throws if something went wrong.

    lssdp::Service my_service("http://239.255.255.250:1900", //discovery_url
        std::chrono::seconds(1800), //max_age
        "http://192.168.1.34:9092", //location_url
        "service_uid_1", //unique_service_identifier
        "my_search_target", //search_target
        "MyProductName", //product_name
        "1.1");  //product_version

* *discovery_url:* This must be a well-formed URL with the multicast address and port. Unfortunately, it is not yet checked wether you give a valid multicast address or not. So beware of that! 
* *max_age (in seconds):* The specification recommends a value greater than or equal to 1800 seconds
* *location_url:* Address of the service as well-formed URL where the service is located. In UPnP this is a link to the service or device schema (see chapter 2.8, 2.9, 2.11).
* *unique_service_identifier:* Unique name of that service
* *search_target:* This will define the notification type. The service will notify with this type and will only response to search requests containing this *search_target (ST)* or *ssdp:all*
* *product_name:* Product name of the device (usually some vendor device name)
* *product_version:* Product version of the device (usually some vendor version)

##### Remark

The *NOTIFY* messages will NOT contain "UPnP/1.1" as version within the *SERVER:* tag!

Additionally, this implementation will also contain some two optional fields:
* *sm_id* Service id will be added to the messages as *SM_ID:*.
* *device_type* Device type of the device will be added to the messages as *DEV_TYPE:*. This service will only response to search requests containing this device type.

#### 2. start notifying and responding

The *lssdp::Service* class is a lock-free and protocol messages only implementation. So it is *NOT* thread-safe and you need to take care of the control flow by yourself.
To do so you may use 3 functions: 
* *void sendNotifyAlive()* : Will send a *NOTIFY* message to all networks that the service is alive
* *void sendNotifyByeBye()* : Will send a *NOTIFY* message to all networks that the service ends now
* *bool checkForMSearchAndSendResponse(timeout)* : Will check for a *M-SEARCH* messages and response if *search target (ST)* and optionally *device type (DEV_TYPE)* matches


Code Example:

    //...
    std::chrono::seconds send_alive_interval = std::chrono::seconds(5);
    auto last_time = std::chrono::system_clock::now();

    // send notify first
    my_service.sendNotifyAlive();

    std::chrono::seconds send_alive_interval = std::chrono::seconds(5);
    auto last_time = std::chrono::system_clock::now();

    // send notify first
    my_service.sendNotifyAlive();

    do
    {
        auto now = std::chrono::system_clock::now();
        if ((now - last_time) >= send_alive_interval)
        {
            last_time = now;
            my_service.sendNotifyAlive();
        }
        // this will return on timeout or on socket closure
        my_service.checkForMSearchAndSendResponse(std::chrono::seconds(1));

        // somebody needs to set keep_running to false to end loop!

    } while (keep_running);

    // send notify to say good bye
    my_service.sendNotifyByeBye();

### Find services with the *ServiceFinder*

If you want to find a services on a certain discovery url you will use the *lssdp::ServiceFinder* class.

#### 1. setup the service finder and search filter 

The *lssdp::ServiceFinder* will immediately discover all *lssdp::NetworkInterface*s and open the socket for the given *discovery_url*.
This CTOR throws if something went wrong.

You may setup a clear *search_target (ST)* and/or a *device_type (DEV_TYPE)* to filter the services responding and notifying on the given *discovery_url*.
If you do not setup any filter, each kind of service is discovered and updated via *checkForServices()* and its *update_callback*.

     lssdp::ServiceFinder my_finder("http://239.255.255.250:1900", //discovery_url
        "MyProductName", //product_name
        "1.1", //product_version
        ""); //search_target

* *discovery_url:* This must be a well-formed URL with the multicast address and port. Unfortunately, it is not yet checked wether you give a valid multicast address or not. So beware of that!
* *product_name:* Product name of the device (usually some vendor device name)
* *product_version:* Product version of the device (usually some vendor version)
* *search_target:* Define the search target you are looking for. The *ServiceFinder* will react only on notifications and responses of this type and will send *M-SEARCH* messages only for this type. If not set *ssdp:all* will be used for *M-SEARCH*.


Additionally, this implementation will also contain some a optional fields:
* *device_type* Device type of the device will be added to the *M-SEARCH* message as *DEV_TYPE:*. If setup the *ServiceFinder* will only react to responses and notififactions containing this device type.

#### 2. start search and check for responding and notifications

The *lssdp::ServiceFinder* class is like the *lssdp::Service* class a lock-free and protocol messages only implementation. So it is NOT thread-safe and you need to take care of the control flow by yourself. To do so you may use 3 functions:

* *void sendMSearch()* : Sends a *M-SEARCH* message immediately to the opened socket. This *M-SEARCH* will contain *search_target (ST)* and/or *device_type (DEV_TYPE)*.
* *void checkNetworkChanges()* : Check for network interface changes explicitely. You do not need to call this if you call sendMSearch frequentely.
* *bool checkForServices(update_callback, timeout)* : Check for service responses and notifications. Received and filtered responses and notifications will be informed via ServiceUpdateEvent in *update_callback* function. 

Following Events are possible for the *update_callback*: 

* *ServiceUpdateEvent::notify_alive* : *NOTIFIY* message with ssdp:alive received.
* *ServiceUpdateEvent::notify_byebye* : *NOTIFIY* message with ssdp:byebye received. 
* *ServiceUpdateEvent::response* : response *OK* message was received. 

Code Example:

    //...
    std::chrono::seconds send_msearch_interval = std::chrono::seconds(5);
    auto last_time = std::chrono::system_clock::now();

    // send search first
    my_finder.sendMSearch();

    do
    {
        auto now = std::chrono::system_clock::now();
        if ((now - last_time) >= send_msearch_interval)
        {
            last_time = now;
            std::cout << std::string("Send M-SEARCH") << std::endl;

            my_finder.sendMSearch();
        }
        // this will return on timeout or on socket closure
        my_finder.checkForServices(
            [&](const lssdp::ServiceFinder::ServiceUpdateEvent& update_event)
            {
                std::cout << std::string("Received ServiceUpdateEvent:") << std::endl;
                std::cout << update_event << std::endl << std::endl;
            },
            std::chrono::seconds(1));

        // somebody needs to set keep_running to false to end loop!

    } while (keep_running);


### Helper classes *ServiceDescription* and *NetworkInterface*

* *lssdp::NetworkInterface*: Convinience class for discovery of NetworkInterfaces with *lssdp::updateNetworkInterfaces()*. Usually this class must not be in the API, but it is helpful to test that, because it is internally used.
* *lssdp::ServiceDescription*: service description class which may contain all properties of a service

_____________________________________________________

## LICENSE

### lssdp-cpp 

This source code ist under MIT License.

### Used Code

* from https://github.com/jeanreP/lssdp which is under MIT License
* from https://github.com/chmike/CxxUrl which is under MIT License (license added)



