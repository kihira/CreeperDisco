#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"
#include "curlpp/Infos.hpp"
#include "lib/json/src/json.hpp"
#include "creeper.h"
#include <boost/log/trivial.hpp>

namespace discord {
    using namespace std;
    using namespace curlpp::Options;
    using json = nlohmann::json;

    typedef uint64_t snowflake;

    const string APP_VERSION = "0.0.1";
    const string API_URL = "https://discordapp.com/api/";
    mutex send_mutex;
    string gateway_url = "";
    string token = "";
    map<snowflake, creeper::Server*> discordCHMap;
    map<snowflake, creeper::Server*> chanServerMap; // todo more use of pointers? Store guild ID only once on Server then have things point to it?

    int last_seq = 0;

    void split(const string input, string delim, vector<string>& output) {
        string part;
        size_t pos = 0, start = 0;
        while ((pos = input.find(delim, start)) != string::npos) {
            part = input.substr(start, pos - start);
            if (part.length() == 0) continue;
            output.push_back(part);
            start = pos + 1;
        }
        output.push_back(input.substr(start));
    }

    template<typename C>
    string dump_vector(const vector<C>& input) {
        if (input.size() == 0) return "";

        stringstream output;
        for (int i = 0; i < input.size(); ++i) {
            output << input[i];
            if (i != input.size()-1) output << ",";
        }
        return output.str();
    }

    inline snowflake sf(string in) {
        return stoull(in);
    }

    json call(string callpoint, json data = {}) {
        stringstream outstream;

        curlpp::Cleanup cleanup; // Clean up used resources
        curlpp::Easy request;

        request.setOpt<WriteStream>(&outstream);
        request.setOpt<Url>(API_URL + callpoint);
        request.setOpt<UserAgent>("DiscordBot (http://github.com/kihira/CreeperDisco, "+APP_VERSION+")");
        request.setOpt<HttpHeader>({"Authorization: Bot "+token, "Content-Type: application/json"});
        if (!data.empty()) {
            request.setOpt<Post>(true);
            request.setOpt<PostFields>(data.dump());
            request.setOpt<PostFieldSize>(data.dump().length());
        }

        request.perform();

        stringstream headers;
        string output = outstream.str();
        size_t body_start = output.find("{"); // todo some endpoints return starting with [ if getting an array of something
        string body = output.substr(body_start);
        headers << output.substr(0, body_start);
        while (!headers.eof()) {
            string header;
            getline(headers, header);
            if (header.find("X-RateLimit") != string::npos) cout << header << endl;
        }

        long response = curlpp::infos::ResponseCode::get(request);
        if (response == 429l) {
            // todo rate limiting
        }

        try {
            json returned = json::parse(body);
            return returned;
        }
        catch(invalid_argument e) {
            cerr << "Failed to parse return from Discord" << endl
                    << "Headers:" << endl << headers.str() << endl
                    << "Body:" << endl << body << endl;
        }
        return {};
    }

