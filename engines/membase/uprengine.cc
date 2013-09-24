#include "config.h"
#include <memcached/engine.h>
#include <memcached/types.h>
#include "membase.h"
#include "uprengine.h"

#include <string>
#include <map>


#if 0

class Dialog {
    uint32_t dialogid;
};

typedef std::map<uint32_t, Dialog*> DialogMap;
typedef std::map<uint32_t, Dialog*>::iterator DialogMapIterator;


class Connection {
    // This is the link of the current ongoing dialogs
    DialogMap dialogs;

    // TO make it simpler to locate the pending requests we've got from
    // ebm we should stash them int its own list
    DialogMaps pendingRequest;
};

static void messageReceived(uint32_t dialogid, void *packet) {
    DialogMapIterator iter = dialogs.find(dialogid);
    if (iter == dialogs.end()) {
        // not found
    }

}
#endif


class Connection {
public:
    Connection(const std::string &nm,
               const void *c,
               uint32_t s,
               uint32_t f) : name(nm), cookie(c), seqno(s), flags(f)
    {

    }

    ENGINE_ERROR_CODE addStream(uint32_t opaque,
                                uint16_t vbucket,
                                uint32_t flags,
                                ENGINE_ERROR_CODE (*stream_req)(const void *cookie,
                                                                uint32_t opaque,
                                                                uint16_t vbucket,
                                                                uint32_t flags,
                                                                uint64_t start_seqno,
                                                                uint64_t end_seqno,
                                                                uint64_t vbucket_uuid,
                                                                uint64_t high_seqno)) {
        return ENGINE_NOT_MY_VBUCKET;
    }


    const std::string name;
    const void *cookie;
    uint32_t seqno;
    uint32_t flags;

    std::map<uint32_t, uint32_t> addStreamRequests;
};

typedef std::map<const void*, Connection*> ConnectionMap;

class UprEngine {
public:
    UprEngine(struct membase_engine *e) {

    }

    ConnectionMap connections;

    Connection *getConnection(const std::string &name) {
        ConnectionMap::iterator iterator;
        for (iterator = connections.begin(); iterator != connections.end(); ++iterator) {
            if (name.compare(iterator->second->name)) {
                return iterator->second;
            }
        }

        return NULL;
    }


    Connection *openConnection(const void *cookie, uint32_t seqno, uint32_t flags, void *name, uint16_t nname) {
        // @todo check that we don't have this connection already
        std::string nm((char*)name, (size_t)nname);
        Connection *ret = new Connection(nm, cookie, seqno, flags);
        Connection *old = getConnection(nm);
        if (old != NULL) {
            if (old->seqno > seqno) {
                delete ret;
                return NULL;
            }

            // Invalidate the old one

        }

        connections[cookie] = ret;
        return ret;
    }

    Connection *getConnection(const void *cookie) {
        ConnectionMap::iterator iter = connections.find(cookie);
        if (iter == connections.end()) {
            return NULL;
        } else {
            return iter->second;
        }
    }

    ENGINE_ERROR_CODE addStream(uint32_t opaque,
                                uint16_t vbucket,
                                uint32_t flags) {
        /* I'm already streaming this bucket */
        return ENGINE_KEY_EEXISTS;
    }

};

static inline UprEngine *getEngine(struct membase_engine *engine) {
    return reinterpret_cast<UprEngine*>(engine->upr_engine);
}

ENGINE_ERROR_CODE upr_handle_open(struct membase_engine *engine,
                                  const void *cookie,
                                  uint32_t opaque,
                                  uint32_t seqno,
                                  uint32_t flags,
                                  void *name,
                                  uint16_t nname)
{
    UprEngine *e = getEngine(engine);
    if (e->openConnection(cookie, seqno, flags, name, nname)) {
        return ENGINE_SUCCESS;
    }

    return ENGINE_KEY_EEXISTS;
}

ENGINE_ERROR_CODE upr_handle_add_stream(struct membase_engine *engine,
                                        const void* cookie,
                                        uint32_t opaque,
                                        uint16_t vbucket,
                                        uint32_t flags,
                                        ENGINE_ERROR_CODE (*stream_req)(const void *cookie,
                                                                        uint32_t opaque,
                                                                        uint16_t vbucket,
                                                                        uint32_t flags,
                                                                        uint64_t start_seqno,
                                                                        uint64_t end_seqno,
                                                                        uint64_t vbucket_uuid,
                                                                        uint64_t high_seqno))

{
    UprEngine *e = getEngine(engine);
    Connection *conn = e->getConnection(cookie);
    if (conn == NULL) {
        return ENGINE_KEY_ENOENT;
    }

    return conn->addStream(opaque, vbucket, flags, stream_req);
}


void *create_upr_engine(struct membase_engine *me)
{
    return reinterpret_cast<void*>(new UprEngine(me));
}
