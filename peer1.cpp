#include "app.h"
#include "json.hpp"
#include "STUNExternalIP.h"

#define configNAME              (char *)"peer1"
#define configPORT              50001
#define configSUBSCRIBETOPIC    (char *)"udpstunserver_20210415"
#define configPUBLISHTOPIC      (char *)"udpstunserver_20210415"

static bool bLoop = true;

static void sigProc(int sig) {
    bLoop = false;
}

static void onSignalingMessage(struct mosquitto *mosq, void *arg, const struct mosquitto_message *message) {
    printf("Received message: %s\n", (char *)message->payload);
}

int main() {
    /* -- Connect to signaling server -- */
    struct mosquitto *mosq = NULL;

    mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        return 1;
    }
    mosquitto_message_callback_set(mosq, onSignalingMessage);
    int rc = mosquitto_connect(mosq, configSIGNALINGSERVER, configSIGNALINGPORT, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        printf("Can't connect to %s:%d\n", configSIGNALINGSERVER, configSIGNALINGPORT);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }
    mosquitto_subscribe(mosq, NULL, configSUBSCRIBETOPIC, 0);
    mosquitto_loop_start(mosq);
    printf("Connected to broker at %s:%d\n", configSIGNALINGSERVER, configSIGNALINGPORT);
    

    /* -- Create UDP Socket -- */

    while (bLoop) {
        
        sleep(1);
    }

    mosquitto_loop_stop(mosq, false);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}
