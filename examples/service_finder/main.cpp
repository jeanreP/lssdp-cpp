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
#include <future>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[])
{
    std::cout << std::string("****************************************************************") << std::endl;
    std::cout << std::string("* Welcome to the lssdpcpp service finder example                ") << std::endl;
    std::cout << std::string("* ServiceFinder will be setup                                   ") << std::endl;
    std::cout << std::string("****************************************************************") << std::endl;

    lssdp::ServiceFinder my_finder("http://239.255.255.250:1900", //discovery_url
        "MyProductName", //product_name
        "1.1", //product_version
        ""); //search_target this will search now for ssdp::all 

    //print information to cout
    std::cout << std::string("Will discover on URL: ") << my_finder.getUrl() << std::endl;

    //set up a thread loop
    std::atomic<bool> keep_running;
    keep_running = true;
    std::promise<bool> loop_ready;
    std::future<bool> is_loop_ready = loop_ready.get_future();

    std::thread(
        [&]
    {
        try
        {
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
                    std::cout << std::string("Send MSearch") << std::endl;
                    std::cout << std::string("service_command>");
                    my_finder.sendMSearch();
                }
                // this will return on timeout or on socket closure
                my_finder.checkForServices(
                    [&](const lssdp::ServiceFinder::ServiceUpdateEvent& update_event)
                    {
                        std::cout << std::string("Received ServiceUpdateEvent:") << std::endl;
                        std::cout << update_event << std::endl << std::endl;
                        std::cout << std::string("service_command>");
                    },
                    std::chrono::seconds(1));

                // somebody needs to set keep_running to false to end loop!

            } while (keep_running);

            loop_ready.set_value(true);
        }
        catch (const std::exception& e)
        {
            std::cout << std::string("exeption occured") << std::endl;
            std::cout << std::string(e.what()) << std::endl;

            loop_ready.set_value(false);
        }
    }
    ).detach();

    std::cout << std::string("service_command>");
    std::string line;
    while (!std::cin.eof())
    {
        std::getline(std::cin, line);
        if (line == "exit"
            || line == "quit"
            || line == "q"
            || line == "e")
        {
            keep_running = false;
            std::cout << std::string("byebye") << std::endl;
            break;
        }
        else
        {
            std::cout << std::string("only exit (e), quit (q) is supported") << std::endl;
            std::cout << std::string("service_command>");
        }
    }

    is_loop_ready.wait();
    if (is_loop_ready.get())
    {
        return 0;
    }
    else
    {
        return -1;
    }
}