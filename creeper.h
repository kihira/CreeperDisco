#ifndef DISCORDBOT_CREEPER_H
#define DISCORDBOT_CREEPER_H

#include <string>
#include <fstream>
#include <unistd.h>
#include <boost/asio/steady_timer.hpp>
#include "lib/json/src/json.hpp"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"
#include "curlpp/Infos.hpp"
#include <boost/log/trivial.hpp>

namespace creeper {
    using namespace std;
    using namespace curlpp::Options;
    using json = nlohmann::json;

    typedef pair<string, string> KeySecretPair;

    const string API_URL = "https://api.creeper.host/";
    KeySecretPair login;

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
        stringstream outstream;

        curlpp::Cleanup cleanup; // Clean up used resources
        curlpp::Easy request;
        curlpp::Forms form;
        form.push_back(new curlpp::FormParts::Content("key", login.first));
        form.push_back(new curlpp::FormParts::Content("secret", login.second));
        form.push_back(new curlpp::FormParts::Content("data", data.dump()));

        request.setOpt<WriteStream>(&outstream);
        request.setOpt<Url>(API_URL + endpoint);
        request.setOpt<HttpPost>(form);
        request.setOpt<Timeout>(20);
        //request.setOpt<HttpVersion>(CURL_HTTP_VERSION_2_0); Need to manually compile curl with it enabled https://serversforhackers.com/video/curl-with-http2-support

        chrono::time_point<chrono::system_clock> start = chrono::system_clock::now();
        request.perform();
        BOOST_LOG_TRIVIAL(info) << "Endpoint call (" << endpoint << ") took " << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start).count() << " milliseconds" << endl;

        json returned = json::parse(outstream.str());
        long response = curlpp::infos::ResponseCode::get(request);
        if (response != 200l) {
            throw CreeperException("Server returned Response Code " + to_string(response));
        }
        else if (returned.empty()){
            throw CreeperException("Server returned empty response");
        }
        else if (returned["status"].get<string>() == "error") {
            throw CreeperException(returned["message"].get<string>());
        }
        if (callback != NULL) callback(returned);
        return returned;
    }

    void alert(boost::asio::steady_timer* timer, int interval) {
        chrono::time_point<chrono::system_clock> start = chrono::system_clock::now();
        json data = call("api/alerts");
        if (!data.empty()) {
            for (int i = 0; i < data["alerts"].size(); ++i) {
                BOOST_LOG_TRIVIAL(info) << ((json)data["alerts"][i])["notes"].get<string>() << endl;
            }
        }

        timer->expires_from_now(chrono::milliseconds(interval));
        timer->async_wait(bind(&creeper::alert, timer, interval));
        BOOST_LOG_TRIVIAL(info) << "Alert loop took " << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start).count() << " milliseconds" << endl;
    }

    inline string replace(string& input, const string& search, const string& replace) {
        size_t f = input.find(search);
        if (f != string::npos) input.replace(f, search.length(), replace);
        return input;
    }

    string format(json data, string returnString) {
        string formattedString = returnString;
        string key = "";
        string value = "";
        for (json::iterator it = data.begin(); it != data.end(); ++it) {
            if (it.value().is_object()) continue; // ignore objects for now
            key = "$"+it.key()+"$";
            value = it.value().dump();
            replace(formattedString, key, value);
        }
        return formattedString;
    }

    class Command {
    public:
        Command(const string cmd, const string endpoint, vector<string> args = {}) {
            this->endpoint = endpoint;
            this->argNames = args;
        }
        virtual string run(vector<string>args = {}) {
            return call(args).dump();
        }
    private:
        string endpoint;
        vector<string> argNames;
    protected:
        json call(vector<string>args) {
            json data;
            if (argNames.size() > 0) {
                if (args.size() != argNames.size()) throw invalid_argument("Invalid number of parameters for command!");

                for (int i = 0; i < argNames.size(); ++i) {
                    data[argNames[i]] = args[i];
                }
            }

            try {
                return creeper::call(endpoint, data);
            }
            catch (CreeperException e) {
                return e.what();
            }
        }
    };

    class FormattedCommand : public Command {
    public:
        FormattedCommand(const string &cmd, const string &endpoint, const vector<string> &args, const string returnString) : Command(cmd, endpoint, args) {
            this->returnString = returnString;
        }
        string run(vector<string>args = {}) {
            json data;
            try {
                data = call(args);
            }
            catch (CreeperException &e) {
                return e.what();
            }

            string formattedString = returnString;
            string key = "";
            string value = "";
            for (json::iterator it = data.begin(); it != data.end(); ++it) {
                if (it.value().is_object()) continue; // ignore objects for now
                key = "$"+it.key()+"$";
                value = it.value().dump();
                replace(formattedString, key, value);
            }
            return formattedString;
        }
    private:
        string returnString;
    };

    //map<string, Command*> commands;
    typedef string(*Cmd)(vector<string>);
    map<string, Cmd> commands;
}

#endif //DISCORDBOT_CREEPER_H