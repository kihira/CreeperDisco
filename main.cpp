#include <iostream>
#include <thread>
#include "creeper.h"
#include "discord.h"

using namespace std;

void callback(nlohmann::json data) {
    cout << data << endl;
}

int main(int argc, char *argv[]) {
    cout << "Loading CreeperDisco" << endl;

    if (argc < 4) {
        cout << "Invalid number of arguments!";
        return 1;
    }
    creeper::login = creeper::KeySecretPair(argv[1], argv[2]);
    discord::token = argv[3];

    creeper::commands["getcpu"] = [](vector<string> args)->string{ return creeper::format(creeper::call("os/getcpu"), "Free: $free$, Used: $used$");};
    creeper::commands["getram"] = [](vector<string> args)->string{ return creeper::format(creeper::call("os/getram"), "Free: $free$, Used: $used$");};
    creeper::commands["gethdd"] = [](vector<string> args)->string{ return creeper::format(creeper::call("os/gethdd"), "Free: $free$, Used: $used$");};
    creeper::commands["stats"] = [](vector<string> args)->string{
        stringstream output;
        output << "CPU" << endl << creeper::commands["getcpu"]({}) << endl << endl
        << "RAM" << endl << creeper::commands["getram"]({}) << endl << endl
        << "HDD" << endl << creeper::commands["gethdd"]({});
        return output.str();
    };

    boost::asio::io_service asio_service;

    // Initialise Discord client
    discord::Client client(asio_service);

    // Initialise alert monitoring
    boost::asio::steady_timer alert_timer(asio_service);
    creeper::alert(&alert_timer, 30000);

    // Create threads and run ASIO loop
    boost::asio::io_service::work work(asio_service); // Need to start work before creating threads otherwise they will terminate immediately
    thread t1 ([&asio_service](){asio_service.run();});
    thread t2 ([&asio_service](){asio_service.run();});
    thread t3 ([&asio_service](){asio_service.run();});

    // Retrieve what we can access (Currently not working)
//    nlohmann::json data;
//    data["key"] = creeper::login.first;
//    cout << creeper::call("accesscontrol/list", data) << endl;

    while (true) {
        string in;
        getline(cin, in);
        if (in.find("quit") != string::npos) {
            break;
        }
        try {
            if (in.length() == 0) continue;
            if (creeper::commands.find(in) != creeper::commands.end()) {
                cout << creeper::commands[in]({}) << endl;
            }
            else cout << creeper::call(in) << endl;
        }
        catch (creeper::CreeperException e) {
            cerr << e.what() << endl;
        }
        catch (curlpp::LogicError &e) {
            cerr << e.what() << endl;
        }
        catch (curlpp::RuntimeError &e) {
            cerr << e.what() << endl;
        }
    }

    //client.disconnect();
    asio_service.stop();

    t1.join();
    t2.join();
    t3.join();

    cout << "Exiting CreeperDisco" << endl;
    return 0;
}