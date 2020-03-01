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
#define CATCH_CONFIG_MAIN
#include "./../../catch/catch.hpp"

#include <lssdpcpp/lssdpcpp.h>

struct EventCounter
{
    void countFor(const lssdp::Service& service,
                  const std::vector<lssdp::ServiceFinder::ServiceUpdateEvent>& update_events)
    {
        _count_alive = 0;
        _count_byebye = 0;
        _count_response = 0;
        for (const auto& current_event : update_events)
        {
            if (service == current_event._service_description)
            {
                if (current_event._event_id == lssdp::ServiceFinder::ServiceUpdateEvent::notify_alive)
                {
                    ++_count_alive;
                }
                else if (current_event._event_id == lssdp::ServiceFinder::ServiceUpdateEvent::notify_byebye)
                {
                    ++_count_byebye;
                }
                else if (current_event._event_id == lssdp::ServiceFinder::ServiceUpdateEvent::response)
                {
                    ++_count_response;
                }
            }
        }
    }
    int _count_alive;
    int _count_byebye;
    int _count_response;
};


TEST_CASE("TestServiceFinder", "checkForServices")
{
    using namespace lssdp;
    SECTION("create ServiceFinder default")
    {
        std::vector<ServiceFinder::ServiceUpdateEvent> service_events;
        using namespace std::chrono;
        Service service1(lssdp::LSSDP_DEFAULT_URL,
                         seconds(1800),
                         "http://localhost::9090",
                         "service1",
                         "my_search_target",
                         "MyTest",
                         "1.1");
        Service service2(lssdp::LSSDP_DEFAULT_URL,
                         seconds(1800),
                         "http://localhost::9090",
                         "service2",
                         "my_search_target",
                         "MyTest",
                         "1.1");
        ServiceFinder finder(lssdp::LSSDP_DEFAULT_URL, "MyTest", "1.1");
        //we create the service findr and will run 30 seconds
        auto first_time = system_clock::now();
        auto last_time = first_time;
        time_point<system_clock> last_time_for_m_search;
        while (last_time - first_time < seconds(30))
        {
            if (last_time_for_m_search == time_point<system_clock>()
                || (last_time - last_time_for_m_search) > seconds(5))
            {
                last_time_for_m_search = system_clock::now();

                //update every 5 seconds
                std::cout << "send m search" << std::endl;
                REQUIRE_NOTHROW(finder.sendMSearch());
                std::cout << "send service 1 alive" << std::endl;
                REQUIRE_NOTHROW(service1.sendNotifyAlive());
                std::cout << "send service 2 alive" << std::endl;
                REQUIRE_NOTHROW(service2.sendNotifyAlive());
            }
            auto did_throw = false;
            try
            {
                std::cout << "look for responding 1" << std::endl;
                auto ret_check_msearch1 = service1.checkForMSearchAndSendResponse(seconds(1));
                std::cout << "look for responding 2" << std::endl;
                auto ret_check_msearch2 = service2.checkForMSearchAndSendResponse(seconds(1));
                std::cout << "check for services" << std::endl;
                auto ret_check_finder  = finder.checkForServices(
                    [&](const ServiceFinder::ServiceUpdateEvent& update_event)
                    {
                    service_events.push_back(update_event);
                        std::cout << update_event << std::endl;
                    },
                    seconds(1));
            }
            catch (std::runtime_error&)
            {
                std::cout << "did throw" << std::endl;
                did_throw = true;
            }
            REQUIRE_FALSE(did_throw);
            last_time = system_clock::now();
        }
        REQUIRE_NOTHROW(service1.sendNotifyByeBye());
        REQUIRE_NOTHROW(service2.sendNotifyByeBye());

        auto ret_check_finder = finder.checkForServices(
            [&](const ServiceFinder::ServiceUpdateEvent& update_event)
            {
                service_events.push_back(update_event);
                std::cout << update_event << std::endl;
            },
            seconds(5));
        
        EventCounter service1_counter;
        service1_counter.countFor(service1, service_events);

        REQUIRE(service1_counter._count_alive > 0);
        REQUIRE(service1_counter._count_byebye > 0);
        REQUIRE(service1_counter._count_response > 0);

        EventCounter service2_counter;
        service2_counter.countFor(service2, service_events);

        REQUIRE(service2_counter._count_alive > 0);
        REQUIRE(service2_counter._count_byebye > 0);
        REQUIRE(service2_counter._count_response > 0);

    }
}
   
