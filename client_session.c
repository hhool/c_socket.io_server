#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "socket_io.h"
#include "endpoint.h"
#include "store.h"
#include "safe_mem.h"

void broadcast_clients(char *except_sessionid, char *message) {
    GList *list = get_store_list();
    GList *it = NULL;
    for (it = list; it; it = it->next) {
        char *sessionid = it->data;
        if (sessionid == NULL) {
            log_warn("the sessioin is NULL ****************");
            continue;
        }

        if (except_sessionid != NULL && strcmp(except_sessionid, sessionid) == 0) {
            continue;
        }

        send_msg(sessionid, message);
    }

    g_list_free(list);
}

void send_msg(char *sessionid, char *message) {
    session_t *session = store_lookup(sessionid);
    if (session == NULL) {
        log_warn("The sessionid %s has no value !", sessionid);
        return;
    }

    GQueue *queue = session->queue;
    if (queue == NULL) {
        log_warn("The queue is NULL !");
        return;
    }

    g_queue_push_head(queue, g_strdup(message));

    if (session->state != CONNECTED_STATE) {
        return;
    }

    client_t *client = session->client;
    if (client) {
        transports_fn *trans_fn = get_transport_fn(client);
        if (trans_fn) {
            trans_fn->output_callback(session);
        } else {
            log_warn("Got NO transport struct !");
        }
    }
}

void notice_connect(message_fields *msg_fields, char *sessionid, char *post_msg) {
    session_t *session = store_lookup(sessionid);
    if (!session) {
        log_warn("The sessionid %s has no value !", sessionid);
        return;
    }
    session->state = CONNECTED_STATE;
    session->endpoint = g_strdup(msg_fields->endpoint);

    send_msg(sessionid, post_msg);
}

void notice_disconnect(message_fields *msg_fields, char *sessionid) {
    session_t *session = store_lookup(sessionid);
    if (session == NULL) {
        log_warn("The sessionid %s has no value !", sessionid);
        return;
    }

    session->state = DISCONNECTED_STATE;
    client_t *client = session->client;
    if (client) {
        on_close(client);
        session->client = NULL;
    }

    store_remove(sessionid);
    free(session);
}