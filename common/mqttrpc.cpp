#include "mqttrpc.h"
#include "utils.h"
#include "mqtt_wrapper.h"

TMQTTRPCServer::TMQTTRPCServer(PMQTTClientBase mqtt_client, const string& driver_id)
    : MQTTClient(mqtt_client)
    , DriverId(driver_id)

{

};

void TMQTTRPCServer::OnMessage(const struct mosquitto_message *message)
{
    static Json::Reader reader;

    if (TopicMatchesSub("/rpc/v1/" + DriverId + "/+/+/+", message->topic)) {
        cout << "mqttrpcserver on message topic: " << message->topic << endl;
        const string topic = message->topic;
        const string payload = static_cast<const char*>(message->payload);
        const vector<string>& tokens = StringSplit(topic, '/');


        Json::Value request;

        bool parsedSuccess = reader.parse(payload, request, false);

        Json::Value reply;
        reply["error"] = Json::Value::null;
        reply["id"] = request["id"];

        if (parsedSuccess) {
            Json::Value params = request["params"];


            const auto& handler = MethodHandlers.find(make_pair(tokens[4], tokens[5]));

            try {
                Json::Value result = handler->second(params);

                reply["result"] = result;
            } catch (const std::exception& ex) {
                reply["error"] = Json::Value();
                reply["error"]["message"] = "Server error";
                reply["error"]["code"] = -32000;
                reply["error"]["data"] = ex.what();
            }
        } else {
                reply["error"] = Json::Value();
                reply["error"]["message"] = "Parse error";
                reply["error"]["code"] = -32700;
        }


        static Json::FastWriter writer;

        MQTTClient->Publish(NULL, topic + "/reply", writer.write(reply), 2, false);
    }
};


void TMQTTRPCServer::OnConnect(int rc)
{
    for (const auto &item : MethodHandlers) {
        const string topic = "/rpc/v1/" + DriverId + "/" + item.first.first + "/" + item.first.second;
        MQTTClient->Subscribe(NULL, topic + "/+");
        MQTTClient->Publish(NULL, topic, "1", 2, true);
    }
};

void TMQTTRPCServer::OnSubscribe(int mid, int qos_count, const int *granted_qos) {}


void TMQTTRPCServer::RegisterMethod(const string& service, const string& method, TMethodHandler handler)
{
    MethodHandlers[make_pair(service, method)] = handler;
}


void TMQTTRPCServer::Init()
{
    MQTTClient->Observe(shared_from_this());


};