    // todo pass server in instead and use it for logging
    void run_cmd(creeper::KeySecretPair& login, string cmd, vector<string> args, string chan_id) {
        chrono::time_point<chrono::system_clock> start = chrono::system_clock::now();

        json send;
        try {
            send["content"] = creeper::commands[cmd](login, args);
        }
        catch (creeper::CreeperException e) {
            send["content"] = e.what(); // todo give enduser a nicer error message and log this one internally
        }
        call("channels/"+chan_id+"/messages", send);

        BOOST_LOG_TRIVIAL(info) << "Command " << cmd << "(" << dump_vector(args) << ") took "
             << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start).count()
             << " milliseconds" << endl;
    }

    // todo should there be one Client per gateway connection?
    // websocketpp can handle multiple connections so could have one Client manage them all but would be a little messy
    class Client {
        typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
        typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;

    public:
        Client(boost::asio::io_service& service) : asio_service(service), heartbeat_timer(asio_service) {
            if (gateway_url.length() == 0) {
                // todo retrying on failed connection
                gateway_url = call("gateway")["url"];
                BOOST_LOG_TRIVIAL(info) << "Using gateway " << gateway_url << endl;
            }

            endpoint.set_access_channels(websocketpp::log::alevel::all);
            endpoint.set_error_channels(websocketpp::log::elevel::all);

            // Initialize ASIO
            endpoint.init_asio(&asio_service);

            // Handlers
            endpoint.set_tls_init_handler([](websocketpp::connection_hdl){
                return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
            });
            endpoint.set_open_handler(bind(&Client::on_open, this, placeholders::_1));
            endpoint.set_close_handler(bind(&Client::on_close, this, placeholders::_1));
            endpoint.set_message_handler(bind(&Client::on_message, this, placeholders::_1, placeholders::_2));

            websocketpp::lib::error_code ec;
            client::connection_ptr con = endpoint.get_connection(gateway_url, ec);
            if (ec) {
                cerr << "Failed to create connection" << endl
                     << "Reason: " << ec.message() << endl;
                return;
            }

            endpoint.connect(con);
        }
        void disconnect() {
            heartbeat_timer.cancel();
            endpoint.close(conn_hdl, websocketpp::close::status::normal, "Shutting Down");
        }
        void on_open(websocketpp::connection_hdl hdl) {
            json identify = {
                    {"op", 2},
                    {"d", {
                                   {"v", 5},
                                   {"token", token},
                                   {"properties", {
                                                          {"$os", "linux"},
                                                          {"$browser", ""},
                                                          {"$device", ""},
                                                          {"$referrer", ""},
                                                          {"$referring_domain", ""}
                                                  }},
                                   {"compress", "false"},
                                   {"large_threshold", 250}
                           }}
            };
            send_mutex.lock();
            endpoint.send(hdl, identify.dump(), websocketpp::frame::opcode::text);
            send_mutex.unlock();
        }
        void on_close(websocketpp::connection_hdl hdl) {
            client::connection_ptr con = endpoint.get_con_from_hdl(hdl);
            cerr << "Connection closed!" << endl
                 << "Code: " << con->get_remote_close_code() << endl
                 << "Reason: " << con->get_remote_close_reason() << endl;
            heartbeat_timer.cancel();
        }
        void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
            json payload = json::parse(msg->get_payload());
            BOOST_LOG_TRIVIAL(info) << "Message!" << endl
                 << "Handler: " << hdl.lock().get() << endl
                 << "Message: " << payload.dump(3) << endl;

            // Set last seq before doing anything
            last_seq = payload["s"];
            json data = payload["d"];
            switch (payload["op"].get<int>()) {
                // Event
                case 0:
                    // READY
                    if (payload["t"] == "READY") {
                        heartbeat(hdl, data["heartbeat_interval"]);

                        BOOST_LOG_TRIVIAL(info) << "Connecting to Discord Gateway" << endl
                             << "   Guilds: " << data["guilds"] << endl
                             << "   DMs: " << data["private_channels"] << endl;
                    }
                    if (payload["t"] == "GUILD_CREATE") {
                        BOOST_LOG_TRIVIAL(info) << "Joined Guild " << data["name"] << " (" << data["id"] << endl
                             << "    Owner: " << data["owner_id"] << endl
                             << "    Members: " << data["member_count"] << endl
                             << "    Large: " << data["large"] << endl;
                        if (data["channels"].size() > 0) {
                            // load channels to map
                            creeper::Server* srv = discordCHMap[sf(data["id"])];
                            for_each(data["channels"].begin(), data["channels"].end(), [data](json& value) { discord::chanServerMap[sf(value["id"])] = discordCHMap[sf(data["id"])];});
                        }
                    }
                    if (payload["t"] == "MESSAGE_CREATE") {
                        vector<string> parts;
                        split(data["content"], " ", parts);
                        if (parts.size() > 0 && creeper::commands.find(parts[0]) != creeper::commands.end()) {
                            creeper::KeySecretPair& login = chanServerMap[sf(data["channel_id"])]->login;
                            asio_service.post(bind(&run_cmd, login, parts[0], vector<string>(), data["channel_id"]));
                        }
                    }
                    break;
                    // Invalid Session
                case 9:
                    // todo handle. This is only sent after sending a resume (opcode 7) to gateway with invalid data
                    cerr << "Recieved Invalid Session ID opcode" << endl;
                    // HELLO
                case 10:
                    // This has never been sent in my time of testing but adding it here in case it finally happens
                    BOOST_LOG_TRIVIAL(info) << "Received HELLO" << endl;
                    // Heartbeat ACK
                case 11:
                    BOOST_LOG_TRIVIAL(info) << "Heartbeat ACK'd" << endl;
                    break;
                default:
                    cerr << "Unknown opcode! Data:" + payload.dump(1) << endl;
            }
        }
    private:
        boost::asio::io_service& asio_service;
        boost::asio::steady_timer heartbeat_timer;
        client endpoint;
        websocketpp::connection_hdl conn_hdl;

        void heartbeat(websocketpp::connection_hdl hdl, int interval) {
            json message = {
                    {"op", 1},
                    {"d", last_seq}
            };
            BOOST_LOG_TRIVIAL(info) << "Sending heartbeat. Last Seq: " << to_string(last_seq) << endl;
            send_mutex.lock();
            endpoint.send(hdl, message.dump(), websocketpp::frame::opcode::text);
            send_mutex.unlock();

            heartbeat_timer.expires_from_now(chrono::milliseconds(interval));
            heartbeat_timer.async_wait(bind(&Client::heartbeat, this, hdl, interval));
        }
    };
}

