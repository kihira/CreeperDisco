#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "curlpp/cURLpp.hpp"
#include "curlpp/Easy.hpp"
#include "curlpp/Options.hpp"
#include "curlpp/Infos.hpp"
#include "lib/json/src/json.hpp"

#define STDC_HEADERS 1

namespace discord {
    using namespace std;
    using namespace curlpp::Options;
    using json = nlohmann::json;

    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
    typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;

    const string APP_VERSION = "0.0.1";
    const string API_URL = "https://discordapp.com/api/";
    string gateway_url = "";
    string token = "";
    client m_endpoint;
    boost::asio::steady_timer heartbeat_timer;

    int last_seq = 0;

    void start(string uri) {
        websocketpp::lib::error_code ec;
        client::connection_ptr con = m_endpoint.get_connection(uri, ec);

        if (ec) {
            m_endpoint.get_alog().write(websocketpp::log::alevel::app,ec.message());
            return;
        }
        m_endpoint.connect(con);

        // Start the ASIO io_service run loop
        m_endpoint.run();
    }

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

    void on_fail(websocketpp::connection_hdl hdl) {
        client::connection_ptr con = m_endpoint.get_con_from_hdl(hdl);

        cout << "Fail handler" << endl;
        cout << con->get_state() << endl;
        cout << con->get_local_close_code() << endl;
        cout << con->get_local_close_reason() << endl;
        cout << con->get_remote_close_code() << endl;
        cout << con->get_remote_close_reason() << endl;
        cout << con->get_ec() << " - " << con->get_ec().message() << endl;
    }

    void on_message(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
        cout << "on_message called with hdl: " << hdl.lock().get()
             << " and message: " << msg->get_payload()
             << endl;

        json payload = json::parse(msg->get_payload());
        // Set last seq before doing anything
        last_seq = payload["s"];
        switch (payload["op"]) {
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

        //m_endpoint.close(hdl,websocketpp::close::status::going_away,"");
    }

    void on_open(client* c, websocketpp::connection_hdl hdl) {
        json identify = {
                {"op", 2},
                {"d", {
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

        m_endpoint.set_access_channels(websocketpp::log::alevel::all);
        m_endpoint.set_error_channels(websocketpp::log::elevel::all);

        // Initialize ASIO
        m_endpoint.init_asio();

        // Handlers
        m_endpoint.set_message_handler(bind(&on_message,&m_endpoint,placeholders::_1,placeholders::_2));
        m_endpoint.set_tls_init_handler([](websocketpp::connection_hdl){
            return websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
        });
        m_endpoint.set_open_handler(bind(&on_open,&m_endpoint,placeholders::_1));
        //m_endpoint.set_close_handler(bind(&d_client::on_close,this,::_1));
        //m_endpoint.set_fail_handler(bind(&on_fail,&m_endpoint,placeholders::_1));

        websocketpp::lib::error_code ec;
        client::connection_ptr con = m_endpoint.get_connection(gateway_url, ec);
        if (ec) {
            cout << "could not create connection because: " << ec.message() << endl;
            return;
        }

        m_endpoint.connect(con);
        m_endpoint.run();
    }
}

