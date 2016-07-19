#include <iostream>
#include "creeper.h"

using namespace std;

string getRamUsage() {
    nlohmann::json data = creeper::call("os/getram");
    float used = data.at("used").get<float>();
    float total = data.at("free").get<float>() + used;
    return to_string((int)floor(used / total * 100)) + "%";
}

void callback(nlohmann::json data) {
    cout << data << endl;
}

int main(int argc, char *argv[]) {
    cout << "Loading CreeperDisco" << endl;

    if (argc < 3) {
        cout << "Invalid number of arguments!";
        return 1;
    }
    creeper::key = argv[1];
    creeper::secret = argv[2];

    // todo Test details are valid
    //if (creeper::call()

    // OOP style commands
    creeper::commands["getram"] = new creeper::Command("getram", "os/getram");

    // using function pointers for commands. Good for simple replies but might not work well for larger ones.
    typedef string(*Cmd)(vector<string>);
    map<string, Cmd> cmds;
    cmds["getcpu"] = [](vector<string> args)->string{ return creeper::call("os/getcpu").dump();};

    do {
        string in;
        cout << "> ";
        getline(cin, in);
        if (in.find("quit") != string::npos) break;
        try {
            if (cmds.find(in) != cmds.end()) {
                cout << cmds[in]({}) << endl;
            }
            else if (creeper::commands.find(in) != creeper::commands.end()) {
                cout << creeper::commands[in]->call() << endl;
            }
            else cout << creeper::call(in) << endl;
        }
        catch (creeper::CreeperException e) {
            cout << e.what() << endl;
        }
        catch (curlpp::LogicError &e) {
            cout << e.what() << endl;
        }
        catch (curlpp::RuntimeError &e) {
            cout << e.what() << endl;
        }
    } while (true);

    cout << "Exiting CreeperDisco" << endl;
    return 0;
}