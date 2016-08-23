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
    // todo testing/manual reg
    discord::discordCHMap[stoull(argv[4])] = new creeper::Server("Test", argv[1], argv[2]);
    discord::token = argv[3];

    // todo need to think whether to stay with function pointers or switch back to oop
    creeper::commands["getcpu"] = [](creeper::KeySecretPair& login, vector<string> args)->string{
        nlohmann::json data = creeper::call(login, "os/getcpu");
        return "Usage: " + to_string(round((100 - data["free"].get<float>())*100)) + "%"; // Use free as "used" returns a string
    };
    creeper::commands["getram"] = [](creeper::KeySecretPair& login, vector<string> args)->string{ return creeper::format(
            creeper::call(login, "os/getram"), "Free: $free$mb, Used: $used$mb");};
    creeper::commands["gethdd"] = [](creeper::KeySecretPair& login, vector<string> args)->string{ return creeper::format(
            creeper::call(login, "os/gethdd"), "Free: $free$, Used: $used$");};
    creeper::commands["stats"] = [](creeper::KeySecretPair& login, vector<string> args)->string{
        stringstream output;
        output << "CPU" << endl << creeper::commands["getcpu"](login, {}) << endl << endl
        << "RAM" << endl << creeper::commands["getram"](login, {}) << endl << endl
        << "HDD" << endl << creeper::commands["gethdd"](login, {});
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
    }

    client.disconnect();
    asio_service.reset();

    t1.join();
    t2.join();
    t3.join();

    cout << "Exiting CreeperDisco" << endl;
    return 0;
}