#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"
#include "curlpp/Infos.hpp"
#include "lib/json/src/json.hpp"

namespace discord {
    using namespace std;
    using namespace curlpp::Options;
    using json = nlohmann::json;

    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
    typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;

    // todo clean up into a class?
    const string APP_VERSION = "0.0.1";
    const string API_URL = "https://discordapp.com/api/";
    string gateway_url = "";
    string token = "";
    client endpoint;
    boost::asio::io_service asio_service;
    boost::asio::steady_timer heartbeat_timer(asio_service);

    int last_seq = 0;

    void heartbeat(client* c, websocketpp::connection_hdl hdl, int interval) {
        json message = {
                {"op", 1},
                {"d", last_seq}
        };
        cout << "Sending heartbeat. Last Seq: " + last_seq << endl;
        c->send(hdl, message.dump(), websocketpp::frame::opcode::text);

        heartbeat_timer.expires_from_now(chrono::milliseconds(interval));
        heartbeat_timer.async_wait(bind(&heartbeat, c, hdl, interval));
    }

    void on_message(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
        cout << "Message!" << endl
             << "Handler: " << hdl.lock().get()
             << "Message: " << msg->get_payload() << endl;

        json payload = json::parse(msg->get_payload());
        // Set last seq before doing anything
        last_seq = payload["s"];
        switch (payload["op"].get<int>()) {
            // Event
            case 0:
                // READY
                if (payload["t"] == "READY") {
                    heartbeat(c, hdl, payload.at("d").at("heartbeat_interval"));
                }
                break;
            // Invalid Session
            case 9:
                // todo handle. This is only sent after sending a resume (opcode 7) to gateway with invalid data
                cerr << "Recieved Invalid Session ID opcode" << endl;
            // HELLO
            case 10:
                // This has never been sent in my time of testing but adding it here in case it finally happens
                cout << "Received HELLO" << endl;
            // Heartbeat ACK
            case 11:
                cout << "Heartbeat ACK'd" << endl;
                break;
            default:
                cerr << "Unknown opcode! Data:" + payload.dump(1) << endl;
        }
    }

    void on_open(client* c, websocketpp::connection_hdl hdl) {
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
                               {"large_threshold", 250},
                               {"shard", {1, 10}}
                       }}
        };
        c->send(hdl, identify.dump(), websocketpp::frame::opcode::text);
    }

    void on_close(client* c, websocketpp::connection_hdl hdl) {
        client::connection_ptr con = c->get_con_from_hdl(hdl);
        cerr << "Connection closed!" << endl
             << "Code: " << con->get_remote_close_code() << endl
             << "Reason: " << con->get_remote_close_reason() << endl;
    }

    json call(string endpoint, json data = {}) {
        stringstream outstream;

        curlpp::Cleanup cleanup; // Clean up used resources
        curlpp::Easy request;

        request.setOpt<WriteStream>(&outstream);
        request.setOpt<Url>(API_URL + endpoint);
        request.setOpt<UserAgent>("DiscordBot (http://github.com/kihira/CreeperDisco, "+APP_VERSION+")");
        request.setOpt<HttpHeader>({"Authorization: Bot "+token});

        request.perform();

        //todo check for rate limiting
        // todo error checking

        json returned = json::parse(outstream.str());
        return returned;
    }

    void init() {
        if (gateway_url.length() == 0) {
            // todo retrying on failed connection
            gateway_url = call("gateway")["url"];
        }

        endpoint.set_access_channels(websocketpp::log::alevel::all);
        endpoint.set_error_channels(websocketpp::log::elevel::all);

        // Initialize ASIO
        endpoint.init_asio(&asio_service);

        // Handlers
        endpoint.set_message_handler(bind(&on_message,&endpoint,placeholders::_1,placeholders::_2));
        endpoint.set_tls_init_handler([](websocketpp::connection_hdl){
            return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
        });
        endpoint.set_open_handler(bind(&on_open,&endpoint,placeholders::_1));
        endpoint.set_close_handler(bind(&on_close,&endpoint,placeholders::_1));

        websocketpp::lib::error_code ec;
        client::connection_ptr con = endpoint.get_connection(gateway_url, ec);
        if (ec) {
            cerr << "Failed to create connection" << endl
                 << "Reason: " << ec.message() << endl;
            return;
        }

        endpoint.connect(con);
        endpoint.run(); // Run ASIO loop
    }
}

