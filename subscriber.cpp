#include "main.h"
#include "json.hpp"
#include "STUNExternalIP.h"
#include "peerconnection.h"

static bool bLoop = true;

static std::string machine;
static PeerConnection *pc = NULL;

const struct STUNServer ourSTUN = { 
    configSTUNSERVER, 
    configSTUNPORT
};

static std::string randomId(size_t length) {
	using std::chrono::high_resolution_clock;
	static thread_local std::mt19937 rng(
	    static_cast<unsigned int>(high_resolution_clock::now().time_since_epoch().count()));
	static const std::string characters(
	    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	std::string id(length, '0');
	std::uniform_int_distribution<int> uniform(0, int(characters.size() - 1));
	std::generate(id.begin(), id.end(), [&]() { return characters.at(uniform(rng)); });
	return id;
}

static void sigProc(int sig) {
    bLoop = false;
    LOGP("[CAUGHT] Signal %d\n", sig);
    exit(EXIT_SUCCESS);
}

static void onSignalingMessage(struct mosquitto *mosq, void *arg, const struct mosquitto_message *message) {
    nlohmann::json msgJs = nlohmann::json::parse((char *)message->payload);

    try {
        int port = msgJs["Port"].get<int>();
        std::string name = msgJs["Name"].get<std::string>();
        std::string PubIP = msgJs["PublicIP"].get<std::string>();
        std::string PrvIP = msgJs["PrivateIP"].get<std::string>();

        std::cout << msgJs.dump(4) << std::endl;

        if (name == machine) {
            Candidate remote;
            remote.fPort = port;
            remote.fPublicIP = PubIP;
            remote.fPrivateIP = PrvIP;

            if (pc) {
                pc->selectedCaindidatePair(&remote);
            }
        }
    }
    catch(const std::exception& e) {
        LOGE("%s\n", e.what());
    }
}

int main() {
    signal(SIGINT, 	sigProc);
	signal(SIGQUIT, sigProc);

    /* ===========================// CONNECT TO SIGNALING SERVER \\=========================== */
    struct mosquitto *mosq = NULL;

    mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        exit(EXIT_FAILURE);
    }
    mosquitto_message_callback_set(mosq, onSignalingMessage);
    int rc = mosquitto_connect(mosq, configSIGNALINGSERVER, configSIGNALINGPORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOGP("Can't connect to %s:%d\n", configSIGNALINGSERVER, configSIGNALINGPORT);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        exit(EXIT_FAILURE);
    }
    mosquitto_subscribe(mosq, NULL, configSIGNALING_TOPIC_RES, 0);
    mosquitto_loop_start(mosq);
    LOGP("[CONNECTED] Broker at %s:%d\n", configSIGNALINGSERVER, configSIGNALINGPORT);

    machine = randomId(5);
    pc = new PeerConnection();
    Candidate candidate = pc->gatherLocalCandidates();
    
    const nlohmann::json msgJs = {
        {"Name"         , machine               },
        {"Port"         , candidate.fPort       },
        {"PublicIP"     , candidate.fPublicIP   },
        {"PrivateIP"    , candidate.fPrivateIP  },
    };
    mosquitto_publish(mosq, NULL, configSIGNALING_TOPIC_REQ, msgJs.dump().length(), msgJs.dump().c_str(), 0, 0);

    while (bLoop) {

        if (pc->gatheringState() == COMPLTED) {
            char *ping = (char*)"PINGREQ";
            pc->sendTo((uint8_t*)ping, strlen(ping));

            char chars[32];
            memset(chars, 0, sizeof(chars));
            if (pc->readFrom((uint8_t*)chars, sizeof(chars)) > 0) {
                printf("Pong message \"%s\" \r\n", chars);
            }
        }
        
        sleep(1);
    }

    mosquitto_loop_stop(mosq, false);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}
