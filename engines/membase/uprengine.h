#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    void *create_upr_engine(struct membase_engine *me);

    ENGINE_ERROR_CODE upr_handle_open(struct membase_engine *engine,
                                      const void *cookie,
                                      uint32_t opaque,
                                      uint32_t seqno,
                                      uint32_t flags,
                                      void *name,
                                      uint16_t nname);

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
                                                                            uint64_t high_seqno));

#ifdef __cplusplus
}
#endif
