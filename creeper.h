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

    const string API_URL = "https://api.creeper.host/";

    typedef const pair<const string, const string> KeySecretPair;

    typedef string(*Cmd)(KeySecretPair&, vector<string>);
    map<string, Cmd> commands;

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

    class Server {
    public:
        Server(const string name, const string key, const string secret) : name(name), login(key, secret) {
        }
        KeySecretPair login;
    private:
        const string name;
    };

    list<Server*> servers;

    inline json call(KeySecretPair& login, string endpoint, json data = {}, function<void(json)> callback = NULL) {
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

        for (list<Server*>::const_iterator iterator = servers.begin(); iterator != servers.end(); iterator++) {
            json data = call((*iterator)->login, "api/alerts"); // todo use Multi interface?
            if (!data.empty()) {
                for (int i = 0; i < data["alerts"].size(); ++i) {
                    BOOST_LOG_TRIVIAL(info) << ((json)data["alerts"][i])["notes"].get<string>() << endl;
                }
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
}

#endif //DISCORDBOT_CREEPER_H