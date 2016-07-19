#ifndef DISCORDBOT_CREEPER_H
#define DISCORDBOT_CREEPER_H

#include <string>
#include <fstream>
#include "lib/json/src/json.hpp"
#include "lib/curlpp/include/curlpp/cURLpp.hpp"
#include "lib/curlpp/include/curlpp/Easy.hpp"
#include "lib/curlpp/include/curlpp/Options.hpp"

namespace creeper {
    using namespace std;
    using namespace curlpp::Options;
    using json = nlohmann::json;

    const string API_URL = "https://api.creeper.host/";
    string key;
    string secret;

    struct CreeperException : public exception {
        virtual const char *what() throw() {
            return msg.c_str();
        }

    private:
        string msg;
    public:
        CreeperException(string msg) {
            this->msg = msg;
        }
    };

    inline json call(string endpoint, json data = {}, function<void(json)> callback = NULL) {
        try {
            stringstream outstream;

            curlpp::Cleanup cleanup; // Clean up used resources
            curlpp::Easy request;
            curlpp::Forms form;
            form.push_back(new curlpp::FormParts::Content("key", key));
            form.push_back(new curlpp::FormParts::Content("secret", secret));
            form.push_back(new curlpp::FormParts::Content("data", data.dump()));

            request.setOpt<WriteStream>(&outstream);
            request.setOpt<Url>(API_URL + endpoint);
            request.setOpt<HttpPost>(form);

            request.perform();

            json returned = json::parse(outstream.str());
            if (returned.at("status").get<string>() == "error") {
                throw CreeperException(returned.at("message").get<string>());
            }
            if (callback != NULL) callback(returned);
            return returned;
        }
        catch (curlpp::LogicError &e) {
            cout << e.what() << endl;
        }
        catch (curlpp::RuntimeError &e) {
            cout << e.what() << endl;
        }
        return {};
    }

    class Command {
    private:
        string cmd;
        string endpoint;
        vector<string> argNames;
    public:
        Command(string cmd, string endpoint, vector<string> args = {}) {
            this->cmd = cmd;
            this->endpoint = endpoint;
            this->argNames = args;
        }
        json call(vector<string>args = {}) {
            json data;
            if (argNames.size() > 0) {
                if (args.size() != argNames.size()) throw CreeperException("Invalid number of parameters for command!");

                for (int i = 0; i < argNames.size(); ++i) {
                    data[argNames[i]] = args[i];
                }
            }

            return creeper::call(endpoint, data);
        }
    };

    map<string, Command*> commands;
}

#endif //DISCORDBOT_CREEPER_H