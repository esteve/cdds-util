#include "cdds/cdds_util.h"

static bool z_sertopic_equal (const struct ddsi_sertopic *acmn, const struct ddsi_sertopic *bcmn)
{
  // no fields in stp beyond the common ones, and those are all checked for equality before this function is called
  (void) acmn; (void) bcmn;
  return true;
}

static uint32_t z_sertopic_hash (const struct ddsi_sertopic *tpcmn)
{
  // nothing beyond the common fields
  (void) tpcmn;
  return 0;
}


static void z_sertopic_free(struct ddsi_sertopic * tpcmn) {
  ddsi_sertopic_fini(tpcmn);
}


static void z_sertopic_zero_samples(const struct ddsi_sertopic * d, void * samples, size_t count) {
  (void)d;
  (void)samples;
  (void)count;
  /* Not using code paths that rely on the samples getting zero'd out */
}


static void z_sertopic_realloc_samples(
  void ** ptrs, const struct ddsi_sertopic * d,
  void * old, size_t oldcount, size_t count)
{
  (void)(ptrs);
  (void)(d);
  (void)(old);
  (void)(oldcount);
  (void)(count);
  /* Not using code paths that rely on this (loans, dispose, unregister with instance handle,
     content filters) */
  abort();
}

static void z_sertopic_free_samples(
  const struct ddsi_sertopic * d, void ** ptrs, size_t count,
  dds_free_op_t op)
{
  (void)(d);    // unused
  (void)(ptrs);    // unused
  (void)(count);    // unused
  /* Not using code paths that rely on this (dispose, unregister with instance handle, content
     filters) */
  assert(!(op & DDS_FREE_ALL_BIT));
  (void) op;
}

static const struct ddsi_sertopic_ops z_sertopic_ops = {
  .free = z_sertopic_free,
  .zero_samples = z_sertopic_zero_samples,
  .realloc_samples = z_sertopic_realloc_samples,
  .free_samples = z_sertopic_free_samples,
  .equal = z_sertopic_equal,
  .hash = z_sertopic_hash
};

static bool z_serdata_eqkey(const struct ddsi_serdata * a, const struct ddsi_serdata * b)
{
  (void)(a);
  (void)(b);
  /* ROS 2 doesn't do keys in a meaningful way yet */
  printf("Called <z_serdata_eqkey>\n");
  return true;
}

static uint32_t z_serdata_size(const struct ddsi_serdata * sd)
{
  printf("Called <z_serdata_size>\n");
  struct z_ddsi_payload * zp = (struct z_ddsi_payload *)sd;
  return zp->size;
}

static void z_serdata_free(struct ddsi_serdata * sd)
{
  printf("Called <z_serdata_free>\n");
  struct z_ddsi_payload * zp = (struct z_ddsi_payload *)sd;
  free(zp->payload);
  zp->size = 0;
  // TODO: verify that dds_fini does not need to be called on zp->sd
  free(zp);
}

static struct ddsi_serdata *z_serdata_from_ser_iov (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  printf("==> <z_serdata_from_ser_iov> for %s -- size %zu\n", tpcmn->name, size);
  struct z_ddsi_payload *zp = (struct z_ddsi_payload *)malloc(sizeof(struct z_ddsi_payload));
  ddsi_serdata_init(&zp->sd, tpcmn, kind);  
  zp->size = size;
  zp->kind = kind;
  zp->payload = 0;
  switch (kind) {
    case SDK_KEY:
    case SDK_DATA:
      zp->payload = malloc(size);
      int offset = 0;
      int csize = 0;
      for (int i = 0; i < niov; ++i) {
        csize += iov[i].iov_len;
        assert(csize <= size);
        memcpy(zp->payload + offset, iov[i].iov_base, iov[i].iov_len);
        offset += iov[i].iov_len;
      }
      break;
    case SDK_EMPTY:
      break;
  }
  return (struct ddsi_serdata *)zp;
}

static struct ddsi_serdata *z_serdata_from_ser (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  printf("Called <z_serdata_from_ser> for %s\n", tpcmn->name);
  // This currently assumes that there is only one fragment.
  assert (fragchain->nextfrag == NULL);
  ddsrt_iovec_t iov = {
    .iov_base = NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain)),
    .iov_len = fragchain->maxp1 // fragchain->min = 0 for first fragment, by definition
  };
  return z_serdata_from_ser_iov (tpcmn, kind, 1, &iov, size);
}

static struct ddsi_serdata *z_serdata_to_topicless (const struct ddsi_serdata *sd) {
  return ddsi_serdata_ref(sd);
}

static const struct ddsi_serdata_ops z_serdata_ops = {
  .get_size = z_serdata_size,
  .eqkey = z_serdata_eqkey,
  .from_ser = z_serdata_from_ser,
  .from_ser_iov = z_serdata_from_ser_iov,
  .to_topicless = z_serdata_to_topicless
};


dds_entity_t z_create_blob_topic(dds_entity_t dp, char *topic_name, char* type_name, bool is_keyless) {
  struct ddsi_sertopic *st = (struct ddsi_sertopic*) malloc(sizeof(struct ddsi_sertopic));
  ddsi_sertopic_init (st, topic_name, type_name, &z_sertopic_ops, &z_serdata_ops, is_keyless);
  return dds_create_topic_generic (dp, &st, NULL, NULL, NULL);
}

int z_take_blob(dds_entity_t rd, struct z_ddsi_payload** sample, dds_sample_info_t * si) {
  struct ddsi_serdata **sd = (struct ddsi_serdata **)sample;
  return dds_takecdr(rd, sd, 1, si, DDS_ANY_STATE);
}
